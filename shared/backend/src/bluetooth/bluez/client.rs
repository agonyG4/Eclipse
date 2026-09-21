use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::future::{Future, pending};
use std::pin::Pin;
use std::sync::Arc;
use std::time::{Duration, Instant};

use async_channel::{Receiver, Sender};
use futures::stream::{FuturesUnordered, SelectAll};
use futures::{FutureExt, StreamExt};
use zbus::message::Type;
use zbus::zvariant::{OwnedObjectPath, OwnedValue};
use zbus::{Connection, MatchRule, Message, MessageStream};

use crate::bluetooth::discovery::DiscoveryOperationKind;
use crate::bluetooth::engine::{BluetoothCore, CoreAction, CoreSnapshot};
use crate::bluetooth::object_store::{InterfaceMap, PropertyMap, PropertyValue};

use super::proxies::{
    Adapter1Proxy, BusDaemonProxy, Device1Proxy, ManagedObjects, ObjectManagerProxy,
    PropertiesProxy,
};

const COMMAND_CAPACITY: usize = 64;
const TASK_LIMIT: usize = 16;
const TASK_QUEUE_CAPACITY: usize = 64;
const SIGNAL_QUEUE_CAPACITY: usize = 32;
const DBUS_CALL_TIMEOUT: Duration = Duration::from_millis(3_000);

#[derive(Clone, Debug)]
enum WorkerCommand {
    SetPowered {
        session_generation: u64,
        target: bool,
    },
    Connect {
        session_generation: u64,
        object_path: String,
        connect: bool,
    },
}

#[derive(Clone, Copy, Debug)]
enum WorkerControl {
    LifecycleChanged,
    ScanOwnersChanged,
    Shutdown,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ConnectionState {
    Stopped,
    Disconnected,
    Connecting,
    Connected,
}

#[derive(Debug, Default)]
struct ReconnectBackoff {
    attempt: usize,
}

impl ReconnectBackoff {
    fn on_failure(&mut self, now: Instant) -> Instant {
        const DELAYS: [Duration; 4] = [
            Duration::from_millis(500),
            Duration::from_secs(1),
            Duration::from_secs(2),
            Duration::from_secs(5),
        ];
        let delay = DELAYS[self.attempt.min(DELAYS.len() - 1)];
        self.attempt = self.attempt.saturating_add(1);
        now + delay
    }

    fn on_success(&mut self) {
        self.attempt = 0;
    }
}

enum SignalOutcome {
    Message(Message),
    Error(String),
    End,
}

#[derive(Clone, Copy, Debug, Default)]
struct LifecycleRequest {
    session_generation: u64,
    running: bool,
}

#[derive(Clone, Default)]
struct ScanOwnerMailbox {
    session_generation: u64,
    owners: BTreeSet<String>,
}

struct WorkerChannels {
    commands: Receiver<WorkerCommand>,
    controls: Receiver<WorkerControl>,
    scan_controls: Receiver<WorkerControl>,
    shutdown: Receiver<WorkerControl>,
}

/// One long-lived worker and system-bus connection for one Bluetooth backend.
/// All channels are bounded; the GUI-facing methods only use `try_send`.
pub struct BluetoothWorker {
    commands: Sender<WorkerCommand>,
    controls: Sender<WorkerControl>,
    scan_controls: Sender<WorkerControl>,
    shutdown: Sender<WorkerControl>,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    scan_owners: Arc<std::sync::Mutex<ScanOwnerMailbox>>,
    _thread: Option<std::thread::JoinHandle<()>>,
}

impl BluetoothWorker {
    pub fn spawn<F>(on_snapshot: F) -> std::io::Result<Self>
    where
        F: Fn(CoreSnapshot) + Send + Sync + 'static,
    {
        let (commands, command_rx) = async_channel::bounded(COMMAND_CAPACITY);
        let (controls, control_rx) = async_channel::bounded(1);
        let (scan_controls, scan_control_rx) = async_channel::bounded(1);
        let (shutdown, shutdown_rx) = async_channel::bounded(1);
        let lifecycle = Arc::new(std::sync::Mutex::new(LifecycleRequest::default()));
        let scan_owners = Arc::new(std::sync::Mutex::new(ScanOwnerMailbox::default()));
        let callback = Arc::new(on_snapshot);
        let worker_lifecycle = Arc::clone(&lifecycle);
        let worker_scan_owners = Arc::clone(&scan_owners);
        let thread = std::thread::Builder::new()
            .name(String::from("astrea-bluetooth-bluez"))
            .spawn(move || {
                smol::block_on(run_worker(
                    WorkerChannels {
                        commands: command_rx,
                        controls: control_rx,
                        scan_controls: scan_control_rx,
                        shutdown: shutdown_rx,
                    },
                    worker_lifecycle,
                    worker_scan_owners,
                    callback,
                ));
            })?;
        Ok(Self {
            commands,
            controls,
            scan_controls,
            shutdown,
            lifecycle,
            scan_owners,
            _thread: Some(thread),
        })
    }

    pub fn start(&self, session_generation: u64) -> bool {
        self.request_lifecycle(session_generation, true)
    }

    pub fn stop(&self, session_generation: u64) {
        let _ = self.request_lifecycle(session_generation, false);
    }

    pub fn set_powered(&self, session_generation: u64, target: bool) -> bool {
        self.commands
            .try_send(WorkerCommand::SetPowered {
                session_generation,
                target,
            })
            .is_ok()
    }

    pub fn request_scan(&self, session_generation: u64, owner: String) -> bool {
        update_scan_owner_request(
            &self.scan_owners,
            &self.scan_controls,
            session_generation,
            owner,
        )
    }

    pub fn release_scan(&self, session_generation: u64, owner: String) {
        update_scan_owner_release(
            &self.scan_owners,
            &self.scan_controls,
            session_generation,
            owner,
        );
    }

    pub fn connect(&self, session_generation: u64, object_path: String, connect: bool) -> bool {
        self.commands
            .try_send(WorkerCommand::Connect {
                session_generation,
                object_path,
                connect,
            })
            .is_ok()
    }

