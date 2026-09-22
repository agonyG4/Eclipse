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

use crate::bluetooth::agent::{
    AGENT_CAPABILITY, AGENT_OBJECT_PATH, Agent1, AgentBroker, AgentPromptView, AgentReleaseHook,
    AgentRequestIds, AgentSubmitResult,
};
use crate::bluetooth::discovery::DiscoveryOperationKind;
use crate::bluetooth::engine::{BluetoothCore, CoreAction, CoreSnapshot};
use crate::bluetooth::object_store::{InterfaceMap, PropertyMap, PropertyValue};

use super::proxies::{
    Adapter1Proxy, AgentManager1Proxy, BusDaemonProxy, Device1Proxy, ManagedObjects,
    ObjectManagerProxy, PropertiesProxy,
};

const COMMAND_CAPACITY: usize = 64;
const TASK_LIMIT: usize = 16;
const TASK_QUEUE_CAPACITY: usize = 64;
const SIGNAL_QUEUE_CAPACITY: usize = 32;
const DBUS_CALL_TIMEOUT: Duration = Duration::from_millis(3_000);
const PAIRING_DBUS_TIMEOUT: Duration = Duration::from_secs(130);

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
    Pair {
        session_generation: u64,
        object_path: String,
    },
    SetTrusted {
        session_generation: u64,
        object_path: String,
        target: bool,
    },
    Forget {
        session_generation: u64,
        object_path: String,
    },
}

#[derive(Clone, Copy, Debug)]
enum WorkerControl {
    LifecycleChanged,
    ScanOwnersChanged,
    Shutdown,
    CancelPairing { session_generation: u64 },
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
    pairing_controls: Receiver<WorkerControl>,
}

#[derive(Default)]
struct QueuedActions {
    ordinary: VecDeque<CoreAction>,
    cancellation: Option<CoreAction>,
}

impl QueuedActions {
    fn clear(&mut self) {
        self.ordinary.clear();
        self.cancellation = None;
    }

    fn queue_cancellation(&mut self, action: CoreAction) {
        debug_assert!(matches!(&action, CoreAction::CancelPairing { .. }));
        self.cancellation = Some(action);
    }
}

/// One long-lived worker and system-bus connection for one Bluetooth backend.
/// All channels are bounded; the GUI-facing methods only use `try_send`.
pub struct BluetoothWorker {
    commands: Sender<WorkerCommand>,
    controls: Sender<WorkerControl>,
    scan_controls: Sender<WorkerControl>,
    shutdown: Sender<WorkerControl>,
    pairing_controls: Sender<WorkerControl>,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    scan_owners: Arc<std::sync::Mutex<ScanOwnerMailbox>>,
    agent_broker: Arc<std::sync::Mutex<Option<AgentBroker>>>,
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
        let (pairing_controls, pairing_control_rx) = async_channel::bounded(1);
        let lifecycle = Arc::new(std::sync::Mutex::new(LifecycleRequest::default()));
        let scan_owners = Arc::new(std::sync::Mutex::new(ScanOwnerMailbox::default()));
        let agent_broker = Arc::new(std::sync::Mutex::new(None));
        let agent_request_ids = Arc::new(AgentRequestIds::default());
        let callback = Arc::new(on_snapshot);
        let worker_lifecycle = Arc::clone(&lifecycle);
        let worker_scan_owners = Arc::clone(&scan_owners);
        let worker_agent_broker = Arc::clone(&agent_broker);
        let worker_agent_request_ids = Arc::clone(&agent_request_ids);
        let thread = std::thread::Builder::new()
            .name(String::from("astrea-bluetooth-bluez"))
            .spawn(move || {
                smol::block_on(run_worker(
                    WorkerChannels {
                        commands: command_rx,
                        controls: control_rx,
                        scan_controls: scan_control_rx,
                        shutdown: shutdown_rx,
                        pairing_controls: pairing_control_rx,
                    },
                    worker_lifecycle,
                    worker_scan_owners,
                    worker_agent_broker,
                    worker_agent_request_ids,
                    callback,
                ));
            })?;
        Ok(Self {
            commands,
            controls,
            scan_controls,
            shutdown,
            pairing_controls,
            lifecycle,
            scan_owners,
            agent_broker,
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

    pub fn pair(&self, session_generation: u64, object_path: String) -> bool {
        self.commands
            .try_send(WorkerCommand::Pair {
                session_generation,
                object_path,
            })
            .is_ok()
    }

    pub fn set_trusted(&self, session_generation: u64, object_path: String, target: bool) -> bool {
        self.commands
            .try_send(WorkerCommand::SetTrusted {
                session_generation,
                object_path,
                target,
            })
            .is_ok()
    }

    pub fn forget(&self, session_generation: u64, object_path: String) -> bool {
        self.commands
            .try_send(WorkerCommand::Forget {
                session_generation,
                object_path,
            })
            .is_ok()
    }

    pub fn cancel_pairing(&self, session_generation: u64) -> bool {
        self.pairing_controls
            .try_send(WorkerControl::CancelPairing { session_generation })
            .is_ok()
    }

    pub fn submit_agent_text(&self, request_id: u64, text: &str) -> AgentSubmitResult {
        self.agent_broker
            .lock()
            .ok()
            .and_then(|broker| {
                broker
                    .as_ref()
                    .map(|broker| broker.submit_text(request_id, text))
            })
            .unwrap_or(AgentSubmitResult::Ignored)
    }

    pub fn confirm_agent_request(&self, request_id: u64, accepted: bool) -> AgentSubmitResult {
        self.agent_broker
            .lock()
            .ok()
            .and_then(|broker| {
                broker
                    .as_ref()
                    .map(|broker| broker.confirm(request_id, accepted))
            })
            .unwrap_or(AgentSubmitResult::Ignored)
    }

    pub fn reject_agent_request(&self, request_id: u64) -> AgentSubmitResult {
        self.agent_broker
            .lock()
            .ok()
            .and_then(|broker| broker.as_ref().map(|broker| broker.reject(request_id)))
            .unwrap_or(AgentSubmitResult::Ignored)
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
        let _ = self._thread.take();
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
    fn owner(&self) -> TransportFuture<'static, Result<Option<String>, OwnerLookupError>>;
    fn next_signal(&self) -> TransportFuture<'static, SignalOutcome>;
    fn set_agent_authority(&self, session_generation: u64, bluez_generation: u64, owner: &str);
    fn invalidate_agent(&self);
    fn unregister_agent(&self) -> TransportFuture<'static, ()>;
    fn agent_broker(&self) -> Option<AgentBroker>;
    fn agent_wake(&self) -> Receiver<()>;
    fn agent_prompt(&self) -> AgentPromptView;
    fn register_agent_task(&self, action: CoreAction) -> TaskFuture;
    fn execute(&self, action: CoreAction) -> TaskFuture;
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum OwnerLookupError {
    NameHasNoOwner,
    Failed(String),
}

impl std::fmt::Display for OwnerLookupError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NameHasNoOwner => formatter.write_str("org.bluez has no owner"),
            Self::Failed(error) => formatter.write_str(error),
        }
    }
}

impl OwnerLookupError {
    fn from_zbus(error: zbus::Error, operation: &'static str) -> Self {
        if Self::is_name_has_no_owner(&error) {
            Self::NameHasNoOwner
        } else {
            Self::Failed(format!("{operation}: {error}"))
        }
    }

    fn is_name_has_no_owner(error: &zbus::Error) -> bool {
        match error {
            zbus::Error::FDO(error) => {
                matches!(error.as_ref(), zbus::fdo::Error::NameHasNoOwner(_))
            }
            zbus::Error::MethodError(name, _, _) => {
                name.as_str() == "org.freedesktop.DBus.Error.NameHasNoOwner"
            }
            _ => false,
        }
    }
}

struct ZbusTransport {
    agent_request_ids: Arc<AgentRequestIds>,
}

struct ZbusBusSession {
    connection: Arc<Connection>,
    streams: Arc<futures::lock::Mutex<BusStreams>>,
    agent: AgentBroker,
    agent_registration: AgentRegistrationTracker,
}

#[derive(Clone, Default)]
struct AgentRegistrationTracker {
    state: Arc<std::sync::Mutex<AgentRegistrationState>>,
}

#[derive(Default)]
struct AgentRegistrationState {
    epoch: u64,
    marker: Option<AgentRegistrationMarker>,
}

struct AgentRegistrationMarker {
    session_generation: u64,
    bluez_generation: u64,
    owner: String,
    epoch: u64,
    registered: bool,
}

impl AgentRegistrationTracker {
    fn begin(
        &self,
        session_generation: u64,
        bluez_generation: u64,
        owner: &str,
    ) -> Result<(bool, u64), String> {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        if let Some(marker) = state.marker.as_ref().filter(|marker| {
            marker.session_generation == session_generation
                && marker.bluez_generation == bluez_generation
                && marker.owner == owner
                && marker.registered
        }) {
            return Ok((true, marker.epoch));
        }
        let epoch = state
            .epoch
            .checked_add(1)
            .ok_or_else(|| String::from("Bluetooth Agent registration epoch exhausted"))?;
        state.epoch = epoch;
        state.marker = Some(AgentRegistrationMarker {
            session_generation,
            bluez_generation,
            owner: owner.to_owned(),
            epoch,
            registered: false,
        });
        Ok((false, epoch))
    }

    fn mark_registered(
        &self,
        session_generation: u64,
        bluez_generation: u64,
        owner: &str,
        epoch: u64,
    ) -> bool {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        let current_epoch = state.epoch;
        let Some(marker) = state.marker.as_mut().filter(|marker| {
            marker.session_generation == session_generation
                && marker.bluez_generation == bluez_generation
                && marker.owner == owner
                && marker.epoch == epoch
                && current_epoch == epoch
        }) else {
            return false;
        };
        marker.registered = true;
        true
    }

    fn is_registered(
        &self,
        session_generation: u64,
        bluez_generation: u64,
        owner: &str,
        epoch: u64,
    ) -> bool {
        let state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        state.marker.as_ref().is_some_and(|marker| {
            marker.session_generation == session_generation
                && marker.bluez_generation == bluez_generation
                && marker.owner == owner
                && marker.epoch == epoch
                && marker.registered
                && state.epoch == epoch
        })
    }

    fn release(&self, session_generation: u64, bluez_generation: u64) {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        if state.marker.as_ref().is_some_and(|marker| {
            marker.session_generation == session_generation
                && marker.bluez_generation == bluez_generation
        }) {
            state.marker = None;
            state.epoch = state.epoch.saturating_add(1);
        }
    }

    fn invalidate_if_replaced(&self, session_generation: u64, bluez_generation: u64) -> bool {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        if state.marker.as_ref().is_some_and(|marker| {
            marker.session_generation != session_generation
                || marker.bluez_generation != bluez_generation
        }) {
            state.marker = None;
            state.epoch = state.epoch.saturating_add(1);
            true
        } else {
            false
        }
    }

    fn invalidate_all(&self) {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        if state.marker.take().is_some() {
            state.epoch = state.epoch.saturating_add(1);
        }
    }

    fn take_for_unregister(&self) -> Option<AgentRegistrationMarker> {
        let mut state = self
            .state
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        let marker = state.marker.take();
        if marker.is_some() {
            state.epoch = state.epoch.saturating_add(1);
        }
        marker.filter(|marker| marker.registered)
    }
}

impl BusTransport for ZbusTransport {
    fn connect(&self) -> TransportFuture<'static, Result<BusSessionHandle, String>> {
        let agent_request_ids = Arc::clone(&self.agent_request_ids);
        Box::pin(async move {
            let connection =
                bounded_result(Connection::system(), "system D-Bus connection").await?;
            let streams =
                bounded_result(BusStreams::new(&connection), "BlueZ signal subscriptions").await?;
            Ok(Arc::new(ZbusBusSession {
                connection: Arc::new(connection),
                streams: Arc::new(futures::lock::Mutex::new(streams)),
                agent: AgentBroker::new_with_request_ids(agent_request_ids),
                agent_registration: AgentRegistrationTracker::default(),
            }) as BusSessionHandle)
        })
    }
}

