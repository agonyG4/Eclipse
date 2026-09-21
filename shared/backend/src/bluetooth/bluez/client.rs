use std::collections::{BTreeMap, VecDeque};
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
    RequestScan {
        session_generation: u64,
        owner: String,
    },
    ReleaseScan {
        session_generation: u64,
        owner: String,
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
    Shutdown,
}

#[derive(Clone, Copy, Debug, Default)]
struct LifecycleRequest {
    session_generation: u64,
    running: bool,
}

struct WorkerChannels {
    commands: Receiver<WorkerCommand>,
    controls: Receiver<WorkerControl>,
    shutdown: Receiver<WorkerControl>,
}

/// One long-lived worker and system-bus connection for one Bluetooth backend.
/// All channels are bounded; the GUI-facing methods only use `try_send`.
pub struct BluetoothWorker {
    commands: Sender<WorkerCommand>,
    controls: Sender<WorkerControl>,
    shutdown: Sender<WorkerControl>,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    _thread: std::thread::JoinHandle<()>,
}

impl BluetoothWorker {
    pub fn spawn<F>(on_snapshot: F) -> std::io::Result<Self>
    where
        F: Fn(CoreSnapshot) + Send + Sync + 'static,
    {
        let (commands, command_rx) = async_channel::bounded(COMMAND_CAPACITY);
        let (controls, control_rx) = async_channel::bounded(1);
        let (shutdown, shutdown_rx) = async_channel::bounded(1);
        let lifecycle = Arc::new(std::sync::Mutex::new(LifecycleRequest::default()));
        let callback = Arc::new(on_snapshot);
        let worker_lifecycle = Arc::clone(&lifecycle);
        let thread = std::thread::Builder::new()
            .name(String::from("astrea-bluetooth-bluez"))
            .spawn(move || {
                smol::block_on(run_worker(
                    WorkerChannels {
                        commands: command_rx,
                        controls: control_rx,
                        shutdown: shutdown_rx,
                    },
                    worker_lifecycle,
                    callback,
                ));
            })?;
        Ok(Self {
            commands,
            controls,
            shutdown,
            lifecycle,
            _thread: thread,
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
        self.commands
            .try_send(WorkerCommand::RequestScan {
                session_generation,
                owner,
            })
            .is_ok()
    }

    pub fn release_scan(&self, session_generation: u64, owner: String) -> bool {
        self.commands
            .try_send(WorkerCommand::ReleaseScan {
                session_generation,
                owner,
            })
            .is_ok()
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
        match self.controls.try_send(WorkerControl::LifecycleChanged) {
            Ok(()) => true,
            Err(async_channel::TrySendError::Full(_)) => true,
            Err(async_channel::TrySendError::Closed(_)) => false,
        }
    }
}

impl Drop for BluetoothWorker {
    fn drop(&mut self) {
        let _ = self.shutdown.try_send(WorkerControl::Shutdown);
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
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    let connection = match bounded_result(Connection::system(), "system D-Bus connection").await {
        Ok(connection) => connection,
        Err(error) => {
            run_without_bus(channels, lifecycle, on_snapshot, error).await;
            return;
        }
    };
    let streams =
        match bounded_result(BusStreams::new(&connection), "BlueZ signal subscriptions").await {
            Ok(streams) => streams,
            Err(error) => {
                run_without_bus(channels, lifecycle, on_snapshot, error).await;
                return;
            }
        };
    let bus = match bounded_result(BusDaemonProxy::new(&connection), "D-Bus daemon proxy").await {
        Ok(proxy) => proxy,
        Err(error) => {
            run_without_bus(channels, lifecycle, on_snapshot, error).await;
            return;
        }
    };
    run_connected(channels, lifecycle, connection, bus, streams, on_snapshot).await;
}

async fn run_without_bus<F>(
    channels: WorkerChannels,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    on_snapshot: Arc<F>,
    reason: String,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    let WorkerChannels {
        commands,
        controls,
        shutdown,
    } = channels;
    let mut core = BluetoothCore::default();
    loop {
        futures::select_biased! {
            control = shutdown.recv().fuse() => {
                if matches!(control, Ok(WorkerControl::Shutdown)) { return; }
            },
            control = controls.recv().fuse() => {
                if matches!(control, Ok(WorkerControl::LifecycleChanged)) {
                    let requested = lifecycle.lock().map(|state| *state).unwrap_or_default();
                    if requested.running {
                        if core.start_generation(requested.session_generation) {
                            core.bus_unavailable(requested.session_generation, reason.clone());
                            on_snapshot(core.snapshot().clone());
                        }
                    } else if core.stop_generation(requested.session_generation) {
                        on_snapshot(core.snapshot().clone());
                    }
                }
            },
            control = commands.recv().fuse() => {
                match control {
                    Ok(_) => {}
                    Err(_) => return,
                }
            },
        }
    }
}

async fn run_connected<F>(
    channels: WorkerChannels,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    connection: Connection,
    bus: BusDaemonProxy<'_>,
    mut streams: BusStreams,
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    let WorkerChannels {
        commands,
        controls,
        shutdown,
    } = channels;
    let mut core = BluetoothCore::default();
    let mut tasks = FuturesUnordered::<TaskFuture>::new();
    let mut queued_actions = VecDeque::<CoreAction>::new();
    let mut owner = String::new();

    loop {
        while tasks.len() < TASK_LIMIT {
            let Some(action) = queued_actions.pop_front() else {
                break;
            };
            tasks.push(make_task(&connection, action));
        }
        let mut lifecycle_changed = None;
        let mut shutdown_requested = false;
        {
            let task = if tasks.is_empty() {
                Box::pin(pending()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            } else {
                Box::pin(tasks.next()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            };
            let timer = if let Some(deadline) = core.next_deadline() {
                Box::pin(async_io::Timer::at(deadline))
                    as Pin<Box<dyn Future<Output = Instant> + Send>>
            } else {
                Box::pin(pending()) as Pin<Box<dyn Future<Output = Instant> + Send>>
            };

            futures::pin_mut!(task, timer);
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
                signal = streams.messages.next().fuse() => {
                    if let Some(Ok(message)) = signal {
                        handle_signal(message, &mut core, &mut owner, &mut queued_actions, on_snapshot.as_ref());
                    }
                },
                _ = timer.fuse() => {
                    let actions = core.on_timer(Instant::now());
                    enqueue_actions(actions, &mut queued_actions, &mut core, on_snapshot.as_ref());
                    on_snapshot(core.snapshot().clone());
                },
            }
        }

        if shutdown_requested {
            drop(tasks);
            queued_actions.clear();
            if let Some(action) = core.stop_discovery_action() {
                let _ = make_task(&connection, action).await;
            }
            return;
        }

        if let Some(requested) = lifecycle_changed {
            let replaces_session = requested.session_generation > core.session_generation();
            let stops_session =
                !requested.running && requested.session_generation >= core.session_generation();
            if replaces_session || stops_session {
                tasks = FuturesUnordered::new();
                queued_actions.clear();
                if let Some(action) = core.stop_discovery_action() {
                    let _ = make_task(&connection, action).await;
                }
            }

            if requested.running {
                if core.start_generation(requested.session_generation) {
                    owner.clear();
                    on_snapshot(core.snapshot().clone());
                    match bounded_result(bus.get_name_owner("org.bluez"), "BlueZ owner lookup")
                        .await
                    {
                        Ok(unique_owner) => {
                            owner.clone_from(&unique_owner);
                            enqueue_actions(
                                core.owner_changed(
                                    requested.session_generation,
                                    Some(unique_owner),
                                ),
                                &mut queued_actions,
                                &mut core,
                                on_snapshot.as_ref(),
                            );
                        }
                        Err(_) => {
                            owner.clear();
                            core.owner_changed(requested.session_generation, None);
                            on_snapshot(core.snapshot().clone());
                        }
                    }
                }
            } else if core.stop_generation(requested.session_generation) {
                owner.clear();
                on_snapshot(core.snapshot().clone());
            }
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
        WorkerCommand::RequestScan {
            session_generation,
            owner,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.request_scan(&owner, now);
            enqueue_actions(action, queued, core, on_snapshot);
            on_snapshot(core.snapshot().clone());
        }
        WorkerCommand::ReleaseScan {
            session_generation,
            owner,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.release_scan(&owner, now);
            enqueue_actions(action, queued, core, on_snapshot);
            on_snapshot(core.snapshot().clone());
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
                    )
                    {
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