    fn request_lifecycle(&self, session_generation: u64, running: bool) -> bool {
        let Ok(mut lifecycle) = self.lifecycle.lock() else {
            return false;
        };
        if session_generation < lifecycle.session_generation {
            return false;
        }
        *lifecycle = LifecycleRequest {
            session_generation,
            running,
        };
        if !running {
            let mut desired = self
                .scan_owners
                .lock()
                .unwrap_or_else(std::sync::PoisonError::into_inner);
            desired.session_generation = session_generation;
            desired.owners.clear();
        }
        match self.controls.try_send(WorkerControl::LifecycleChanged) {
            Ok(()) => true,
            Err(async_channel::TrySendError::Full(_)) => true,
            Err(async_channel::TrySendError::Closed(_)) => false,
        }
    }
}

fn update_scan_owner_request(
    scan_owners: &std::sync::Mutex<ScanOwnerMailbox>,
    scan_controls: &Sender<WorkerControl>,
    session_generation: u64,
    owner: String,
) -> bool {
    if owner.is_empty() {
        return false;
    }
    let mut desired = scan_owners
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    if session_generation < desired.session_generation {
        return false;
    }
    if session_generation > desired.session_generation {
        desired.session_generation = session_generation;
        desired.owners.clear();
    }
    desired.owners.insert(owner);
    drop(desired);
    match scan_controls.try_send(WorkerControl::ScanOwnersChanged) {
        Ok(()) | Err(async_channel::TrySendError::Full(_)) => true,
        Err(async_channel::TrySendError::Closed(_)) => false,
    }
}

fn update_scan_owner_release(
    scan_owners: &std::sync::Mutex<ScanOwnerMailbox>,
    scan_controls: &Sender<WorkerControl>,
    session_generation: u64,
    owner: String,
) {
    if owner.is_empty() {
        return;
    }
    let mut desired = scan_owners
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    if session_generation < desired.session_generation {
        return;
    }
    if session_generation > desired.session_generation {
        desired.session_generation = session_generation;
        desired.owners.clear();
    }
    desired.owners.remove(&owner);
    drop(desired);
    let _ = scan_controls.try_send(WorkerControl::ScanOwnersChanged);
}

impl Drop for BluetoothWorker {
    fn drop(&mut self) {
        let _ = self.shutdown.try_send(WorkerControl::Shutdown);
        if let Some(thread) = self._thread.take() {
            let _ = thread.join();
        }
    }
}

enum TaskResult {
    Probe {
        session_generation: u64,
        bluez_generation: u64,
        result: Result<BTreeMap<String, InterfaceMap>, String>,
    },
    GetAll {
        session_generation: u64,
        owner: String,
        token: crate::bluetooth::object_store::InterfaceRefreshToken,
        result: Result<PropertyMap, String>,
    },
    Operation {
        action: CoreAction,
        result: Result<(), String>,
    },
}

type TaskFuture = Pin<Box<dyn Future<Output = TaskResult> + Send>>;
type SignalStream = Pin<Box<dyn futures::Stream<Item = zbus::Result<Message>> + Send>>;

struct BusStreams {
    messages: SelectAll<SignalStream>,
}

impl BusStreams {
    async fn new(connection: &Connection) -> zbus::Result<Self> {
        let owner = make_match_rule(
            "org.freedesktop.DBus",
            "NameOwnerChanged",
            Some("org.freedesktop.DBus"),
            Some("/org/freedesktop/DBus"),
            Some("org.bluez"),
        )?;
        let object_added = make_match_rule(
            "org.freedesktop.DBus.ObjectManager",
            "InterfacesAdded",
            Some("org.bluez"),
            Some("/"),
            None,
        )?;
        let object_removed = make_match_rule(
            "org.freedesktop.DBus.ObjectManager",
            "InterfacesRemoved",
            Some("org.bluez"),
            Some("/"),
            None,
        )?;
        let properties = make_match_rule(
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            Some("org.bluez"),
            None,
            None,
        )?;

        let owner_stream = MessageStream::for_match_rule(owner, connection, Some(8)).await?;
        let added_stream =
            MessageStream::for_match_rule(object_added, connection, Some(SIGNAL_QUEUE_CAPACITY))
                .await?;
        let removed_stream =
            MessageStream::for_match_rule(object_removed, connection, Some(SIGNAL_QUEUE_CAPACITY))
                .await?;
        let properties_stream =
            MessageStream::for_match_rule(properties, connection, Some(SIGNAL_QUEUE_CAPACITY))
                .await?;
        let mut messages = SelectAll::new();
        messages.push(Box::pin(owner_stream) as SignalStream);
        messages.push(Box::pin(added_stream) as SignalStream);
        messages.push(Box::pin(removed_stream) as SignalStream);
        messages.push(Box::pin(properties_stream) as SignalStream);
        Ok(Self { messages })
    }
}

type TransportFuture<'a, T> = Pin<Box<dyn Future<Output = T> + Send + 'a>>;
type BusSessionHandle = Arc<dyn BusSession>;

trait BusTransport: Send + Sync {
    fn connect(&self) -> TransportFuture<'static, Result<BusSessionHandle, String>>;
}

trait BusSession: Send + Sync {
    fn owner(&self) -> TransportFuture<'static, Result<String, String>>;
    fn next_signal(&self) -> TransportFuture<'static, SignalOutcome>;
    fn execute(&self, action: CoreAction) -> TaskFuture;
}

struct ZbusTransport;

struct ZbusBusSession {
    connection: Arc<Connection>,
    streams: Arc<futures::lock::Mutex<BusStreams>>,
}

impl BusTransport for ZbusTransport {
    fn connect(&self) -> TransportFuture<'static, Result<BusSessionHandle, String>> {
        Box::pin(async {
            let connection =
                bounded_result(Connection::system(), "system D-Bus connection").await?;
            let streams =
                bounded_result(BusStreams::new(&connection), "BlueZ signal subscriptions").await?;
            Ok(Arc::new(ZbusBusSession {
                connection: Arc::new(connection),
                streams: Arc::new(futures::lock::Mutex::new(streams)),
            }) as BusSessionHandle)
        })
    }
}