impl BusSession for ZbusBusSession {
    fn owner(&self) -> TransportFuture<'static, Result<Option<String>, OwnerLookupError>> {
        let connection = Arc::clone(&self.connection);
        Box::pin(async move {
            let proxy =
                bounded_zbus_result(BusDaemonProxy::new(&connection), "D-Bus daemon proxy").await?;
            match bounded_zbus_result(proxy.get_name_owner("org.bluez"), "BlueZ owner lookup").await
            {
                Ok(owner) => Ok(Some(owner)),
                Err(OwnerLookupError::NameHasNoOwner) => Ok(None),
                Err(error) => Err(error),
            }
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

    fn set_agent_authority(&self, session_generation: u64, bluez_generation: u64, owner: &str) {
        if self
            .agent_registration
            .invalidate_if_replaced(session_generation, bluez_generation)
        {
            self.agent.on_bluez_owner_replaced();
        }
        if owner.is_empty() {
            self.agent_registration.invalidate_all();
            self.agent.on_bluez_owner_replaced();
        } else {
            self.agent
                .set_authority(session_generation, bluez_generation, owner);
        }
    }

    fn invalidate_agent(&self) {
        self.agent_registration.invalidate_all();
        self.agent.on_bluez_owner_replaced();
    }

    fn unregister_agent(&self) -> TransportFuture<'static, ()> {
        let connection = Arc::clone(&self.connection);
        let registration = self.agent_registration.clone();
        let broker = self.agent.clone();
        Box::pin(async move {
            let current = registration.take_for_unregister();
            broker.on_service_stop();
            let Some(current) = current else {
                return;
            };
            let Ok(path) = OwnedObjectPath::try_from(AGENT_OBJECT_PATH) else {
                return;
            };
            let Ok(builder) =
                AgentManager1Proxy::builder(&connection).destination(current.owner.as_str())
            else {
                return;
            };
            let Ok(proxy) = builder.build().await else {
                return;
            };
            let _ = bounded_result(proxy.unregister_agent(path), "BlueZ Agent1 unregister").await;
        })
    }

    fn agent_broker(&self) -> Option<AgentBroker> {
        Some(self.agent.clone())
    }

    fn agent_wake(&self) -> Receiver<()> {
        self.agent.wake_receiver()
    }

    fn agent_prompt(&self) -> AgentPromptView {
        self.agent.prompt()
    }

    fn register_agent_task(&self, action: CoreAction) -> TaskFuture {
        let CoreAction::RegisterAgent {
            session_generation,
            bluez_generation,
            owner,
            operation_id,
            pairing_epoch,
            device_path,
        } = action
        else {
            unreachable!("register_agent_task called for another action");
        };
        let action = CoreAction::RegisterAgent {
            session_generation,
            bluez_generation,
            owner: owner.clone(),
            operation_id,
            pairing_epoch,
            device_path,
        };
        let connection = Arc::clone(&self.connection);
        let broker = self.agent.clone();
        let registration = self.agent_registration.clone();
        let release_registration = registration.clone();
        let release_hook = AgentReleaseHook::new(move |session_generation, bluez_generation| {
            release_registration.release(session_generation, bluez_generation);
        });
        Box::pin(async move {
            let (already_registered, epoch) =
                match registration.begin(session_generation, bluez_generation, &owner) {
                    Ok(result) => result,
                    Err(error) => {
                        return TaskResult::Operation {
                            action,
                            result: Err(error),
                        };
                    }
                };
            let mut result = if already_registered {
                Ok(())
            } else {
                let path = OwnedObjectPath::try_from(AGENT_OBJECT_PATH)
                    .map_err(|error| format!("invalid Agent1 object path: {error}"));
                match path {
                    Ok(path) => {
                        let exported = bounded_result(
                            connection.object_server().at(
                                AGENT_OBJECT_PATH,
                                Agent1::with_release_hook(broker.clone(), release_hook),
                            ),
                            "Agent1 export",
                        )
                        .await;
                        match exported {
                            Ok(_) => {
                                match AgentManager1Proxy::builder(&connection)
                                    .destination(owner.as_str())
                                {
                                    Ok(builder) => match builder.build().await {
                                        Ok(proxy) => {
                                            bounded_result(
                                                proxy.register_agent(path, AGENT_CAPABILITY),
                                                "BlueZ Agent1 registration",
                                            )
                                            .await
                                        }
                                        Err(error) => Err(error.to_string()),
                                    },
                                    Err(error) => Err(error.to_string()),
                                }
                            }
                            Err(error) => Err(error),
                        }
                    }
                    Err(error) => Err(error),
                }
            };
            if result.is_ok() {
                let still_registered = if already_registered {
                    registration.is_registered(session_generation, bluez_generation, &owner, epoch)
                } else {
                    registration.mark_registered(
                        session_generation,
                        bluez_generation,
                        &owner,
                        epoch,
                    )
                };
                if !still_registered {
                    result = Err(String::from(
                        "Bluetooth Agent registration was invalidated before Pair",
                    ));
                }
            }
            TaskResult::Operation { action, result }
        })
    }

    fn execute(&self, action: CoreAction) -> TaskFuture {
        match action {
            CoreAction::RegisterAgent { .. } => self.register_agent_task(action),
            CoreAction::Pair {
                pairing_epoch,
                device_path,
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                ..
            } => {
                self.agent.set_pairing_context(
                    pairing_epoch,
                    session_generation,
                    bluez_generation,
                    &device_path,
                );
                make_task(
                    &self.connection,
                    CoreAction::Pair {
                        pairing_epoch,
                        device_path,
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                    },
                )
            }
            action @ CoreAction::CancelPairing { .. } => {
                self.agent.cancel_for_lifecycle();
                make_task(&self.connection, action)
            }
            action => make_task(&self.connection, action),
        }
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
    agent_broker: Arc<std::sync::Mutex<Option<AgentBroker>>>,
    agent_request_ids: Arc<AgentRequestIds>,
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    run_worker_with_transport(
        channels,
        lifecycle,
        scan_owners,
        agent_broker,
        Arc::new(ZbusTransport { agent_request_ids }),
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
        let owner = session.owner().await.map_err(|error| error.to_string())?;
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

fn best_effort_unregister_agent(session: Option<BusSessionHandle>) {
    let Some(session) = session else {
        return;
    };
    if let Some(broker) = session.agent_broker() {
        broker.on_service_stop();
    }
    smol::spawn(async move {
        session.unregister_agent().await;
    })
    .detach();
}

fn clear_agent_broker(agent_broker: &std::sync::Mutex<Option<AgentBroker>>) {
    *agent_broker
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner) = None;
}

fn apply_scan_owner_update<F>(
    scan_owners: &std::sync::Mutex<ScanOwnerMailbox>,
    core: &mut BluetoothCore,
    queued_actions: &mut QueuedActions,
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
    queued_actions: &mut QueuedActions,
    in_flight_device_paths: &mut BTreeSet<String>,
) {
    retire_in_flight_tasks(tasks, in_flight_device_paths);
    queued_actions.clear();
}

fn retire_in_flight_tasks(
    tasks: &mut FuturesUnordered<TaskFuture>,
    in_flight_device_paths: &mut BTreeSet<String>,
) {
    *tasks = FuturesUnordered::new();
    in_flight_device_paths.clear();
}

async fn run_worker_with_transport<F>(
    channels: WorkerChannels,
    lifecycle: Arc<std::sync::Mutex<LifecycleRequest>>,
    scan_owners: Arc<std::sync::Mutex<ScanOwnerMailbox>>,
    agent_broker: Arc<std::sync::Mutex<Option<AgentBroker>>>,
    transport: Arc<dyn BusTransport>,
    on_snapshot: Arc<F>,
) where
    F: Fn(CoreSnapshot) + Send + Sync + 'static,
{
    let publish = Arc::clone(&on_snapshot);
    let current_agent = Arc::clone(&agent_broker);
    let on_snapshot = Arc::new(move |snapshot: CoreSnapshot| {
        if !snapshot.pairing
            && let Some(broker) = current_agent
                .lock()
                .unwrap_or_else(std::sync::PoisonError::into_inner)
                .as_ref()
        {
            broker.clear_pairing_context();
        }
        publish(snapshot);
    });
    let WorkerChannels {
        commands,
        controls,
        scan_controls,
        shutdown,
        pairing_controls,
    } = channels;
    let mut core = BluetoothCore::default();
    let mut state = ConnectionState::Stopped;
    let mut session = None::<BusSessionHandle>;
    let mut owner = String::new();
    let mut tasks = FuturesUnordered::<TaskFuture>::new();
    let mut cancellation_tasks = FuturesUnordered::<TaskFuture>::new();
    let mut queued_actions = QueuedActions::default();
    let mut in_flight_device_paths = BTreeSet::<String>::new();
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
        let mut cancellation_task_completed = false;
        {
            let task = if tasks.is_empty() {
                Box::pin(pending()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            } else {
                Box::pin(tasks.next()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            };
            let cancellation_task = if cancellation_tasks.is_empty() {
                Box::pin(pending()) as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            } else {
                Box::pin(cancellation_tasks.next())
                    as Pin<Box<dyn Future<Output = Option<TaskResult>> + Send>>
            };
            let signal = session.as_ref().map_or_else(
                || Box::pin(pending()) as TransportFuture<'static, SignalOutcome>,
                |session| session.next_signal(),
            );
            let agent_wake = session.as_ref().map_or_else(
                || {
                    Box::pin(pending())
                        as TransportFuture<'static, Result<(), async_channel::RecvError>>
                },
                |session| {
                    let receiver = session.agent_wake();
                    Box::pin(async move { receiver.recv().await })
                        as TransportFuture<'static, Result<(), async_channel::RecvError>>
                },
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

            futures::pin_mut!(task, cancellation_task, signal, connect, timer);
            futures::select_biased! {
                shutdown_result = shutdown.recv().fuse() => {
                    if matches!(shutdown_result, Ok(WorkerControl::Shutdown)) {
                        shutdown_requested = true;
                    } else {
                        return;
                    }
                },
                result = cancellation_task.fuse() => {
                    cancellation_task_completed = result.is_some();
                },
                control_result = controls.recv().fuse() => {
                    match control_result {
                        Ok(WorkerControl::LifecycleChanged) => {
                            lifecycle_changed = Some(lifecycle.lock().map(|state| *state).unwrap_or_default());
                        }
                        Ok(WorkerControl::Shutdown) => shutdown_requested = true,
                        Ok(WorkerControl::ScanOwnersChanged) => scan_changed = true,
                        Ok(WorkerControl::CancelPairing { .. }) => {},
                        Err(_) => return,
                    }
                },
                scan_result = scan_controls.recv().fuse() => {
                    match scan_result {
                        Ok(WorkerControl::ScanOwnersChanged) => scan_changed = true,
                        Ok(WorkerControl::Shutdown) => shutdown_requested = true,
                        Ok(WorkerControl::LifecycleChanged) => {},
                        Ok(WorkerControl::CancelPairing { .. }) => {},
                        Err(_) => return,
                    }
                },
                pairing_result = pairing_controls.recv().fuse() => {
                    match pairing_result {
                        Ok(WorkerControl::CancelPairing { session_generation }) => {
                            handle_cancel_pairing(
                                session_generation,
                                &mut core,
                                &mut queued_actions,
                                on_snapshot.as_ref(),
                            );
                        }
                        Ok(WorkerControl::Shutdown) => shutdown_requested = true,
                        Ok(_) => {},
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
                    if let Some(result) = result
                        && let Some(object_path) = handle_task_result(
                            result,
                            &mut core,
                            &mut owner,
                            &mut queued_actions,
                            on_snapshot.as_ref(),
                        )
                    {
                        in_flight_device_paths.remove(&object_path);
                    }
                },
                signal_result = signal.fuse() => {
                    match signal_result {
                        SignalOutcome::Message(message) => {
                            let previous_generation = core.bluez_generation();
                            handle_signal(
                                message,
                                &mut core,
                                &mut owner,
                                session.as_ref(),
                                &mut queued_actions,
                                on_snapshot.as_ref(),
                            );
                            if core.bluez_generation() != previous_generation {
                                generation_replaced = true;
                            }
                        }
                        SignalOutcome::Error(error) => connection_lost = Some(error),
                        SignalOutcome::End => connection_lost = Some(String::from("BlueZ signal stream ended")),
                    }
                },
                agent_result = agent_wake.fuse() => {
                    if agent_result.is_ok()
                        && let Some(session) = session.as_ref()
                    {
                        let prompt = session.agent_prompt();
                        if core.agent_prompt_changed(
                            core.session_generation(),
                            core.bluez_generation(),
                            prompt,
                        ) {
                            on_snapshot(core.snapshot().clone());
                        }
                    }
                },
                connection = connect.fuse() => {
                    connection_result = connection;
                },
                _ = timer.fuse() => reconnect_timer_fired = true,
            }
        }

        if cancellation_task_completed {
            cancellation_tasks = FuturesUnordered::new();
        }

        if generation_replaced {
            retire_in_flight_tasks(&mut tasks, &mut in_flight_device_paths);
            cancellation_tasks = FuturesUnordered::new();
            queued_actions.cancellation = None;
        }

        if shutdown_requested {
            let current_session = session.take();
            best_effort_stop_discovery(current_session.clone(), core.stop_discovery_action());
            best_effort_unregister_agent(current_session);
            clear_agent_broker(&agent_broker);
            return;
        }

        if let Some(requested) = lifecycle_changed {
            if !requested.running && requested.session_generation >= core.session_generation() {
                let current_session = session.take();
                best_effort_stop_discovery(current_session.clone(), core.stop_discovery_action());
                best_effort_unregister_agent(current_session);
                clear_agent_broker(&agent_broker);
                connecting = None;
                reconnect_at = None;
                retire_generation_work(
                    &mut tasks,
                    &mut queued_actions,
                    &mut in_flight_device_paths,
                );
                cancellation_tasks = FuturesUnordered::new();
                owner.clear();
                state = ConnectionState::Stopped;
                if core.stop_generation(requested.session_generation) {
                    on_snapshot(core.snapshot().clone());
                }
            } else if requested.running && requested.session_generation > core.session_generation()
            {
                let current_session = session.take();
                best_effort_stop_discovery(current_session, core.stop_discovery_action());
                clear_agent_broker(&agent_broker);
                connecting = None;
                reconnect_at = Some(Instant::now());
                retire_generation_work(
                    &mut tasks,
                    &mut queued_actions,
                    &mut in_flight_device_paths,
                );
                cancellation_tasks = FuturesUnordered::new();
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
                    let actions = core.owner_changed(core.session_generation(), new_owner);
                    if owner.is_empty() {
                        new_session.invalidate_agent();
                    } else {
                        new_session.set_agent_authority(
                            core.session_generation(),
                            core.bluez_generation(),
                            &owner,
                        );
                    }
                    *agent_broker
                        .lock()
                        .unwrap_or_else(std::sync::PoisonError::into_inner) =
                        new_session.agent_broker();
                    session = Some(new_session);
                    state = ConnectionState::Connected;
                    enqueue_actions(
                        actions,
                        &mut queued_actions,
                        &mut core,
                        on_snapshot.as_ref(),
                    );
                    on_snapshot(core.snapshot().clone());
                }
                Err(error) => {
                    clear_agent_broker(&agent_broker);
                    state = ConnectionState::Disconnected;
                    reconnect_at = Some(backoff.on_failure(Instant::now()));
                    if core.bus_unavailable(core.session_generation(), error) {
                        on_snapshot(core.snapshot().clone());
                    }
                }
            }
        }

        if let Some(error) = connection_lost {
            if let Some(session) = session.as_ref() {
                session.invalidate_agent();
            }
            clear_agent_broker(&agent_broker);
            session = None;
            cancellation_tasks = FuturesUnordered::new();
            owner.clear();
            state = ConnectionState::Disconnected;
            reconnect_at = Some(backoff.on_failure(Instant::now()));
            retire_generation_work(&mut tasks, &mut queued_actions, &mut in_flight_device_paths);
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

        dispatch_pairing_cancellation(
            &mut queued_actions,
            &mut cancellation_tasks,
            session.as_ref(),
            &core,
            &owner,
        );

        while tasks.len() < TASK_LIMIT {
            let Some(action) = take_next_dispatchable_action(
                &mut queued_actions.ordinary,
                &in_flight_device_paths,
            ) else {
                break;
            };
            let Some(session) = session.as_ref() else {
                queued_actions.ordinary.push_front(action);
                break;
            };
            if let Some(object_path) = ordinary_device_path(&action) {
                in_flight_device_paths.insert(object_path.clone());
            }
            tasks.push(session.execute(action));
        }
    }
}

fn take_next_dispatchable_action(
    queued_actions: &mut VecDeque<CoreAction>,
    in_flight_device_paths: &BTreeSet<String>,
) -> Option<CoreAction> {
    let mut blocked = VecDeque::new();
    while let Some(action) = queued_actions.pop_front() {
        let blocked_for_device = ordinary_device_path(&action)
            .is_some_and(|object_path| in_flight_device_paths.contains(object_path));
        if blocked_for_device {
            blocked.push_back(action);
        } else {
            queued_actions.extend(blocked);
            return Some(action);
        }
    }
    queued_actions.extend(blocked);
    None
}

fn dispatch_pairing_cancellation(
    queued: &mut QueuedActions,
    cancellation_tasks: &mut FuturesUnordered<TaskFuture>,
    session: Option<&BusSessionHandle>,
    core: &BluetoothCore,
    owner: &str,
) {
    if !cancellation_tasks.is_empty() {
        return;
    }
    let Some(action @ CoreAction::CancelPairing { .. }) = queued.cancellation.take() else {
        return;
    };
    let current = matches!(
        &action,
        CoreAction::CancelPairing {
            session_generation,
            bluez_generation,
            owner: action_owner,
            ..
        } if *session_generation == core.session_generation()
            && *bluez_generation == core.bluez_generation()
            && action_owner == owner
    );
    if current && let Some(session) = session {
        cancellation_tasks.push(session.execute(action));
    }
}

fn ordinary_device_path(action: &CoreAction) -> Option<&String> {
    match action {
        CoreAction::Connect { object_path, .. }
        | CoreAction::RegisterAgent {
            device_path: object_path,
            ..
        }
        | CoreAction::Pair {
            device_path: object_path,
            ..
        }
        | CoreAction::SetTrusted { object_path, .. }
        | CoreAction::RemoveDevice { object_path, .. } => Some(object_path),
        CoreAction::StartBus { .. }
        | CoreAction::Probe { .. }
        | CoreAction::GetAll { .. }
        | CoreAction::SetPowered { .. }
        | CoreAction::Discovery { .. }
        | CoreAction::CancelPairing { .. } => None,
    }
}

async fn handle_command<F>(
    command: WorkerCommand,
    core: &mut BluetoothCore,
    queued: &mut QueuedActions,
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
        WorkerCommand::Pair {
            session_generation,
            object_path,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.pair_device(&object_path);
            enqueue_actions(action, queued, core, on_snapshot);
            on_snapshot(core.snapshot().clone());
        }
        WorkerCommand::SetTrusted {
            session_generation,
            object_path,
            target,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.set_device_trusted(&object_path, target);
            enqueue_actions(action, queued, core, on_snapshot);
            on_snapshot(core.snapshot().clone());
        }
        WorkerCommand::Forget {
            session_generation,
            object_path,
        } => {
            if session_generation != core.session_generation() {
                return;
            }
            let action = core.forget_device(&object_path);
            enqueue_actions(action, queued, core, on_snapshot);
            on_snapshot(core.snapshot().clone());
        }
    }
}

fn handle_cancel_pairing<F>(
    session_generation: u64,
    core: &mut BluetoothCore,
    queued: &mut QueuedActions,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    if session_generation != core.session_generation() {
        return;
    }
    if let Some(action) = core.cancel_pairing() {
        enqueue_actions([action], queued, core, on_snapshot);
        on_snapshot(core.snapshot().clone());
    }
}

fn handle_task_result<F>(
    result: TaskResult,
    core: &mut BluetoothCore,
    _owner: &mut String,
    queued: &mut QueuedActions,
    on_snapshot: &F,
) -> Option<String>
where
    F: Fn(CoreSnapshot),
{
    let mut completed_device_path = None;
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
                    completed_device_path = Some(object_path);
                }
                CoreAction::Probe { .. }
                | CoreAction::StartBus { .. }
                | CoreAction::GetAll { .. } => {}
                CoreAction::RegisterAgent {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    pairing_epoch,
                    device_path,
                    ..
                } => {
                    match result {
                        Ok(()) => {
                            let next = core.agent_registered(
                                session_generation,
                                bluez_generation,
                                operation_id,
                                pairing_epoch,
                            );
                            enqueue_actions(next, queued, core, on_snapshot);
                        }
                        Err(error) => {
                            core.agent_registration_failed(
                                session_generation,
                                bluez_generation,
                                operation_id,
                                pairing_epoch,
                                Some(error),
                            );
                        }
                    }
                    on_snapshot(core.snapshot().clone());
                    completed_device_path = Some(device_path);
                }
                CoreAction::Pair {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    pairing_epoch,
                    device_path,
                    ..
                } => {
                    core.pair_reply(
                        session_generation,
                        bluez_generation,
                        operation_id,
                        pairing_epoch,
                        &device_path,
                        result.is_ok(),
                        result.err(),
                    );
                    on_snapshot(core.snapshot().clone());
                    completed_device_path = Some(device_path);
                }
                CoreAction::CancelPairing { .. } => {}
                CoreAction::SetTrusted {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    object_path,
                    target,
                    ..
                } => {
                    let _ = core.trusted_reply(
                        session_generation,
                        bluez_generation,
                        operation_id,
                        &object_path,
                        target,
                        result.is_ok(),
                        result.err(),
                    );
                    completed_device_path = Some(object_path);
                }
                CoreAction::RemoveDevice {
                    session_generation,
                    bluez_generation,
                    operation_id,
                    object_path,
                    ..
                } => {
                    let _ = core.forget_reply(
                        session_generation,
                        bluez_generation,
                        operation_id,
                        &object_path,
                        result.is_ok(),
                        result.err(),
                    );
                    completed_device_path = Some(object_path);
                }
            }
        }
    }
    completed_device_path
}

fn handle_signal<F>(
    message: Message,
    core: &mut BluetoothCore,
    owner: &mut String,
    session: Option<&BusSessionHandle>,
    queued: &mut QueuedActions,
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
            if let Some(session) = session {
                if owner.is_empty() {
                    session.invalidate_agent();
                } else {
                    session.set_agent_authority(
                        core.session_generation(),
                        core.bluez_generation(),
                        owner,
                    );
                }
            }
            // owner_changed has created the replacement generation's actions.
            // Discard only the older queued generation here; the worker retires
            // old in-flight futures separately after this handler returns.
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
    queued: &mut QueuedActions,
    core: &mut BluetoothCore,
    on_snapshot: &F,
) where
    F: Fn(CoreSnapshot),
{
    for action in actions {
        if matches!(&action, CoreAction::CancelPairing { .. }) {
            queued.queue_cancellation(action);
            continue;
        }
        if let CoreAction::Connect { object_path, .. } = &action {
            queued.ordinary.retain(|queued_action| {
                !matches!(
                    queued_action,
                    CoreAction::Connect {
                        object_path: queued_path,
                        ..
                    } if queued_path == object_path
                )
            });
        }
        if let CoreAction::SetTrusted { object_path, .. } = &action {
            queued.ordinary.retain(|queued_action| {
                !matches!(
                    queued_action,
                    CoreAction::SetTrusted {
                        object_path: queued_path,
                        ..
                    } if queued_path == object_path
                )
            });
        }
        if queued.ordinary.len() < TASK_QUEUE_CAPACITY {
            queued.ordinary.push_back(action);
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
            operation_id,
            object_path,
            connect,
            ..
        } => {
            core.connect_reply(
                session_generation,
                bluez_generation,
                operation_id,
                &object_path,
                connect,
                false,
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
        CoreAction::RegisterAgent {
            session_generation,
            bluez_generation,
            operation_id,
            pairing_epoch,
            ..
        } => {
            core.agent_registration_failed(
                session_generation,
                bluez_generation,
                operation_id,
                pairing_epoch,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::Pair {
            session_generation,
            bluez_generation,
            operation_id,
            pairing_epoch,
            device_path,
            ..
        } => {
            core.pair_reply(
                session_generation,
                bluez_generation,
                operation_id,
                pairing_epoch,
                &device_path,
                false,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::CancelPairing { .. } => {}
        CoreAction::SetTrusted {
            session_generation,
            bluez_generation,
            operation_id,
            object_path,
            target,
            ..
        } => {
            core.trusted_reply(
                session_generation,
                bluez_generation,
                operation_id,
                &object_path,
                target,
                false,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
        CoreAction::RemoveDevice {
            session_generation,
            bluez_generation,
            operation_id,
            object_path,
            ..
        } => {
            core.forget_reply(
                session_generation,
                bluez_generation,
                operation_id,
                &object_path,
                false,
                Some(String::from("Bluetooth backend command queue is full")),
            );
        }
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
            CoreAction::RegisterAgent { .. } => TaskResult::Operation {
                action: CoreAction::RegisterAgent {
                    session_generation: 0,
                    bluez_generation: 0,
                    owner: String::new(),
                    operation_id: 0,
                    pairing_epoch: 0,
                    device_path: String::new(),
                },
                result: Err(String::from(
                    "Agent registration must use the connection session",
                )),
            },
            CoreAction::Pair {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                pairing_epoch,
                device_path,
            } => {
                let result = bounded_result_with_timeout(
                    async {
                        let proxy = Device1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(device_path.as_str())?
                            .build()
                            .await?;
                        proxy.pair().await
                    },
                    "BlueZ pairing operation",
                    PAIRING_DBUS_TIMEOUT,
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::Pair {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        pairing_epoch,
                        device_path,
                    },
                    result,
                }
            }
            CoreAction::CancelPairing {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                pairing_epoch,
                device_path,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Device1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(device_path.as_str())?
                            .build()
                            .await?;
                        proxy.cancel_pairing().await
                    },
                    "BlueZ pairing cancellation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::CancelPairing {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        pairing_epoch,
                        device_path,
                    },
                    result,
                }
            }
            CoreAction::SetTrusted {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                object_path,
                target,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Device1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(object_path.as_str())?
                            .build()
                            .await?;
                        proxy.set_trusted(target).await
                    },
                    "BlueZ trust operation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::SetTrusted {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        object_path,
                        target,
                    },
                    result,
                }
            }
            CoreAction::RemoveDevice {
                session_generation,
                bluez_generation,
                owner,
                operation_id,
                adapter_path,
                object_path,
            } => {
                let result = bounded_result(
                    async {
                        let proxy = Adapter1Proxy::builder(&connection)
                            .destination(owner.as_str())?
                            .path(adapter_path.as_str())?
                            .build()
                            .await?;
                        proxy
                            .remove_device(OwnedObjectPath::try_from(object_path.as_str())?)
                            .await
                    },
                    "BlueZ forget operation",
                )
                .await;
                TaskResult::Operation {
                    action: CoreAction::RemoveDevice {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        adapter_path,
                        object_path,
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
    bounded_result_with_timeout(future, operation, DBUS_CALL_TIMEOUT).await
}

async fn bounded_result_with_timeout<T, E>(
    future: impl Future<Output = Result<T, E>>,
    operation: &'static str,
    timeout_duration: Duration,
) -> Result<T, String>
where
    E: std::fmt::Display,
{
    futures::pin_mut!(future);
    let timeout = async_io::Timer::after(timeout_duration);
    futures::pin_mut!(timeout);
    match futures::future::select(future, timeout).await {
        futures::future::Either::Left((result, _)) => result.map_err(|error| error.to_string()),
        futures::future::Either::Right((_, _)) => Err(format!("{operation} timed out")),
    }
}

async fn bounded_zbus_result<T>(
    future: impl Future<Output = zbus::Result<T>>,
    operation: &'static str,
) -> Result<T, OwnerLookupError> {
    futures::pin_mut!(future);
    let timeout = async_io::Timer::after(DBUS_CALL_TIMEOUT);
    futures::pin_mut!(timeout);
    match futures::future::select(future, timeout).await {
        futures::future::Either::Left((result, _)) => {
            result.map_err(|error| OwnerLookupError::from_zbus(error, operation))
        }
        futures::future::Either::Right((_, _)) => {
            Err(OwnerLookupError::Failed(format!("{operation} timed out")))
        }
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
        AgentRegistrationTracker, BluetoothWorker, BusSession, BusSessionHandle, BusTransport,
        CoreAction, LifecycleRequest, OwnerLookupError, PAIRING_DBUS_TIMEOUT, QueuedActions,
        ScanOwnerMailbox, TASK_LIMIT, TASK_QUEUE_CAPACITY, TaskFuture, TaskResult, WorkerChannels,
        WorkerCommand, WorkerControl, apply_scan_owner_update, dispatch_pairing_cancellation,
        enqueue_actions, retire_generation_work, run_worker_with_transport,
        update_scan_owner_release, update_scan_owner_request,
    };
    use super::{ConnectionState, ReconnectBackoff, SignalOutcome};
    use crate::bluetooth::agent::{
        Agent1, AgentBroker, AgentError, AgentPromptKind, AgentPromptView, AgentReleaseHook,
        AgentRequestIds, AgentSubmitResult,
    };
    use crate::bluetooth::engine::{BluetoothCore, CoreSnapshot};
    use crate::bluetooth::object_store::{InterfaceMap, PropertyMap, PropertyValue};
    use crate::bluetooth::pairing::PAIRING_TIMEOUT;
    use async_channel::{Receiver, Sender};
    use futures::stream::FuturesUnordered;
    use std::collections::{BTreeMap, BTreeSet, VecDeque};
    use std::future::Future;
    use std::pin::Pin;
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::{Arc, Condvar, Mutex};
    use std::time::{Duration, Instant};
    use zbus::Message;

    const AGENT_OWNER: &str = ":1.42";
    const AGENT_DEVICE: &str = "/org/bluez/hci0/dev_AA";

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
        Message(Message),
        Error(String),
        End,
    }

    struct FakeSession {
        owner: Option<String>,
        owner_error: Option<String>,
        signals: Receiver<FakeSignal>,
        agent_wake: Receiver<()>,
        probe_objects: BTreeMap<String, InterfaceMap>,
        operations: Arc<OperationTracker>,
        broker: AgentBroker,
        registration: AgentRegistrationTracker,
        get_all_properties: Arc<Mutex<PropertyMap>>,
        authority: Arc<Mutex<(u64, u64)>>,
        prompt_override: Arc<Mutex<Option<AgentPromptView>>>,
        prompt_reads: Arc<AtomicUsize>,
    }

    #[derive(Clone)]
    struct FakeSessionControl {
        signals: Sender<FakeSignal>,
        operations: Arc<OperationTracker>,
        broker: AgentBroker,
        registration: AgentRegistrationTracker,
        get_all_properties: Arc<Mutex<PropertyMap>>,
        authority: Arc<Mutex<(u64, u64)>>,
        prompt_override: Arc<Mutex<Option<AgentPromptView>>>,
        prompt_reads: Arc<AtomicUsize>,
    }

    impl FakeSessionControl {
        fn start_passkey_request(&self) -> Receiver<Result<u32, AgentError>> {
            let broker = self.broker.clone();
            let (session_generation, bluez_generation) =
                *self.authority.lock().expect("Agent authority lock");
            let (result_tx, result_rx) = async_channel::bounded(1);
            std::thread::spawn(move || {
                let result = smol::block_on(broker.request_passkey(
                    AGENT_OWNER,
                    session_generation,
                    bluez_generation,
                    AGENT_DEVICE,
                ));
                let _ = result_tx.try_send(result);
            });
            result_rx
        }

        fn prompt(&self) -> AgentPromptView {
            self.broker.prompt()
        }

        fn release_from_bluez(
            &self,
            session_generation: u64,
            bluez_generation: u64,
        ) -> Result<(), AgentError> {
            let registration = self.registration.clone();
            let hook = AgentReleaseHook::new(move |session, bluez| {
                registration.release(session, bluez);
            });
            Agent1::with_release_hook(self.broker.clone(), hook).release_from_bluez(
                AGENT_OWNER,
                session_generation,
                bluez_generation,
            )
        }

        fn set_get_all_paired(&self, paired: bool) {
            self.get_all_properties
                .lock()
                .expect("GetAll properties lock")
                .insert(String::from("Paired"), PropertyValue::Boolean(paired));
        }

        fn emit(&self, message: Message) {
            self.signals
                .try_send(FakeSignal::Message(message))
                .expect("fake BlueZ signal");
        }

        fn publish_prompt_and_wait(&self, prompt: AgentPromptView) {
            let previous_reads = self.prompt_reads.load(Ordering::SeqCst);
            *self
                .prompt_override
                .lock()
                .expect("fake prompt override lock") = Some(prompt);
            self.broker.notify_for_test();
            let deadline = Instant::now() + Duration::from_secs(2);
            while self.prompt_reads.load(Ordering::SeqCst) == previous_reads {
                assert!(
                    Instant::now() < deadline,
                    "worker did not read Agent prompt"
                );
                std::thread::sleep(Duration::from_millis(2));
            }
        }
    }

    struct FakeSessionPlan {
        session: FakeSession,
        control: FakeSessionControl,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct DispatchedDeviceOperation {
        operation_id: u64,
        object_path: String,
        connect: bool,
    }

    struct OperationTracker {
        dispatched: Mutex<Vec<DispatchedDeviceOperation>>,
        completions: Mutex<BTreeMap<u64, Sender<Result<(), String>>>>,
        events: Mutex<Vec<&'static str>>,
        pair_release: Mutex<Option<Sender<Result<(), String>>>>,
        probes: AtomicUsize,
        active: AtomicUsize,
        max_active: AtomicUsize,
        changed: Condvar,
    }

    impl OperationTracker {
        fn new() -> Arc<Self> {
            Arc::new(Self {
                dispatched: Mutex::new(Vec::new()),
                completions: Mutex::new(BTreeMap::new()),
                events: Mutex::new(Vec::new()),
                pair_release: Mutex::new(None),
                probes: AtomicUsize::new(0),
                active: AtomicUsize::new(0),
                max_active: AtomicUsize::new(0),
                changed: Condvar::new(),
            })
        }

        fn record_device(
            &self,
            operation_id: u64,
            object_path: String,
            connect: bool,
        ) -> Receiver<Result<(), String>> {
            let (sender, receiver) = async_channel::bounded(1);
            self.dispatched
                .lock()
                .expect("dispatch log lock")
                .push(DispatchedDeviceOperation {
                    operation_id,
                    object_path,
                    connect,
                });
            self.completions
                .lock()
                .expect("completion lock")
                .insert(operation_id, sender);
            let active = self.active.fetch_add(1, Ordering::SeqCst) + 1;
            self.max_active.fetch_max(active, Ordering::SeqCst);
            self.changed.notify_all();
            receiver
        }

        fn record_probe(&self) {
            self.probes.fetch_add(1, Ordering::SeqCst);
            self.changed.notify_all();
        }

        fn record_event(&self, event: &'static str) {
            self.events.lock().expect("event log lock").push(event);
            self.changed.notify_all();
        }

        fn events(&self) -> Vec<&'static str> {
            self.events.lock().expect("event log lock").clone()
        }

        fn wait_for_event_count(&self, event: &'static str, count: usize) {
            let deadline = Instant::now() + Duration::from_secs(2);
            while self
                .events()
                .iter()
                .filter(|candidate| **candidate == event)
                .count()
                < count
            {
                assert!(
                    Instant::now() < deadline,
                    "timed out waiting for worker event count"
                );
                std::thread::sleep(Duration::from_millis(2));
            }
        }

        fn wait_for_event(&self, event: &'static str) {
            let deadline = Instant::now() + Duration::from_secs(2);
            while !self.events().contains(&event) {
                assert!(
                    Instant::now() < deadline,
                    "timed out waiting for worker event"
                );
                std::thread::sleep(Duration::from_millis(2));
            }
        }

        fn record_pair(&self) -> Receiver<Result<(), String>> {
            let (sender, receiver) = async_channel::bounded(1);
            *self.pair_release.lock().expect("pair release lock") = Some(sender);
            self.record_event("pair");
            receiver
        }

        fn complete_pair(&self) {
            self.finish_pair(Ok(()));
        }

        fn finish_pair(&self, result: Result<(), String>) {
            let sender = self
                .pair_release
                .lock()
                .expect("pair release lock")
                .take()
                .expect("pair release sender");
            sender.try_send(result).expect("pair release");
        }

        fn finish_device(&self, operation_id: u64) {
            self.completions
                .lock()
                .expect("completion lock")
                .remove(&operation_id);
            self.active.fetch_sub(1, Ordering::SeqCst);
            self.changed.notify_all();
        }

        fn complete(&self, operation_id: u64, result: Result<(), String>) {
            let sender = self
                .completions
                .lock()
                .expect("completion lock")
                .get(&operation_id)
                .cloned()
                .expect("operation completion sender");
            sender.try_send(result).expect("operation completion");
        }

        fn wait_for_dispatches(&self, count: usize) {
            let deadline = Instant::now() + Duration::from_secs(2);
            let mut dispatched = self.dispatched.lock().expect("dispatch log lock");
            while dispatched.len() < count {
                let remaining = deadline.saturating_duration_since(Instant::now());
                assert!(!remaining.is_zero(), "timed out waiting for dispatch");
                let (next, timeout) = self
                    .changed
                    .wait_timeout(dispatched, remaining)
                    .expect("dispatch wait");
                dispatched = next;
                assert!(!timeout.timed_out(), "timed out waiting for dispatch");
            }
        }

        fn wait_for_probes(&self, count: usize) {
            let deadline = Instant::now() + Duration::from_secs(2);
            while self.probes.load(Ordering::SeqCst) < count {
                assert!(Instant::now() < deadline, "timed out waiting for Probe");
                std::thread::sleep(Duration::from_millis(2));
            }
        }

        fn dispatched(&self) -> Vec<DispatchedDeviceOperation> {
            self.dispatched.lock().expect("dispatch log lock").clone()
        }

        fn max_active(&self) -> usize {
            self.max_active.load(Ordering::SeqCst)
        }
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
        fn owner(
            &self,
        ) -> Pin<Box<dyn Future<Output = Result<Option<String>, OwnerLookupError>> + Send + 'static>>
        {
            let owner = self.owner.clone();
            let owner_error = self.owner_error.clone();
            Box::pin(async move {
                if let Some(error) = owner_error {
                    return Err(OwnerLookupError::Failed(error));
                }
                Ok(owner)
            })
        }

        fn next_signal(&self) -> Pin<Box<dyn Future<Output = SignalOutcome> + Send + 'static>> {
            let signals = self.signals.clone();
            Box::pin(async move {
                match signals.recv().await {
                    Ok(FakeSignal::Message(message)) => SignalOutcome::Message(message),
                    Ok(FakeSignal::Error(error)) => SignalOutcome::Error(error),
                    Ok(FakeSignal::End) | Err(_) => SignalOutcome::End,
                }
            })
        }

        fn set_agent_authority(&self, session_generation: u64, bluez_generation: u64, owner: &str) {
            *self.authority.lock().expect("Agent authority lock") =
                (session_generation, bluez_generation);
            self.registration
                .invalidate_if_replaced(session_generation, bluez_generation);
            if owner.is_empty() {
                self.registration.invalidate_all();
            }
            self.broker
                .set_authority(session_generation, bluez_generation, owner);
        }

        fn invalidate_agent(&self) {
            self.registration.invalidate_all();
            self.broker.on_bluez_owner_replaced();
        }

        fn unregister_agent(&self) -> Pin<Box<dyn Future<Output = ()> + Send + 'static>> {
            Box::pin(async {})
        }

        fn agent_broker(&self) -> Option<AgentBroker> {
            Some(self.broker.clone())
        }

        fn agent_wake(&self) -> Receiver<()> {
            self.agent_wake.clone()
        }

        fn agent_prompt(&self) -> AgentPromptView {
            self.prompt_reads.fetch_add(1, Ordering::SeqCst);
            self.prompt_override
                .lock()
                .expect("fake prompt override lock")
                .clone()
                .unwrap_or_else(|| self.broker.prompt())
        }

        fn register_agent_task(&self, action: CoreAction) -> TaskFuture {
            if let CoreAction::RegisterAgent {
                session_generation,
                bluez_generation,
                owner,
                pairing_epoch,
                device_path,
                ..
            } = &action
            {
                self.broker.set_pairing_context(
                    *pairing_epoch,
                    *session_generation,
                    *bluez_generation,
                    device_path,
                );
                let (already_registered, epoch) = self
                    .registration
                    .begin(*session_generation, *bluez_generation, owner)
                    .expect("fake Agent registration epoch");
                if !already_registered {
                    self.operations.record_event("agent_exported");
                    self.operations.record_event("register_agent");
                    assert!(self.registration.mark_registered(
                        *session_generation,
                        *bluez_generation,
                        owner,
                        epoch
                    ));
                }
            }
            Box::pin(async move {
                TaskResult::Operation {
                    action,
                    result: Ok(()),
                }
            })
        }

        fn execute(&self, action: CoreAction) -> TaskFuture {
            let operations = Arc::clone(&self.operations);
            let probe_objects = self.probe_objects.clone();
            match action {
                CoreAction::Probe {
                    session_generation,
                    bluez_generation,
                    ..
                } => {
                    operations.record_probe();
                    Box::pin(async move {
                        TaskResult::Probe {
                            session_generation,
                            bluez_generation,
                            result: Ok(probe_objects),
                        }
                    })
                }
                CoreAction::GetAll {
                    session_generation,
                    owner,
                    token,
                } => {
                    let properties = self
                        .get_all_properties
                        .lock()
                        .expect("GetAll properties lock")
                        .clone();
                    Box::pin(async move {
                        TaskResult::GetAll {
                            session_generation,
                            owner,
                            token,
                            result: Ok(properties),
                        }
                    })
                }
                CoreAction::RegisterAgent { .. } => self.register_agent_task(action),
                CoreAction::Pair { .. } => {
                    let completion = operations.record_pair();
                    Box::pin(async move {
                        let result = completion
                            .recv()
                            .await
                            .unwrap_or_else(|_| Err(String::from("fake Pair canceled")));
                        TaskResult::Operation { action, result }
                    })
                }
                CoreAction::CancelPairing { .. } => {
                    operations.record_event("cancel_pairing");
                    Box::pin(async move {
                        TaskResult::Operation {
                            action,
                            result: Ok(()),
                        }
                    })
                }
                action @ CoreAction::SetPowered { .. }
                | action @ CoreAction::Discovery { .. }
                | action @ CoreAction::StartBus { .. }
                | action @ CoreAction::SetTrusted { .. }
                | action @ CoreAction::RemoveDevice { .. } => Box::pin(async move {
                    TaskResult::Operation {
                        action,
                        result: Ok(()),
                    }
                }),
                CoreAction::Connect {
                    session_generation,
                    bluez_generation,
                    owner,
                    operation_id,
                    object_path,
                    connect,
                } => {
                    let completion =
                        operations.record_device(operation_id, object_path.clone(), connect);
                    let action = CoreAction::Connect {
                        session_generation,
                        bluez_generation,
                        owner,
                        operation_id,
                        object_path,
                        connect,
                    };
                    Box::pin(async move {
                        let result = completion
                            .recv()
                            .await
                            .unwrap_or_else(|_| Err(String::from("fake operation cancelled")));
                        operations.finish_device(operation_id);
                        TaskResult::Operation { action, result }
                    })
                }
            }
        }
    }

    struct WorkerHarness {
        commands: Sender<WorkerCommand>,
        lifecycle: Arc<Mutex<LifecycleRequest>>,
        controls: Sender<WorkerControl>,
        _scan_controls: Sender<WorkerControl>,
        pairing_controls: Sender<WorkerControl>,
        agent_broker: Arc<Mutex<Option<AgentBroker>>>,
        shutdown: Sender<WorkerControl>,
        snapshots: Receiver<CoreSnapshot>,
        session_controls: Vec<FakeSessionControl>,
        thread: Option<std::thread::JoinHandle<()>>,
    }

    impl WorkerHarness {
        fn new(plans: Vec<Result<FakeSessionPlan, String>>) -> (Self, Arc<AtomicUsize>) {
            let (commands, command_rx) = async_channel::bounded(64);
            let (controls, control_rx) = async_channel::bounded(1);
            let (scan_controls, scan_control_rx) = async_channel::bounded(1);
            let (pairing_controls, pairing_control_rx) = async_channel::bounded(1);
            let (shutdown, shutdown_rx) = async_channel::bounded(1);
            let lifecycle = Arc::new(Mutex::new(LifecycleRequest::default()));
            let scan_owners = Arc::new(Mutex::new(ScanOwnerMailbox::default()));
            let (snapshots, snapshot_rx) = async_channel::bounded(32);
            let attempts = Arc::new(AtomicUsize::new(0));
            let request_ids = Arc::new(AgentRequestIds::default());
            let mut session_controls = Vec::new();
            let plans: Vec<Result<FakeSession, String>> = plans
                .into_iter()
                .map(|plan| match plan {
                    Ok(mut plan) => {
                        let broker = AgentBroker::new_with_request_ids(Arc::clone(&request_ids));
                        let agent_wake = broker.wake_receiver();
                        plan.session.broker = broker.clone();
                        plan.session.agent_wake = agent_wake.clone();
                        plan.control.broker = broker;
                        session_controls.push(plan.control.clone());
                        Ok(plan.session)
                    }
                    Err(error) => Err(error),
                })
                .collect();
            let transport = Arc::new(FakeTransport {
                plans: Arc::new(Mutex::new(VecDeque::from(plans))),
                attempts: Arc::clone(&attempts),
            });
            let thread_lifecycle = Arc::clone(&lifecycle);
            let thread_scan_owners = Arc::clone(&scan_owners);
            let agent_broker = Arc::new(Mutex::new(None));
            let thread_agent_broker = Arc::clone(&agent_broker);
            let thread = std::thread::spawn(move || {
                smol::block_on(run_worker_with_transport(
                    WorkerChannels {
                        commands: command_rx,
                        controls: control_rx,
                        scan_controls: scan_control_rx,
                        shutdown: shutdown_rx,
                        pairing_controls: pairing_control_rx,
                    },
                    thread_lifecycle,
                    thread_scan_owners,
                    thread_agent_broker,
                    transport,
                    Arc::new(move |snapshot| {
                        let _ = snapshots.try_send(snapshot);
                    }),
                ));
            });
            (
                Self {
                    commands,
                    lifecycle,
                    controls,
                    _scan_controls: scan_controls,
                    pairing_controls,
                    agent_broker,
                    shutdown,
                    snapshots: snapshot_rx,
                    session_controls,
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

        fn stop(&self) {
            let mut lifecycle = self.lifecycle.lock().expect("lifecycle lock");
            lifecycle.session_generation += 1;
            lifecycle.running = false;
            self.controls
                .try_send(WorkerControl::LifecycleChanged)
                .expect("lifecycle wake");
        }

        fn submit_agent_text(&self, request_id: u64, text: &str) -> AgentSubmitResult {
            self.agent_broker
                .lock()
                .expect("worker Agent broker lock")
                .as_ref()
                .map(|broker| broker.submit_text(request_id, text))
                .unwrap_or(AgentSubmitResult::Ignored)
        }

        fn pair(&self, object_path: &str) {
            let session_generation = self
                .lifecycle
                .lock()
                .expect("lifecycle lock")
                .session_generation;
            self.commands
                .try_send(WorkerCommand::Pair {
                    session_generation,
                    object_path: object_path.to_owned(),
                })
                .expect("pair command");
        }

        fn cancel_pairing(&self) {
            let session_generation = self
                .lifecycle
                .lock()
                .expect("lifecycle lock")
                .session_generation;
            self.pairing_controls
                .try_send(WorkerControl::CancelPairing { session_generation })
                .expect("pairing cancellation control");
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

        fn session(&self, index: usize) -> FakeSessionControl {
            self.session_controls[index].clone()
        }

        fn connect(&self, object_path: &str, connect: bool) {
            self.commands
                .try_send(WorkerCommand::Connect {
                    session_generation: 1,
                    object_path: object_path.to_owned(),
                    connect,
                })
                .expect("connect command");
        }

        fn drain_snapshots(&self) -> Vec<CoreSnapshot> {
            let mut snapshots = Vec::new();
            while let Ok(snapshot) = self.snapshots.try_recv() {
                snapshots.push(snapshot);
            }
            snapshots
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

    fn pending_session(signals: Vec<FakeSignal>) -> FakeSessionPlan {
        let (sender, receiver) = async_channel::bounded(32);
        for signal in signals {
            sender.try_send(signal).expect("initial fake signal");
        }
        let operations = OperationTracker::new();
        let broker = AgentBroker::new();
        let registration = AgentRegistrationTracker::default();
        let get_all_properties = Arc::new(Mutex::new(PropertyMap::new()));
        let authority = Arc::new(Mutex::new((0, 0)));
        let prompt_override = Arc::new(Mutex::new(None));
        let prompt_reads = Arc::new(AtomicUsize::new(0));
        let agent_wake = broker.wake_receiver();
        let control = FakeSessionControl {
            signals: sender,
            operations: Arc::clone(&operations),
            broker: broker.clone(),
            registration: registration.clone(),
            get_all_properties: Arc::clone(&get_all_properties),
            authority: Arc::clone(&authority),
            prompt_override: Arc::clone(&prompt_override),
            prompt_reads: Arc::clone(&prompt_reads),
        };
        FakeSessionPlan {
            session: FakeSession {
                owner: Some(String::from(":1.42")),
                owner_error: None,
                signals: receiver,
                agent_wake,
                probe_objects: test_managed_objects(&["/org/bluez/hci0/dev_AA"], false),
                operations,
                broker,
                registration,
                get_all_properties,
                authority,
                prompt_override,
                prompt_reads,
            },
            control,
        }
    }

    fn session_plan(
        owner: Option<&str>,
        owner_error: Option<&str>,
        devices: &[&str],
    ) -> FakeSessionPlan {
        session_plan_with_pairing(owner, owner_error, devices, true)
    }

    fn session_plan_with_pairing(
        owner: Option<&str>,
        owner_error: Option<&str>,
        devices: &[&str],
        paired: bool,
    ) -> FakeSessionPlan {
        let (sender, receiver) = async_channel::bounded(32);
        let operations = OperationTracker::new();
        let broker = AgentBroker::new();
        let registration = AgentRegistrationTracker::default();
        let get_all_properties = Arc::new(Mutex::new(PropertyMap::new()));
        let authority = Arc::new(Mutex::new((0, 0)));
        let prompt_override = Arc::new(Mutex::new(None));
        let prompt_reads = Arc::new(AtomicUsize::new(0));
        let agent_wake = broker.wake_receiver();
        let control = FakeSessionControl {
            signals: sender,
            operations: Arc::clone(&operations),
            broker: broker.clone(),
            registration: registration.clone(),
            get_all_properties: Arc::clone(&get_all_properties),
            authority: Arc::clone(&authority),
            prompt_override: Arc::clone(&prompt_override),
            prompt_reads: Arc::clone(&prompt_reads),
        };
        FakeSessionPlan {
            session: FakeSession {
                owner: owner.map(str::to_owned),
                owner_error: owner_error.map(str::to_owned),
                signals: receiver,
                agent_wake,
                probe_objects: test_managed_objects_with_pairing(devices, false, paired),
                operations,
                broker,
                registration,
                get_all_properties,
                authority,
                prompt_override,
                prompt_reads,
            },
            control,
        }
    }

    fn test_managed_objects(devices: &[&str], connected: bool) -> BTreeMap<String, InterfaceMap> {
        test_managed_objects_with_pairing(devices, connected, true)
    }

    fn test_managed_objects_with_pairing(
        devices: &[&str],
        connected: bool,
        paired: bool,
    ) -> BTreeMap<String, InterfaceMap> {
        let mut objects = BTreeMap::new();
        objects.insert(
            String::from("/org/bluez/hci0"),
            BTreeMap::from([(
                String::from("org.bluez.Adapter1"),
                BTreeMap::from([
                    (
                        String::from("Alias"),
                        PropertyValue::String(String::from("Adapter")),
                    ),
                    (String::from("Powered"), PropertyValue::Boolean(true)),
                    (String::from("Discovering"), PropertyValue::Boolean(false)),
                ]),
            )]),
        );
        for path in devices {
            objects.insert(
                (*path).to_owned(),
                BTreeMap::from([(
                    String::from("org.bluez.Device1"),
                    BTreeMap::from([
                        (
                            String::from("Address"),
                            PropertyValue::String(String::from("AA:BB")),
                        ),
                        (
                            String::from("Alias"),
                            PropertyValue::String((*path).to_owned()),
                        ),
                        (String::from("Paired"), PropertyValue::Boolean(paired)),
                        (String::from("Trusted"), PropertyValue::Boolean(false)),
                        (String::from("Connected"), PropertyValue::Boolean(connected)),
                    ]),
                )]),
            );
        }
        objects
    }

    fn name_owner_changed(old_owner: &str, new_owner: &str) -> Message {
        Message::signal(
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "NameOwnerChanged",
        )
        .expect("name-owner signal builder")
        .build(&(
            String::from("org.bluez"),
            old_owner.to_owned(),
            new_owner.to_owned(),
        ))
        .expect("name-owner signal")
    }

    fn properties_changed(path: &str, owner: &str, connected: bool) -> Message {
        let changed = std::collections::HashMap::from([(
            String::from("Connected"),
            zbus::zvariant::OwnedValue::from(connected),
        )]);
        Message::signal(path, "org.freedesktop.DBus.Properties", "PropertiesChanged")
            .expect("properties signal builder")
            .sender(owner)
            .expect("properties signal sender")
            .build(&(
                String::from("org.bluez.Device1"),
                changed,
                Vec::<String>::new(),
            ))
            .expect("properties signal")
    }

    fn properties_invalidated(path: &str, owner: &str, property: &str) -> Message {
        Message::signal(path, "org.freedesktop.DBus.Properties", "PropertiesChanged")
            .expect("properties signal builder")
            .sender(owner)
            .expect("properties signal sender")
            .build(&(
                String::from("org.bluez.Device1"),
                std::collections::HashMap::<String, zbus::zvariant::OwnedValue>::new(),
                vec![property.to_owned()],
            ))
            .expect("properties signal")
    }

    fn ready_worker(devices: &[&str]) -> (WorkerHarness, FakeSessionControl) {
        let harness = WorkerHarness::new(vec![Ok(session_plan(Some(":1.42"), None, devices))]).0;
        let session = harness.session(0);
        harness.start();
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        (harness, session)
    }

    fn ready_unpaired_worker(devices: &[&str]) -> (WorkerHarness, FakeSessionControl) {
        let harness = WorkerHarness::new(vec![Ok(session_plan_with_pairing(
            Some(":1.42"),
            None,
            devices,
            false,
        ))])
        .0;
        let session = harness.session(0);
        harness.start();
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        (harness, session)
    }

    fn ready_core() -> BluetoothCore {
        let mut core = BluetoothCore::default();
        assert!(core.start_generation(1));
        let actions = core.owner_changed(1, Some(String::from(":1.42")));
        assert!(matches!(actions.as_slice(), [CoreAction::Probe { .. }]));
        core.managed_objects(
            1,
            1,
            test_managed_objects(&["/org/bluez/hci0/dev_AA"], false),
        );
        core
    }

    fn ready_unpaired_core() -> BluetoothCore {
        let mut core = BluetoothCore::default();
        assert!(core.start_generation(1));
        let actions = core.owner_changed(1, Some(String::from(AGENT_OWNER)));
        assert!(matches!(actions.as_slice(), [CoreAction::Probe { .. }]));
        core.managed_objects(
            1,
            1,
            test_managed_objects_with_pairing(&[AGENT_DEVICE], false, false),
        );
        core
    }

    fn saturated_action_queue() -> QueuedActions {
        QueuedActions {
            ordinary: (0..TASK_QUEUE_CAPACITY)
                .map(|session_generation| CoreAction::StartBus {
                    session_generation: session_generation as u64,
                })
                .collect(),
            cancellation: None,
        }
    }

    fn wait_until(predicate: impl Fn() -> bool) {
        let deadline = Instant::now() + Duration::from_secs(2);
        while !predicate() {
            assert!(
                Instant::now() < deadline,
                "timed out waiting for worker state"
            );
            std::thread::sleep(Duration::from_millis(5));
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
        apply_scan_owner_update(
            &scan_owners,
            &mut core,
            &mut QueuedActions::default(),
            &|_| {},
        );
        assert_eq!(core.scan_owners().len(), 2);

        update_scan_owner_release(&scan_owners, &scan_controls, 1, String::from("topbar"));
        update_scan_owner_release(
            &scan_owners,
            &scan_controls,
            1,
            String::from("bluetooth-popup"),
        );
        apply_scan_owner_update(
            &scan_owners,
            &mut core,
            &mut QueuedActions::default(),
            &|_| {},
        );
        assert!(core.scan_owners().is_empty());
    }

    #[test]
    fn generation_replacement_drops_old_tasks_before_new_probe() {
        let mut tasks = futures::stream::FuturesUnordered::<TaskFuture>::new();
        for _ in 0..TASK_LIMIT {
            tasks.push(Box::pin(futures::future::pending::<TaskResult>()));
        }
        let mut queued_actions = QueuedActions::default();
        queued_actions.ordinary.push_back(CoreAction::Probe {
            session_generation: 1,
            bluez_generation: 1,
            owner: String::from(":1.42"),
        });

        retire_generation_work(&mut tasks, &mut queued_actions, &mut BTreeSet::new());

        assert!(tasks.is_empty());
        assert!(queued_actions.ordinary.is_empty());
        assert!(queued_actions.cancellation.is_none());

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
    fn worker_name_owner_change_retains_and_executes_replacement_probe() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        session.operations.wait_for_dispatches(1);

        session
            .signals
            .try_send(FakeSignal::Message(name_owner_changed(":1.42", ":1.84")))
            .expect("owner-change signal");
        session.operations.wait_for_probes(2);
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);

        assert_eq!(session.operations.probes.load(Ordering::SeqCst), 2);
        harness.shutdown();
    }

    #[test]
    fn worker_bluez_disappearance_waits_without_probe_then_probes_on_return() {
        let (harness, session) = ready_worker(&[]);
        session
            .signals
            .try_send(FakeSignal::Message(name_owner_changed(":1.42", "")))
            .expect("owner disappearance signal");
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        assert_eq!(session.operations.probes.load(Ordering::SeqCst), 1);

        session
            .signals
            .try_send(FakeSignal::Message(name_owner_changed("", ":1.84")))
            .expect("owner appearance signal");
        session.operations.wait_for_probes(2);
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        harness.shutdown();
    }

    #[test]
    fn same_device_connect_then_disconnect_never_dispatches_concurrently() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        session.operations.wait_for_dispatches(1);
        let first = session.operations.dispatched()[0].operation_id;
        harness.connect("/org/bluez/hci0/dev_AA", false);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(session.operations.dispatched().len(), 1);

        session.operations.complete(first, Ok(()));
        session.operations.wait_for_dispatches(2);
        let dispatched = session.operations.dispatched();
        assert!(!dispatched[1].connect);
        assert_eq!(session.operations.max_active(), 1);
        harness.shutdown();
    }

    #[test]
    fn same_device_disconnect_then_connect_never_dispatches_concurrently() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", false);
        session.operations.wait_for_dispatches(1);
        let first = session.operations.dispatched()[0].operation_id;
        harness.connect("/org/bluez/hci0/dev_AA", true);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(session.operations.dispatched().len(), 1);

        session.operations.complete(first, Ok(()));
        session.operations.wait_for_dispatches(2);
        let dispatched = session.operations.dispatched();
        assert!(dispatched[1].connect);
        assert_eq!(session.operations.max_active(), 1);
        harness.shutdown();
    }

    #[test]
    fn same_device_three_operations_coalesce_to_latest_intent() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        session.operations.wait_for_dispatches(1);
        let first = session.operations.dispatched()[0].operation_id;
        harness.connect("/org/bluez/hci0/dev_AA", false);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(session.operations.dispatched().len(), 1);

        session.operations.complete(first, Ok(()));
        session.operations.wait_for_dispatches(2);
        let dispatched = session.operations.dispatched();
        assert!(dispatched[1].connect);
        assert_eq!(session.operations.max_active(), 1);
        harness.shutdown();
    }

    #[test]
    fn different_devices_retain_concurrent_dispatch() {
        let (harness, session) =
            ready_worker(&["/org/bluez/hci0/dev_AA", "/org/bluez/hci0/dev_BB"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        harness.connect("/org/bluez/hci0/dev_BB", true);
        session.operations.wait_for_dispatches(2);
        assert_eq!(session.operations.max_active(), 2);

        for operation in session.operations.dispatched() {
            session.operations.complete(operation.operation_id, Ok(()));
        }
        harness.shutdown();
    }

    #[test]
    fn worker_exports_agent_before_registering_and_pairing() {
        let (harness, session) = ready_unpaired_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.pair("/org/bluez/hci0/dev_AA");
        session.operations.wait_for_event("pair");
        let events = session.operations.events();
        let export = events
            .iter()
            .position(|event| *event == "agent_exported")
            .expect("agent export event");
        let register = events
            .iter()
            .position(|event| *event == "register_agent")
            .expect("agent registration event");
        let pair = events
            .iter()
            .position(|event| *event == "pair")
            .expect("pair event");
        assert!(export < register);
        assert!(register < pair);
        session.operations.complete_pair();
        harness.shutdown();
    }

    #[test]
    fn worker_release_invalidates_registration_and_next_pair_registers_before_pair() {
        let (harness, session) = ready_unpaired_worker(&[AGENT_DEVICE]);
        harness.pair(AGENT_DEVICE);
        session.operations.wait_for_event_count("pair", 1);
        assert!(session.registration.is_registered(1, 1, AGENT_OWNER, 1));

        session
            .release_from_bluez(1, 1)
            .expect("authorized BlueZ Release");
        assert!(!session.registration.is_registered(1, 1, AGENT_OWNER, 1));
        session
            .operations
            .finish_pair(Err(String::from("Agent was released")));
        let snapshot = harness.wait_for(|snapshot| !snapshot.pairing);
        assert!(!snapshot.pairing);

        harness.pair(AGENT_DEVICE);
        session.operations.wait_for_event_count("pair", 2);
        let events = session.operations.events();
        let registrations: Vec<_> = events
            .iter()
            .enumerate()
            .filter_map(|(index, event)| (*event == "register_agent").then_some(index))
            .collect();
        let pairs: Vec<_> = events
            .iter()
            .enumerate()
            .filter_map(|(index, event)| (*event == "pair").then_some(index))
            .collect();
        assert_eq!(registrations.len(), 2);
        assert_eq!(pairs.len(), 2);
        assert!(registrations[1] < pairs[1]);
        harness.shutdown();
    }

    #[test]
    fn worker_agent_request_ids_survive_bus_reconnect_and_service_restart() {
        let mut first = pending_session(Vec::new());
        first.session.probe_objects =
            test_managed_objects_with_pairing(&[AGENT_DEVICE], false, false);
        let second = session_plan_with_pairing(Some(AGENT_OWNER), None, &[AGENT_DEVICE], false);
        let third = session_plan_with_pairing(Some(AGENT_OWNER), None, &[AGENT_DEVICE], false);
        let (harness, attempts) = WorkerHarness::new(vec![Ok(first), Ok(second), Ok(third)]);
        let old_session = harness.session(0);
        harness.start();
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        harness.pair(AGENT_DEVICE);
        old_session.operations.wait_for_event("pair");
        let _ = old_session.start_passkey_request();
        let old_prompt = harness.wait_for(|snapshot| snapshot.agent_request_active);
        assert_eq!(old_prompt.agent_request_id, old_session.prompt().request_id);

        old_session
            .signals
            .try_send(FakeSignal::End)
            .expect("system bus disconnect");
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        let reconnected = harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Ready
                && attempts.load(Ordering::SeqCst) >= 2
        });
        assert!(reconnected.ready);
        let new_session = harness.session(1);
        harness.pair(AGENT_DEVICE);
        new_session.operations.wait_for_event("pair");
        let _ = new_session.start_passkey_request();
        let new_prompt = harness.wait_for(|snapshot| snapshot.agent_request_active);
        assert_ne!(old_prompt.agent_request_id, new_prompt.agent_request_id);
        assert_eq!(
            harness.submit_agent_text(old_prompt.agent_request_id, "123456"),
            AgentSubmitResult::Ignored
        );
        assert_eq!(
            harness.submit_agent_text(new_prompt.agent_request_id, "123456"),
            AgentSubmitResult::Accepted
        );
        harness.cancel_pairing();
        harness.wait_for(|snapshot| !snapshot.pairing);

        harness.stop();
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Stopped);
        harness.start();
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Ready
                && snapshot.session_generation == 3
        });
        let restarted = harness.session(2);
        harness.pair(AGENT_DEVICE);
        restarted.operations.wait_for_event("pair");
        let _ = restarted.start_passkey_request();
        let after_restart = harness.wait_for(|snapshot| snapshot.agent_request_active);
        assert_ne!(new_prompt.agent_request_id, after_restart.agent_request_id);
        assert_eq!(
            harness.submit_agent_text(new_prompt.agent_request_id, "123456"),
            AgentSubmitResult::Ignored
        );
        assert_eq!(
            harness.submit_agent_text(after_restart.agent_request_id, "123456"),
            AgentSubmitResult::Accepted
        );
        harness.cancel_pairing();
        harness.shutdown();
    }

    #[test]
    fn worker_get_all_paired_convergence_clears_broker_and_rejects_retired_prompt() {
        let (harness, session) = ready_unpaired_worker(&[AGENT_DEVICE]);
        harness.pair(AGENT_DEVICE);
        session.operations.wait_for_event("pair");
        let _ = session.start_passkey_request();
        let prompt = harness.wait_for(|snapshot| snapshot.agent_request_active);
        assert!(prompt.pairing);

        session.set_get_all_paired(true);
        session.emit(properties_invalidated(AGENT_DEVICE, AGENT_OWNER, "Paired"));
        let converged = harness.wait_for(|snapshot| !snapshot.pairing);
        assert!(!converged.agent_request_active);
        assert!(!session.prompt().active);

        session.publish_prompt_and_wait(AgentPromptView {
            active: true,
            request_id: prompt.agent_request_id,
            pairing_epoch: 1,
            session_generation: 1,
            bluez_generation: 1,
            kind: AgentPromptKind::PasskeyInput,
            device_path: AGENT_DEVICE.to_owned(),
            ..AgentPromptView::default()
        });
        assert!(
            harness
                .drain_snapshots()
                .iter()
                .all(|snapshot| !snapshot.agent_request_active)
        );
        assert_eq!(
            harness.submit_agent_text(prompt.agent_request_id, "123456"),
            AgentSubmitResult::Ignored
        );
        harness.shutdown();
    }

    #[test]
    fn cancel_pairing_dispatches_while_pair_is_in_flight() {
        let (harness, session) = ready_unpaired_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.pair("/org/bluez/hci0/dev_AA");
        session.operations.wait_for_event("pair");
        harness.cancel_pairing();
        session.operations.wait_for_event("cancel_pairing");
        session.operations.complete_pair();
        harness.shutdown();
    }

    #[test]
    fn request_ids_survive_bus_replacement_and_service_restart() {
        let request_ids = Arc::new(AgentRequestIds::default());
        let old_broker = AgentBroker::new_with_request_ids(Arc::clone(&request_ids));
        old_broker.set_authority(1, 1, AGENT_OWNER);
        old_broker.set_pairing_context(1, 1, 1, AGENT_DEVICE);
        let old_request_broker = old_broker.clone();
        let old_request = smol::spawn(async move {
            old_request_broker
                .request_passkey(AGENT_OWNER, 1, 1, AGENT_DEVICE)
                .await
        });
        let old_prompt = wait_for_agent_prompt(&old_broker);
        old_broker.cancel_for_lifecycle();
        assert_eq!(
            smol::block_on(old_request),
            Err(crate::bluetooth::agent::AgentError::Canceled)
        );

        let reconnected_broker = AgentBroker::new_with_request_ids(Arc::clone(&request_ids));
        reconnected_broker.set_authority(1, 2, AGENT_OWNER);
        reconnected_broker.set_pairing_context(2, 1, 2, AGENT_DEVICE);
        let new_request_broker = reconnected_broker.clone();
        let new_request = smol::spawn(async move {
            new_request_broker
                .request_passkey(AGENT_OWNER, 1, 2, AGENT_DEVICE)
                .await
        });
        let reconnected_prompt = wait_for_agent_prompt(&reconnected_broker);
        assert_ne!(old_prompt.request_id, reconnected_prompt.request_id);
        assert_eq!(
            reconnected_broker.submit_text(old_prompt.request_id, "1234"),
            crate::bluetooth::agent::AgentSubmitResult::Ignored
        );
        assert_eq!(
            reconnected_broker.submit_text(reconnected_prompt.request_id, "004201"),
            crate::bluetooth::agent::AgentSubmitResult::Accepted
        );
        assert_eq!(smol::block_on(new_request), Ok(4201));

        let restarted_broker = AgentBroker::new_with_request_ids(request_ids);
        restarted_broker.set_authority(2, 1, AGENT_OWNER);
        restarted_broker.set_pairing_context(1, 2, 1, AGENT_DEVICE);
        let restarted_request_broker = restarted_broker.clone();
        let restarted_request = smol::spawn(async move {
            restarted_request_broker
                .request_passkey(AGENT_OWNER, 2, 1, AGENT_DEVICE)
                .await
        });
        let restarted_prompt = wait_for_agent_prompt(&restarted_broker);
        assert_ne!(reconnected_prompt.request_id, restarted_prompt.request_id);
        assert_eq!(
            restarted_broker.submit_text(restarted_prompt.request_id, "5678"),
            crate::bluetooth::agent::AgentSubmitResult::Accepted
        );
        assert_eq!(smol::block_on(restarted_request), Ok(5678));
    }

    fn wait_for_agent_prompt(broker: &AgentBroker) -> AgentPromptView {
        let wake = broker.wake_receiver();
        smol::block_on(async {
            loop {
                let prompt = broker.prompt();
                if prompt.active {
                    return prompt;
                }
                wake.recv().await.expect("AgentBroker wake");
            }
        })
    }

    #[test]
    fn authorized_release_invalidates_registration_before_the_next_pair() {
        let registration = AgentRegistrationTracker::default();
        let (already_registered, first_epoch) = registration
            .begin(1, 1, AGENT_OWNER)
            .expect("first registration epoch");
        assert!(!already_registered);
        assert!(registration.mark_registered(1, 1, AGENT_OWNER, first_epoch));

        let broker = AgentBroker::new();
        broker.set_authority(1, 1, AGENT_OWNER);
        broker.set_pairing_context(1, 1, 1, AGENT_DEVICE);
        broker
            .display_passkey(AGENT_OWNER, 1, 1, AGENT_DEVICE, 123456, 0)
            .expect("display prompt");
        let release_registration = registration.clone();
        let agent = Agent1::with_release_hook(
            broker.clone(),
            AgentReleaseHook::new(move |session, bluez| {
                release_registration.release(session, bluez);
            }),
        );
        assert!(registration.is_registered(1, 1, AGENT_OWNER, first_epoch));

        assert_eq!(
            agent.release_from_bluez(":1.99", 1, 1),
            Err(crate::bluetooth::agent::AgentError::Rejected)
        );
        assert!(registration.is_registered(1, 1, AGENT_OWNER, first_epoch));
        assert!(broker.prompt().active);

        agent
            .release_from_bluez(AGENT_OWNER, 1, 1)
            .expect("authorized Release");
        assert!(!registration.is_registered(1, 1, AGENT_OWNER, first_epoch));
        assert!(!broker.prompt().active);

        let (already_registered, next_epoch) = registration
            .begin(1, 1, AGENT_OWNER)
            .expect("registration after Release");
        assert!(!already_registered);
        assert_ne!(next_epoch, first_epoch);
        assert!(registration.mark_registered(1, 1, AGENT_OWNER, next_epoch));
    }

    #[test]
    fn cancellation_side_slot_dispatches_while_pair_and_ordinary_scheduler_are_full() {
        let mut core = ready_unpaired_core();
        let register = core.pair_device(AGENT_DEVICE).expect("Pair request");
        let CoreAction::RegisterAgent {
            operation_id,
            pairing_epoch,
            ..
        } = register
        else {
            panic!("agent registration action");
        };
        let pair = core
            .agent_registered(1, 1, operation_id, pairing_epoch)
            .expect("registered Pair action");
        assert!(core.snapshot().pairing);

        let plan = session_plan_with_pairing(Some(AGENT_OWNER), None, &[AGENT_DEVICE], false);
        let operations = Arc::clone(&plan.control.operations);
        let session: BusSessionHandle = Arc::new(plan.session);
        let ordinary_tasks = FuturesUnordered::<TaskFuture>::new();
        ordinary_tasks.push(session.execute(pair));
        for _ in 1..TASK_LIMIT {
            ordinary_tasks.push(Box::pin(futures::future::pending::<TaskResult>()));
        }
        assert_eq!(ordinary_tasks.len(), TASK_LIMIT);
        assert!(operations.events().contains(&"pair"));

        let cancel = core.cancel_pairing().expect("active Pair cancellation");
        let mut queued = saturated_action_queue();
        enqueue_actions([cancel], &mut queued, &mut core, &|_| {});
        assert_eq!(queued.ordinary.len(), TASK_QUEUE_CAPACITY);
        assert!(queued.cancellation.is_some());

        let mut cancellation_tasks = FuturesUnordered::<TaskFuture>::new();
        dispatch_pairing_cancellation(
            &mut queued,
            &mut cancellation_tasks,
            Some(&session),
            &core,
            AGENT_OWNER,
        );

        assert!(operations.events().contains(&"cancel_pairing"));
        assert_eq!(ordinary_tasks.len(), TASK_LIMIT);
        assert_eq!(cancellation_tasks.len(), 1);
    }

    #[test]
    fn pairing_timeout_cancellation_survives_ordinary_queue_saturation() {
        assert!(PAIRING_DBUS_TIMEOUT > PAIRING_TIMEOUT);
        let mut core = ready_unpaired_core();
        let register = core.pair_device(AGENT_DEVICE).expect("Pair request");
        let CoreAction::RegisterAgent {
            operation_id,
            pairing_epoch,
            ..
        } = register
        else {
            panic!("agent registration action");
        };
        let pair = core
            .agent_registered(1, 1, operation_id, pairing_epoch)
            .expect("registered Pair action");

        let plan = session_plan_with_pairing(Some(AGENT_OWNER), None, &[AGENT_DEVICE], false);
        let operations = Arc::clone(&plan.control.operations);
        let session: BusSessionHandle = Arc::new(plan.session);
        let ordinary_tasks = FuturesUnordered::<TaskFuture>::new();
        ordinary_tasks.push(session.execute(pair));
        for _ in 1..TASK_LIMIT {
            ordinary_tasks.push(Box::pin(futures::future::pending::<TaskResult>()));
        }

        let timed_out = core.on_timer(Instant::now() + PAIRING_TIMEOUT);
        assert!(!core.snapshot().pairing);
        assert_eq!(
            core.snapshot().pairing_error.as_deref(),
            Some("Bluetooth pairing timed out")
        );
        let mut queued = saturated_action_queue();
        enqueue_actions(timed_out, &mut queued, &mut core, &|_| {});
        assert_eq!(queued.ordinary.len(), TASK_QUEUE_CAPACITY);

        let mut cancellation_tasks = FuturesUnordered::<TaskFuture>::new();
        dispatch_pairing_cancellation(
            &mut queued,
            &mut cancellation_tasks,
            Some(&session),
            &core,
            AGENT_OWNER,
        );

        assert!(operations.events().contains(&"cancel_pairing"));
        assert_eq!(ordinary_tasks.len(), TASK_LIMIT);
        assert_eq!(cancellation_tasks.len(), 1);
    }

    #[test]
    fn old_generation_cancellation_is_not_sent_to_the_replacement_owner() {
        let mut core = ready_unpaired_core();
        let _ = core.pair_device(AGENT_DEVICE).expect("Pair request");
        let cancel = core.cancel_pairing().expect("active Pair cancellation");
        let plan = session_plan_with_pairing(Some(AGENT_OWNER), None, &[AGENT_DEVICE], false);
        let operations = Arc::clone(&plan.control.operations);
        let session: BusSessionHandle = Arc::new(plan.session);
        let mut queued = saturated_action_queue();
        enqueue_actions([cancel], &mut queued, &mut core, &|_| {});

        let _ = core.owner_changed(1, Some(String::from(":1.84")));
        let mut cancellation_tasks = FuturesUnordered::<TaskFuture>::new();
        dispatch_pairing_cancellation(
            &mut queued,
            &mut cancellation_tasks,
            Some(&session),
            &core,
            ":1.84",
        );

        assert!(!operations.events().contains(&"cancel_pairing"));
        assert!(cancellation_tasks.is_empty());
    }

    #[test]
    fn stale_device_completion_cannot_degrade_newer_worker_state() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        session.operations.wait_for_dispatches(1);
        let first = session.operations.dispatched()[0].operation_id;
        harness.connect("/org/bluez/hci0/dev_AA", false);
        session
            .operations
            .complete(first, Err(String::from("stale connect failure")));
        session.operations.wait_for_dispatches(2);
        assert!(
            harness
                .drain_snapshots()
                .into_iter()
                .all(|snapshot| snapshot.error.as_deref() != Some("stale connect failure"))
        );
        harness.shutdown();
    }

    #[test]
    fn authoritative_device_state_converges_current_operation() {
        let (harness, session) = ready_worker(&["/org/bluez/hci0/dev_AA"]);
        harness.connect("/org/bluez/hci0/dev_AA", true);
        session.operations.wait_for_dispatches(1);
        let operation = session.operations.dispatched()[0].operation_id;
        session.operations.complete(operation, Ok(()));
        session
            .signals
            .try_send(FakeSignal::Message(properties_changed(
                "/org/bluez/hci0/dev_AA",
                ":1.42",
                true,
            )))
            .expect("connected property signal");
        let snapshot = harness.wait_for(|snapshot| snapshot.connected_count == 1);
        assert!(snapshot.devices[0].connected);
        harness.shutdown();
    }

    #[test]
    fn queue_full_connect_failure_preserves_newer_operation_identity() {
        let mut core = ready_core();
        let first = core
            .connect_device("/org/bluez/hci0/dev_AA", true)
            .expect("first operation");
        let replacement = core
            .connect_device("/org/bluez/hci0/dev_AA", false)
            .expect("replacement operation");
        let mut queued = saturated_action_queue();
        enqueue_actions([first], &mut queued, &mut core, &|_| {});
        assert_eq!(
            core.snapshot().state,
            crate::bluetooth::engine::ServiceState::Ready
        );
        assert_eq!(core.snapshot().error, None);
        assert_eq!(core.pending_device_operations(), 1);
        let CoreAction::Connect {
            session_generation,
            bluez_generation,
            operation_id,
            object_path,
            connect,
            ..
        } = replacement
        else {
            unreachable!();
        };
        assert!(!core.connect_reply(
            session_generation,
            bluez_generation,
            operation_id,
            &object_path,
            connect,
            true,
            None,
        ));
    }

    #[test]
    fn queue_full_current_connect_failure_reports_bounded_queue_error() {
        let mut core = ready_core();
        let action = core
            .connect_device("/org/bluez/hci0/dev_AA", true)
            .expect("operation");
        let mut queued = saturated_action_queue();
        enqueue_actions([action], &mut queued, &mut core, &|_| {});
        assert_eq!(
            core.snapshot().error.as_deref(),
            Some("Bluetooth backend command queue is full")
        );
        assert_eq!(core.pending_device_operations(), 0);
    }

    #[test]
    fn no_bluez_owner_is_healthy_and_does_not_retry_connection() {
        let (harness, attempts) = WorkerHarness::new(vec![Ok(session_plan(None, None, &[]))]);
        harness.start();
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        std::thread::sleep(Duration::from_millis(650));
        assert_eq!(attempts.load(Ordering::SeqCst), 1);
        harness.shutdown();
    }

    #[test]
    fn owner_lookup_failure_retries_and_successful_retry_probes() {
        let (harness, attempts) = WorkerHarness::new(vec![
            Ok(session_plan(Some(":1.42"), Some("lookup failed"), &[])),
            Ok(session_plan(Some(":1.84"), None, &[])),
        ]);
        harness.start();
        harness.wait_for(|snapshot| {
            snapshot.state == crate::bluetooth::engine::ServiceState::Unavailable
        });
        wait_until(|| attempts.load(Ordering::SeqCst) == 2);
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        harness.shutdown();
    }

    #[test]
    fn owner_lookup_success_probes_without_retry() {
        let (harness, attempts) =
            WorkerHarness::new(vec![Ok(session_plan(Some(":1.42"), None, &[]))]);
        harness.start();
        harness
            .wait_for(|snapshot| snapshot.state == crate::bluetooth::engine::ServiceState::Ready);
        std::thread::sleep(Duration::from_millis(20));
        assert_eq!(attempts.load(Ordering::SeqCst), 1);
        harness.shutdown();
    }

    #[test]
    fn production_drop_does_not_join_worker_thread() {
        let source = include_str!("client.rs");
        let production = source
            .split("#[cfg(test)]")
            .next()
            .expect("production source");
        assert!(!production.contains("thread.join()"));
    }

    #[test]
    fn initial_connection_failure_recovers_with_a_fresh_connection() {
        let (harness, attempts) = WorkerHarness::new(vec![
            Err(String::from("system bus unavailable")),
            Ok(pending_session(Vec::new())),
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
                Ok(pending_session(Vec::new())),
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
        let (stopped, attempts) = WorkerHarness::new(vec![Ok(pending_session(Vec::new()))]);
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
    fn dropping_worker_wakes_the_worker_thread() {
        let started = Instant::now();
        let worker = BluetoothWorker::spawn(|_| {}).expect("worker thread");
        drop(worker);
        assert!(started.elapsed() < Duration::from_millis(200));
    }
}