impl BusSession for ZbusBusSession {
    fn owner(&self) -> TransportFuture<'static, Result<String, String>> {
        let connection = Arc::clone(&self.connection);
        Box::pin(async move {
            let proxy =
                bounded_result(BusDaemonProxy::new(&connection), "D-Bus daemon proxy").await?;
            bounded_result(proxy.get_name_owner("org.bluez"), "BlueZ owner lookup").await
        })
    }

    fn next_signal(&self) -> TransportFuture<'static, SignalOutcome> {
        let streams = Arc::clone(&self.streams);
        Box::pin(async move {
            let mut streams = streams.lock().await;
            match streams.messages.next().await {
                Some(Ok(message)) => SignalOutcome::Message(message),
                Some(Err(error)) => SignalOutcome::Error(error.to_string()),
                None => SignalOutcome::End,
            }
        })
    }

    fn execute(&self, action: CoreAction) -> TaskFuture {
        make_task(&self.connection, action)
    }
}

fn make_match_rule(
    interface: &str,
    member: &str,
    sender: Option<&str>,
    path: Option<&str>,
    arg0: Option<&str>,
) -> zbus::Result<zbus::OwnedMatchRule> {
    let mut builder = MatchRule::builder()
        .msg_type(Type::Signal)
        .interface(interface)?
        .member(member)?;
    if let Some(sender) = sender {
        builder = builder.sender(sender)?;
    }
    if let Some(path) = path {
        builder = builder.path(path)?;
    }
    if let Some(arg0) = arg0 {
        builder = builder.arg(0, arg0)?;
    }
    Ok(builder.build().into_owned().into())
}

async fn run_worker<F>(
    channels: WorkerChannels,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    scan_owners: Arc<std::sync::Mutex<ScanOwnerMailbox>>,
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    run_worker_with_transport(
        channels,
        lifecycle,
        scan_owners,
        Arc::new(ZbusTransport),
        on_snapshot,
    )
    .await;
}

type ConnectionResult = Result<(BusSessionHandle, Option<String>), String>;
type ConnectAttempt = Pin<Box<dyn Future<Output = ConnectionResult> + Send>>;
type ConnectSelectionFuture<'a> =
    Pin<Box<dyn Future<Output = Option<ConnectionResult>> + Send + 'a>>;

fn connect_attempt(transport: Arc<dyn BusTransport>) -> ConnectAttempt {
    Box::pin(async move {
        let session = transport.connect().await?;
        let owner = session.owner().await.ok();
        Ok((session, owner))
    })
}

fn scan_owner_request(scan_owners: &std::sync::Mutex<ScanOwnerMailbox>) -> ScanOwnerMailbox {
    scan_owners
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
        .clone()
}

fn best_effort_stop_discovery(session: Option<BusSessionHandle>, action: Option<CoreAction>) {
    let (Some(session), Some(action)) = (session, action) else {
        return;
    };
    smol::spawn(async move {
        let _ = session.execute(action).await;
    })
    .detach();
}

fn apply_scan_owner_update<F>(
    scan_owners: &std::sync::Mutex<ScanOwnerMailbox>,
    core: &mut BluetoothCore,
    queued_actions: &mut VecDeque<CoreAction>,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    let desired = scan_owner_request(scan_owners);
    if desired.session_generation != core.session_generation() {
        return;
    }
    let actions = core.replace_scan_owners(desired.owners, Instant::now());
    enqueue_actions(actions, queued_actions, core, on_snapshot);
    on_snapshot(core.snapshot().clone());
}

fn retire_generation_work(
    tasks: &mut FuturesUnordered<TaskFuture>,
    queued_actions: &mut VecDeque<CoreAction>,
) {
    *tasks = FuturesUnordered::new();
    queued_actions.clear();
}

async fn run_worker_with_transport<F>(
    channels: WorkerChannels,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    scan_owners: Arc<std::sync::Mutex<ScanOwnerMailbox>>,
    transport: Arc<dyn BusTransport>,
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    let WorkerChannels {
        commands,
        controls,
        scan_controls,
        shutdown,
    } = channels;
    let mut core = BluetoothCore::default();
    let mut state = ConnectionState::Stopped;
    let mut session = None::<BusSessionHandle>;
    let mut owner = String::new();
    let mut tasks = FuturesUnordered::<TaskFuture>::new();
    let mut queued_actions = VecDeque::<CoreAction>::new();
    let mut backoff = ReconnectBackoff::default();
    let mut reconnect_at = None::<Instant>;
    let mut connecting = None::<ConnectAttempt>;

    loop {
        let requested = lifecycle.lock().map(|state| *state).unwrap_or_default();
        if requested.running
            && state == ConnectionState::Disconnected
            && connecting.is_none()
            && reconnect_at.is_none()
        {
            reconnect_at = Some(Instant::now());
        }
        if requested.running
            && state == ConnectionState::Disconnected
            && connecting.is_none()
            && reconnect_at.is_some_and(|deadline| deadline <= Instant::now())
        {
            connecting = Some(connect_attempt(Arc::clone(&transport)));
            reconnect_at = None;
            state = ConnectionState::Connecting;
        }

        let mut lifecycle_changed = None;
        let mut scan_changed = false;
        let mut shutdown_requested = false;
        let mut connection_lost = None::<String>;
        let mut connection_result = None::<ConnectionResult>;
        let mut reconnect_timer_fired = false;
        let mut generation_replaced = false;
        {
            let task = if tasks.is_empty() {
                Box::pin(pending()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            } else {
                Box::pin(tasks.next()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            };
            let signal = session.as_ref().map_or_else(
                || Box::pin(pending()) as TransportFuture<'static, SignalOutcome>,
                |session| session.next_signal(),
            );
            let connect: ConnectSelectionFuture<'_> = match connecting.as_mut() {
                Some(connect) => Box::pin(connect.as_mut().map(Some)),
                None => Box::pin(pending()),
            };
            let timer = [core.next_deadline(), reconnect_at]
                .into_iter()
                .flatten()
                .min()
                .map_or_else(
                    || Box::pin(pending()) as TransportFuture<'static, Instant>,
                    |deadline| {
                        Box::pin(async_io::Timer::at(deadline)) as TransportFuture<'static, Instant>
                    },
                );

            futures::pin_mut!(task, signal, connect, timer);
            futures::select_biased! {
                shutdown_result = shutdown.recv().fuse() => {
                    if matches!(shutdown_result, Ok(WorkerControl::Shutdown)) {
                        shutdown_requested = true;
                    } else {
                        return;
                    }
                },
                control_result = controls.recv().fuse() => {
                    match control_result {
                        Ok(WorkerControl::LifecycleChanged) => {
                            lifecycle_changed = Some(lifecycle.lock().map(|state| *state).unwrap_or_default());
                        }
                        Ok(WorkerControl::Shutdown) => shutdown_requested = true,
                        Ok(WorkerControl::ScanOwnersChanged) => scan_changed = true,
                        Err(_) => return,
                    }
                },
                scan_result = scan_controls.recv().fuse() => {
                    match scan_result {
                        Ok(WorkerControl::ScanOwnersChanged) => scan_changed = true,
                        Ok(WorkerControl::Shutdown) => shutdown_requested = true,
                        Ok(WorkerControl::LifecycleChanged) => {},
                        Err(_) => return,
                    }
                },
                command_result = commands.recv().fuse() => {
                    match command_result {
                        Ok(command) => handle_command(command, &mut core, &mut queued_actions, on_snapshot.as_ref()).await,
                        Err(_) => return,
                    }
                },
                result = task.fuse() => {
                    if let Some(result) = result {
                        handle_task_result(result, &mut core, &mut owner, &mut queued_actions, on_snapshot.as_ref());
                    }
                },
                signal_result = signal.fuse() => {
                    match signal_result {
                        SignalOutcome::Message(message) => {
                            let previous_generation = core.bluez_generation();
                            handle_signal(message, &mut core, &mut owner, &mut queued_actions, on_snapshot.as_ref());
                            if core.bluez_generation() != previous_generation {
                                generation_replaced = true;
                            }
                        }
                        SignalOutcome::Error(error) => connection_lost = Some(error),
                        SignalOutcome::End => connection_lost = Some(String::from("BlueZ signal stream ended")),
                    }
                },
                connection = connect.fuse() => {
                    connection_result = connection;
                },
                _ = timer.fuse() => reconnect_timer_fired = true,
            }
        }

        if generation_replaced {
            retire_generation_work(&mut tasks, &mut queued_actions);
        }

        if shutdown_requested {
            best_effort_stop_discovery(session, core.stop_discovery_action());
            return;
        }

        if let Some(requested) = lifecycle_changed {
            if !requested.running && requested.session_generation >= core.session_generation() {
                best_effort_stop_discovery(session.take(), core.stop_discovery_action());
                connecting = None;
                reconnect_at = None;
                retire_generation_work(&mut tasks, &mut queued_actions);
                owner.clear();
                state = ConnectionState::Stopped;
                if core.stop_generation(requested.session_generation) {
                    on_snapshot(core.snapshot().clone());
                }
            } else if requested.running && requested.session_generation > core.session_generation()
            {
                best_effort_stop_discovery(session.take(), core.stop_discovery_action());
                connecting = None;
                reconnect_at = Some(Instant::now());
                retire_generation_work(&mut tasks, &mut queued_actions);
                owner.clear();
                state = ConnectionState::Disconnected;
                if core.start_generation(requested.session_generation) {
                    backoff.on_success();
                    on_snapshot(core.snapshot().clone());
                }
            }
        }

        if scan_changed {
            apply_scan_owner_update(
                &scan_owners,
                &mut core,
                &mut queued_actions,
                on_snapshot.as_ref(),
            );
        }

        if let Some(result) = connection_result {
            connecting = None;
            match result {
                Ok((new_session, new_owner)) => {
                    backoff.on_success();
                    owner = new_owner.clone().unwrap_or_default();
                    session = Some(new_session);
                    state = ConnectionState::Connected;
                    let actions = core.owner_changed(core.session_generation(), new_owner);
                    enqueue_actions(
                        actions,
                        &mut queued_actions,
                        &mut core,
                        on_snapshot.as_ref(),
                    );
                    on_snapshot(core.snapshot().clone());
                }
                Err(error) => {
                    state = ConnectionState::Disconnected;
                    reconnect_at = Some(backoff.on_failure(Instant::now()));
                    if core.bus_unavailable(core.session_generation(), error) {
                        on_snapshot(core.snapshot().clone());
                    }
                }
            }
        }

        if let Some(error) = connection_lost {
            session = None;
            owner.clear();
            state = ConnectionState::Disconnected;
            reconnect_at = Some(backoff.on_failure(Instant::now()));
            retire_generation_work(&mut tasks, &mut queued_actions);
            core.connection_lost(core.session_generation(), error);
            on_snapshot(core.snapshot().clone());
        }

        if reconnect_timer_fired {
            reconnect_at = None;
            let actions = core.on_timer(Instant::now());
            enqueue_actions(
                actions,
                &mut queued_actions,
                &mut core,
                on_snapshot.as_ref(),
            );
            on_snapshot(core.snapshot().clone());
        }

        while tasks.len() < TASK_LIMIT {
            let Some(action) = queued_actions.pop_front() else {
                break;
            };
            let Some(session) = session.as_ref() else {
                queued_actions.push_front(action);
                break;
            };
            tasks.push(session.execute(action));
        }
    }
}

async fn handle_command<F>(
    command: WorkerCommand,
    core: &mut BluetoothCore,
    queued: &mut VecDeque<CoreAction>,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    let now = Instant::now();
    match command {
        WorkerCommand::SetPowered {
            session_generation,
            target,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            if let Some(action) = core.set_powered(target, now) {
                enqueue_actions([action], queued, core, on_snapshot);
                on_snapshot(core.snapshot().clone());
            }
        }
        WorkerCommand::Connect {
            session_generation,
            object_path,
            connect,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.connect_device(&object_path, connect);
            enqueue_actions(action, queued, core, on_snapshot);
        }
    }
}

fn handle_task_result<F>(
    result: TaskResult,
    core: &mut BluetoothCore,
    _owner: &mut String,
    queued: &mut VecDeque<CoreAction>,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    match result {
        TaskResult::Probe {
            session_generation,
            bluez_generation,
            result,
        } => match result {
            Ok(objects) => {
                if let Some(actions) =
                    core.managed_objects(session_generation, bluez_generation, objects)
                {
                    on_snapshot(core.snapshot().clone());
                    enqueue_actions(actions, queued, core, on_snapshot);
                    enqueue_actions(core.on_timer(Instant::now()), queued, core, on_snapshot);
                }
            }
            Err(error) => {
                if core.operation_failed(session_generation, bluez_generation, Some(error)) {
                    on_snapshot(core.snapshot().clone());
                }
            }
        },
        TaskResult::GetAll {
            session_generation,
            owner: expected_owner,
            token,
            result,
        } => match result {
            Ok(properties) => {
                if core.refresh_reply(session_generation, &expected_owner, &token, properties) {
                    on_snapshot(core.snapshot().clone());
                }
            }
            Err(error) => {
                if core.operation_failed(session_generation, token.generation, Some(error)) {
                    on_snapshot(core.snapshot().clone());
                }
            }
        },
        TaskResult::Operation { action, result } => {
            let now = Instant::now();
            match action {
                CoreAction::SetPowered {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    ..
                } => {
                    let success = result.is_ok();
                    let error = result.err();
                    if core.power_reply(
                        session_generation,
                        bluez_generation,
                        operation_id,
                        success,
                        error,
                    ) {
                        on_snapshot(core.snapshot().clone());
                    }
                }
                CoreAction::Discovery {
                    session_generation,
                    bluez_generation,
                    operation,
                    ..
                } => {
                    let success = result.is_ok();
                    let error = result.err();
                    let (_, next) = core.discovery_reply(
                        session_generation,
                        bluez_generation,
                        operation.id,
                        success,
                        now,
                        error,
                    );
                    enqueue_actions(next, queued, core, on_snapshot);
                    on_snapshot(core.snapshot().clone());
                }
                CoreAction::Connect {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    object_path,
                    connect,
                    ..
                } => {
                    let success = result.is_ok();
                    let error = result.err();
                    if core.connect_reply(
                        session_generation,
                        bluez_generation,
                        operation_id,
                        &object_path,
                        connect,
                        success,
                        error,
                    ) {
                        on_snapshot(core.snapshot().clone());
                    }
                }
                CoreAction::Probe { .. }
                | CoreAction::StartBus { .. }
                | CoreAction::GetAll { .. } => {}
            }
        }
    }
}

fn handle_signal<F>(
    message: Message,
    core: &mut BluetoothCore,
    owner: &mut String,
    queued: &mut VecDeque<CoreAction>,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    let header = message.header();
    let interface = header
        .interface()
        .map(|value| value.as_str())
        .unwrap_or_default();
    let member = header
        .member()
        .map(|value| value.as_str())
        .unwrap_or_default();
    let now = Instant::now();
    if interface == "org.freedesktop.DBus" && member == "NameOwnerChanged" {
        let Ok((name, _old_owner, new_owner)) =
            message.body().deserialize::<(String, String, String)>()
        else {
            return;
        };
        if name != "org.bluez" {
            return;
        }
        let new_owner = if new_owner.is_empty() {
            None
        } else {
            Some(new_owner)
        };
        *owner = new_owner.clone().unwrap_or_default();
        let previous_generation = core.bluez_generation();
        let actions = core.owner_changed(core.session_generation(), new_owner);
        if core.bluez_generation() != previous_generation {
            queued.clear();
        }
        enqueue_actions(actions, queued, core, on_snapshot);
        on_snapshot(core.snapshot().clone());
        return;
    }

    let sender = header.sender().map(|value| value.to_string());
    if sender.as_deref() != Some(owner.as_str()) || owner.is_empty() {
        return;
    }
    let session_generation = core.session_generation();
    let bluez_generation = core.bluez_generation();
    match (interface, member) {
        ("org.freedesktop.DBus.ObjectManager", "InterfacesAdded") => {
            if let Ok((path, interfaces)) = message
                .body()
                .deserialize::<(OwnedObjectPath, ManagedInterfaces)>()
            {
                let interfaces = convert_interfaces(interfaces);
                if core.interfaces_added(
                    session_generation,
                    bluez_generation,
                    path.as_str(),
                    interfaces,
                ) {
                    enqueue_actions(core.on_timer(now), queued, core, on_snapshot);
                    on_snapshot(core.snapshot().clone());
                }
            }
        }
        ("org.freedesktop.DBus.ObjectManager", "InterfacesRemoved") => {
            if let Ok((path, interfaces)) = message
                .body()
                .deserialize::<(OwnedObjectPath, Vec<String>)>()
                && core.interfaces_removed(
                    session_generation,
                    bluez_generation,
                    path.as_str(),
                    &interfaces,
                )
            {
                enqueue_actions(core.on_timer(now), queued, core, on_snapshot);
                on_snapshot(core.snapshot().clone());
            }
        }
        ("org.freedesktop.DBus.Properties", "PropertiesChanged") => {
            if let Ok((interface_name, changed, invalidated)) =
                message
                    .body()
                    .deserialize::<(String, HashMapProperties, Vec<String>)>()
            {
                let Some(path) = header.path().map(|value| value.to_string()) else {
                    return;
                };
                let (accepted, refresh) = core.properties_changed(
                    session_generation,
                    bluez_generation,
                    &path,
                    &interface_name,
                    convert_properties(changed),
                    &invalidated,
                );
                if accepted {
                    enqueue_actions(refresh, queued, core, on_snapshot);
                    enqueue_actions(core.on_timer(now), queued, core, on_snapshot);
                    on_snapshot(core.snapshot().clone());
                }
            }
        }
        _ => {}
    }
}

type ManagedInterfaces = std::collections::HashMap<String, HashMapProperties>;
type HashMapProperties = std::collections::HashMap<String, OwnedValue>;

fn enqueue_actions<F>(
    actions: impl IntoIterator<Item = CoreAction>,
    queued: &mut VecDeque<CoreAction>,
    core: &mut BluetoothCore,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    for action in actions {
        if queued.len() < TASK_QUEUE_CAPACITY {
            queued.push_back(action);
        } else {
            fail_action(core, action, Instant::now());
            on_snapshot(core.snapshot().clone());
        }
    }
}

fn fail_action(core: &mut BluetoothCore, action: CoreAction, now: Instant) {
    match action {
        CoreAction::SetPowered {
            session_generation,
            bluez_generation,
            operation_id,
            ..
        } => {
            core.power_reply(
                session_generation,
                bluez_generation,
                operation_id,
                false,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::Discovery {
            session_generation,
            bluez_generation,
            operation,
            ..
        } => {
            core.discovery_reply(
                session_generation,
                bluez_generation,
                operation.id,
                false,
                now,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::Connect {
            session_generation,
            bluez_generation,
            ..
        } => {
            core.operation_failed(
                session_generation,
                bluez_generation,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::Probe {
            session_generation,
            bluez_generation,
            ..
        } => {
            core.operation_failed(
                session_generation,
                bluez_generation,
                Some(String::from(
                    "Bluetooth initial object snapshot could not be queued",
                )),
            );
        }
        CoreAction::GetAll {
            session_generation,
            token,
            ..
        } => {
            core.operation_failed(
                session_generation,
                token.generation,
                Some(String::from(
                    "Bluetooth property refresh could not be queued",
                )),
            );
        }
        CoreAction::StartBus { .. } => {}
    }
}

fn make_task(connection: &Connection, action: CoreAction) -> TaskFuture {
    let connection = connection.clone();
    Box::pin(async move {
        match action {
            CoreAction::Probe {
                session_generation,
                bluez_generation,
                owner,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = ObjectManagerProxy::builder(&connection)
                            .destination(owner.as_str())?
                            .build()
                            .await?;
                        let managed = proxy.get_managed_objects().await?;
                        Ok::<_, zbus::Error>(convert_managed_objects(managed))
                    },
                    "BlueZ object snapshot",
                )
                .await;
                TaskResult::Probe {
                    session_generation,
                    bluez_generation,
                    result,
                }
            }
            CoreAction::GetAll {
                session_generation,
                owner,
                token,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = PropertiesProxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(token.object_path.as_str())?
                            .build()
                            .await?;
                        let properties = proxy.get_all(token.interface_name.as_str()).await?;
                        Ok::<_, zbus::Error>(convert_properties(properties))
                    },
                    "BlueZ property refresh",
                )
                .await;
                TaskResult::GetAll {
                    session_generation,
                    owner,
                    token,
                    result,
                }
            }
            CoreAction::SetPowered {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                adapter_path,
                target,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Adapter1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(adapter_path.as_str())?
                            .build()
                            .await?;
                        proxy.set_powered(target).await
                    },
                    "BlueZ power operation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::SetPowered {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        adapter_path,
                        target,
                    },
                    result,
                }
            }
            CoreAction::Discovery {
                session_generation,
                bluez_generation,
                operation,
                owner,
                adapter_path,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Adapter1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(adapter_path.as_str())?
                            .build()
                            .await?;
                        match operation.kind {
                            DiscoveryOperationKind::Start => proxy.start_discovery().await,
                            DiscoveryOperationKind::Stop => proxy.stop_discovery().await,
                        }
                    },
                    "BlueZ discovery operation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::Discovery {
                        session_generation,
                        bluez_generation,
                        operation,
                        owner,
                        adapter_path,
                    },
                    result,
                }
            }
            CoreAction::Connect {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                object_path,
                connect,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Device1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(object_path.as_str())?
                            .build()
                            .await?;
                        if connect {
                            proxy.connect().await
                        } else {
                            proxy.disconnect().await
                        }
                    },
                    "BlueZ device operation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::Connect {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        object_path,
                        connect,
                    },
                    result,
                }
            }
            CoreAction::StartBus { session_generation } => {
                let _ = session_generation;
                TaskResult::Operation {
                    action: CoreAction::StartBus { session_generation },
                    result: Ok(()),
                }
            }
        }
    })
}

async fn bounded_result<T, E>(
    future: impl Future<Output = Result<T, E>>,
    operation: &'static str,
) -> Result<T, String>
where
    E: std::fmt::Display,
{
    futures::pin_mut!(future);
    let timeout = async_io::Timer::after(DBUS_CALL_TIMEOUT);
    futures::pin_mut!(timeout);
    match futures::future::select(future, timeout).await {
        futures::future::Either::Left((result, _)) => result.map_err(|error| error.to_string()),
        futures::future::Either::Right((_, _)) => Err(format!("{operation} timed out")),
    }
}

fn convert_managed_objects(managed: ManagedObjects) -> BTreeMap<String, InterfaceMap> {
    managed
        .into_iter()
        .map(|(path, interfaces)| (path.as_str().to_owned(), convert_interfaces(interfaces)))
        .collect()
}

fn convert_interfaces(interfaces: ManagedInterfaces) -> InterfaceMap {
    interfaces
        .into_iter()
        .map(|(name, properties)| (name, convert_properties(properties)))
        .collect()
}

fn convert_properties(properties: HashMapProperties) -> PropertyMap {
    properties
        .into_iter()
        .map(|(name, value)| (name, convert_property(value)))
        .collect()
}

fn convert_property(value: OwnedValue) -> PropertyValue {
    if let Ok(value) = bool::try_from(value.clone()) {
        return PropertyValue::Boolean(value);
    }
    if let Ok(value) = String::try_from(value.clone()) {
        return PropertyValue::String(value);
    }
    if let Ok(value) = i16::try_from(value.clone()) {
        return PropertyValue::Integer(i32::from(value));
    }
    if let Ok(value) = i32::try_from(value.clone()) {
        return PropertyValue::Integer(value);
    }
    if let Ok(value) = u8::try_from(value.clone()) {
        return PropertyValue::Integer(i32::from(value));
    }
    if let Ok(value) = u32::try_from(value)
        && let Ok(value) = i32::try_from(value)
    {
        return PropertyValue::Integer(value);
    }
    PropertyValue::Unsupported
}

#[cfg(test)]
mod worker_lifecycle_tests {
    use super::{
        BluetoothWorker, BusSession, BusSessionHandle, BusTransport, CoreAction, LifecycleRequest,
        ScanOwnerMailbox, TASK_LIMIT, TaskFuture, TaskResult, WorkerChannels, WorkerCommand,
        WorkerControl, apply_scan_owner_update, retire_generation_work, run_worker_with_transport,
        update_scan_owner_release, update_scan_owner_request,
    };
    use super::{ConnectionState, ReconnectBackoff, SignalOutcome};
    use crate::bluetooth::engine::{BluetoothCore, CoreSnapshot};
    use crate::bluetooth::object_store::{InterfaceMap, PropertyMap};
    use async_channel::{Receiver, Sender};
    use std::collections::VecDeque;
    use std::future::Future;
    use std::pin::Pin;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::{Arc, Mutex};
    use std::time::{Duration, Instant};

    #[test]
    fn reconnect_backoff_is_bounded_and_resets_after_success() {
        let now = Instant::now();
        let mut backoff = ReconnectBackoff::default();

        assert_eq!(backoff.on_failure(now), now + Duration::from_millis(500));
        assert_eq!(backoff.on_failure(now), now + Duration::from_secs(1));
        assert_eq!(backoff.on_failure(now), now + Duration::from_secs(2));
        assert_eq!(backoff.on_failure(now), now + Duration::from_secs(5));
        assert_eq!(backoff.on_failure(now), now + Duration::from_secs(5));

        backoff.on_success();
        assert_eq!(backoff.on_failure(now), now + Duration::from_millis(500));
    }

    #[test]
    fn signal_stream_outcomes_are_distinct() {
        assert!(matches!(SignalOutcome::End, SignalOutcome::End));
        assert_eq!(ConnectionState::Disconnected, ConnectionState::Disconnected);
    }

    enum FakeSignal {
        Pending,
        Error(String),
        End,
    }

    struct FakeSession {
        owner: Option<String>,
        signals: Arc<Mutex<VecDeque<FakeSignal>>>,
    }

    struct FakeTransport {
        plans: Arc<Mutex<VecDeque<Result<FakeSession, String>>>>,
        attempts: Arc<AtomicUsize>,
    }

    impl BusTransport for FakeTransport {
        fn connect(
            &self,
        ) -> Pin<Box<dyn Future<Output = Result<BusSessionHandle, String>> + Send + 'static>>
        {
            let plan = self
                .plans
                .lock()
                .expect("fake plans lock")
                .pop_front()
                .unwrap_or_else(|| Err(String::from("no fake connection plan")));
            let attempts = Arc::clone(&self.attempts);
            Box::pin(async move {
                attempts.fetch_add(1, Ordering::SeqCst);
                plan.map(|session| Arc::new(session) as BusSessionHandle)
            })
        }
    }

    impl BusSession for FakeSession {
        fn owner(&self) -> Pin<Box<dyn Future<Output = Result<String, String>> + Send + 'static>> {
            let owner = self.owner.clone();
            Box::pin(async move { owner.ok_or_else(|| String::from("BlueZ has no owner")) })
        }

        fn next_signal(&self) -> Pin<Box<dyn Future<Output = SignalOutcome> + Send + 'static>> {
            let signals = Arc::clone(&self.signals);
            Box::pin(async move {
                let signal = signals.lock().expect("fake signals lock").pop_front();
                match signal {
                    Some(FakeSignal::Error(error)) => SignalOutcome::Error(error),
                    Some(FakeSignal::End) => SignalOutcome::End,
                    Some(FakeSignal::Pending) | None => futures::future::pending().await,
                }
            })
        }

        fn execute(&self, action: CoreAction) -> TaskFuture {
            Box::pin(async move {
                match action {
                    CoreAction::Probe {
                        session_generation,
                        bluez_generation,
                        ..
                    } => TaskResult::Probe {
                        session_generation,
                        bluez_generation,
                        result: Ok(std::collections::BTreeMap::<String, InterfaceMap>::new()),
                    },
                    CoreAction::GetAll {
                        session_generation,
                        owner,
                        token,
                    } => TaskResult::GetAll {
                        session_generation,
                        owner,
                        token,
                        result: Ok(PropertyMap::new()),
                    },
                    action @ CoreAction::SetPowered { .. }
                    | action @ CoreAction::Discovery { .. }
                    | action @ CoreAction::Connect { .. }
                    | action @ CoreAction::StartBus { .. } => TaskResult::Operation {
                        action,
                        result: Ok(()),
                    },
                }
            })
        }
    }

    struct WorkerHarness {
        _commands: Sender<WorkerCommand>,
        lifecycle: Arc<Mutex<LifecycleRequest>>,
        controls: Sender<WorkerControl>,
        _scan_controls: Sender<WorkerControl>,
        shutdown: Sender<WorkerControl>,
        snapshots: Receiver<CoreSnapshot>,
        thread: Option<std::thread::JoinHandle<()>>,
    }

    impl WorkerHarness {
        fn new(plans: Vec<Result<FakeSession, String>>) -> (Self, Arc<AtomicUsize>) {
            let (commands, command_rx) = async_channel::bounded(64);
            let (controls, control_rx) = async_channel::bounded(1);
            let (scan_controls, scan_control_rx) = async_channel::bounded(1);
            let (shutdown, shutdown_rx) = async_channel::bounded(1);
            let lifecycle = Arc::new(Mutex::new(LifecycleRequest::default()));
            let scan_owners = Arc::new(Mutex::new(ScanOwnerMailbox::default()));
            let (snapshots, snapshot_rx) = async_channel::bounded(32);
            let attempts = Arc::new(AtomicUsize::new(0));
            let transport = Arc::new(FakeTransport {
                plans: Arc::new(Mutex::new(VecDeque::from(plans))),
                attempts: Arc::clone(&attempts),
            });
            let thread_lifecycle = Arc::clone(&lifecycle);
            let thread_scan_owners = Arc::clone(&scan_owners);
            let thread = std::thread::spawn(move || {
                smol::block_on(run_worker_with_transport(
                    WorkerChannels {
                        commands: command_rx,
                        controls: control_rx,
                        scan_controls: scan_control_rx,
                        shutdown: shutdown_rx,
                    },
                    thread_lifecycle,
                    thread_scan_owners,
                    transport,
                    Arc::new(move |snapshot| {
                        let _ = snapshots.try_send(snapshot);
                    }),
                ));
            });
            (
                Self {
                    _commands: commands,
                    lifecycle,
                    controls,
                    _scan_controls: scan_controls,
                    shutdown,
                    snapshots: snapshot_rx,
                    thread: Some(thread),
                },
                attempts,
            )
        }

        fn start(&self) {
            let mut lifecycle = self.lifecycle.lock().expect("lifecycle lock");
            lifecycle.session_generation += 1;
            lifecycle.running = true;
            self.controls
                .try_send(WorkerControl::LifecycleChanged)
                .expect("lifecycle wake");
        }

        fn wait_for(&self, predicate: impl Fn(&CoreSnapshot) -> bool) -> CoreSnapshot {
            smol::block_on(async {
                loop {
                    let snapshot = self.snapshots.recv().await.expect("snapshot");
                    if predicate(&snapshot) {
                        return snapshot;
                    }
                }
            })
        }

        fn shutdown(mut self) {
            self.shutdown
                .try_send(WorkerControl::Shutdown)
                .expect("shutdown wake");
            self.thread
                .take()
                .expect("worker thread")
                .join()
                .expect("worker join");
        }
    }

    fn pending_session(signals: Vec<FakeSignal>) -> FakeSession {
        FakeSession {
            owner: Some(String::from(":1.42")),
            signals: Arc::new(Mutex::new(VecDeque::from(signals))),
        }
    }

    #[test]
    fn saturated_commands_cannot_drop_the_final_scan_owner_release() {
        let (ordinary, _ordinary_rx) = async_channel::bounded(64);
        for index in 0..64 {
            ordinary
                .try_send(WorkerCommand::SetPowered {
                    session_generation: 1,
                    target: index % 2 == 0,
                })
                .expect("ordinary queue capacity");
        }
        let (scan_controls, _scan_rx) = async_channel::bounded(1);
        let scan_owners = Mutex::new(ScanOwnerMailbox::default());
        assert!(update_scan_owner_request(
            &scan_owners,
            &scan_controls,
            1,
            String::from("topbar"),
        ));
        assert!(update_scan_owner_request(
            &scan_owners,
            &scan_controls,
            1,
            String::from("bluetooth-popup"),
        ));

        let mut core = BluetoothCore::default();
        assert!(core.start_generation(1));
        apply_scan_owner_update(&scan_owners, &mut core, &mut VecDeque::new(), &|_| {});
        assert_eq!(core.scan_owners().len(), 2);

        update_scan_owner_release(&scan_owners, &scan_controls, 1, String::from("topbar"));
        update_scan_owner_release(
            &scan_owners,
            &scan_controls,
            1,
            String::from("bluetooth-popup"),
        );
        apply_scan_owner_update(&scan_owners, &mut core, &mut VecDeque::new(), &|_| {});
        assert!(core.scan_owners().is_empty());
    }

    #[test]
    fn generation_replacement_drops_old_tasks_before_new_probe() {
        let mut tasks = futures::stream::FuturesUnordered::<TaskFuture>::new();
        for _ in 0..TASK_LIMIT {
            tasks.push(Box::pin(futures::future::pending::<TaskResult>()));
        }
        let mut queued_actions = VecDeque::from([CoreAction::Probe {
            session_generation: 1,
            bluez_generation: 1,
            owner: String::from(":1.42"),
        }]);

        retire_generation_work(&mut tasks, &mut queued_actions);

        assert!(tasks.is_empty());
        assert!(queued_actions.is_empty());

        tasks.push(Box::pin(async {
            TaskResult::Probe {
                session_generation: 2,
                bluez_generation: 2,
                result: Ok(std::collections::BTreeMap::<String, InterfaceMap>::new()),
            }
        }));
        assert!(smol::block_on(async { futures::StreamExt::next(&mut tasks).await }).is_some());
    }

    #[test]
    fn initial_connection_failure_recovers_with_a_fresh_connection() {
        let (harness, attempts) = WorkerHarness::new(vec![
            Err(String::from("system bus unavailable")),
            Ok(pending_session(vec![FakeSignal::Pending])),
        ]);
        harness.start();
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        assert_eq!(attempts.load(Ordering::SeqCst), 2);
        harness.shutdown();
    }

    #[test]
    fn signal_error_and_terminal_end_each_trigger_recovery() {
        for signal in [
            FakeSignal::Error(String::from("signal error")),
            FakeSignal::End,
        ] {
            let (harness, attempts) = WorkerHarness::new(vec![
                Ok(pending_session(vec![signal])),
                Ok(pending_session(vec![FakeSignal::Pending])),
            ]);
            harness.start();
            harness.wait_for(|snapshot| {
                snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
            });
            harness.wait_for(|snapshot| {
                snapshot.state == crate::bluetooth::engine::ServiceState::Ready
            });
            assert_eq!(attempts.load(Ordering::SeqCst), 2);
            harness.shutdown();
        }
    }

    #[test]
    fn stopped_worker_never_attempts_connection_and_shutdown_cancels_backoff() {
        let (stopped, attempts) =
            WorkerHarness::new(vec![Ok(pending_session(vec![FakeSignal::Pending]))]);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(attempts.load(Ordering::SeqCst), 0);
        stopped.shutdown();

        let (running, attempts) = WorkerHarness::new(vec![Err(String::from("temporary failure"))]);
        running.start();
        running.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        let started = Instant::now();
        running.shutdown();
        assert!(started.elapsed() < Duration::from_millis(200));
        assert_eq!(attempts.load(Ordering::SeqCst), 1);
    }

    #[test]
    fn dropping_worker_wakes_and_joins_the_worker_thread() {
        let started = Instant::now();
        let worker = BluetoothWorker::spawn(|_| {}).expect("worker thread");
        drop(worker);
        assert!(started.elapsed() < Duration::from_millis(200));
    }
}
