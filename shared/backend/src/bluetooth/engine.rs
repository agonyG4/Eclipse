use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::fmt;
use std::time::Instant;

use super::agent::{AgentPromptKind, AgentPromptView};
use super::device::{AdapterInfo, BluetoothDevice, adapters, project_devices, select_adapter};
use super::discovery::{DiscoveryCommand, DiscoveryReply, DiscoveryState};
use super::object_store::{InterfaceMap, InterfaceRefreshToken, ObjectStore, PropertyMap};
use super::operations::{
    DeviceOperationCompletion, DeviceOperationState, OperationIds, PendingDeviceOperation,
    PowerState, PowerUpdate,
};
use super::pairing::{
    DeviceOperationIdentity, ForgetCompletion, ForgetState, PairingCompletion, PairingIdentity,
    PairingState, TrustCompletion, TrustState,
};

const INITIAL_SNAPSHOT_EVENT_CAPACITY: usize = 128;

#[derive(Clone, Debug)]
enum BluezEvent {
    InterfacesAdded {
        path: String,
        interfaces: InterfaceMap,
    },
    InterfacesRemoved {
        path: String,
        interfaces: Vec<String>,
    },
    PropertiesChanged {
        path: String,
        interface: String,
        changed: PropertyMap,
        invalidated: Vec<String>,
    },
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ServiceState {
    #[default]
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
}

#[derive(Clone, Default, PartialEq, Eq)]
pub struct CoreSnapshot {
    pub session_generation: u64,
    pub state: ServiceState,
    pub available: bool,
    pub ready: bool,
    pub adapter_available: bool,
    pub adapter_path: String,
    pub adapter_name: String,
    pub powered: bool,
    pub power_pending: bool,
    pub scanning: bool,
    pub connected_count: i32,
    pub connected_name: String,
    pub error: Option<String>,
    pub devices: Vec<BluetoothDevice>,
    pub pairing: bool,
    pub pairing_device_path: String,
    pub pairing_device_name: String,
    pub pairing_error: Option<String>,
    pub agent_request_active: bool,
    pub agent_request_id: u64,
    pub agent_request_kind: AgentPromptKind,
    pub agent_device_path: String,
    pub agent_device_name: String,
    pub agent_passkey: Option<u32>,
    pub agent_entered: Option<u16>,
    pub agent_service_uuid: Option<String>,
    pub agent_display_pin: Option<String>,
    pub operation_error: Option<String>,
}

impl fmt::Debug for CoreSnapshot {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("CoreSnapshot")
            .field("session_generation", &self.session_generation)
            .field("state", &self.state)
            .field("available", &self.available)
            .field("ready", &self.ready)
            .field("adapter_available", &self.adapter_available)
            .field("adapter_path", &self.adapter_path)
            .field("adapter_name", &self.adapter_name)
            .field("powered", &self.powered)
            .field("power_pending", &self.power_pending)
            .field("scanning", &self.scanning)
            .field("connected_count", &self.connected_count)
            .field("connected_name", &self.connected_name)
            .field("error", &self.error)
            .field("devices", &self.devices)
            .field("pairing", &self.pairing)
            .field("pairing_device_path", &self.pairing_device_path)
            .field("pairing_device_name", &self.pairing_device_name)
            .field("pairing_error", &self.pairing_error)
            .field("agent_request_active", &self.agent_request_active)
            .field("agent_request_id", &self.agent_request_id)
            .field("agent_request_kind", &self.agent_request_kind)
            .field("agent_device_path", &self.agent_device_path)
            .field("agent_device_name", &self.agent_device_name)
            .field("has_agent_passkey", &self.agent_passkey.is_some())
            .field("agent_entered", &self.agent_entered)
            .field("agent_service_uuid", &self.agent_service_uuid)
            .field("has_agent_display_pin", &self.agent_display_pin.is_some())
            .field("operation_error", &self.operation_error)
            .finish()
    }
}

impl CoreSnapshot {
    pub fn can_connect(&self, object_path: &str) -> bool {
        self.available
            && self
                .devices
                .iter()
                .any(|device| device.object_path == object_path && device.paired)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum CoreAction {
    StartBus {
        session_generation: u64,
    },
    Probe {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
    },
    SetPowered {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        adapter_path: String,
        target: bool,
    },
    Discovery {
        session_generation: u64,
        bluez_generation: u64,
        operation: DiscoveryCommand,
        owner: String,
        adapter_path: String,
    },
    Connect {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        object_path: String,
        connect: bool,
    },
    RegisterAgent {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        pairing_epoch: u64,
        device_path: String,
    },
    Pair {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        pairing_epoch: u64,
        device_path: String,
    },
    CancelPairing {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        pairing_epoch: u64,
        device_path: String,
    },
    SetTrusted {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        object_path: String,
        target: bool,
    },
    RemoveDevice {
        session_generation: u64,
        bluez_generation: u64,
        owner: String,
        operation_id: u64,
        adapter_path: String,
        object_path: String,
    },
    GetAll {
        session_generation: u64,
        owner: String,
        token: InterfaceRefreshToken,
    },
}

#[derive(Default)]
pub struct BluetoothCore {
    session_generation: u64,
    bluez_generation: u64,
    running: bool,
    owner_known_for_session: bool,
    owner: Option<String>,
    store: ObjectStore,
    selected_adapter_path: String,
    power: PowerState,
    operation_ids: OperationIds,
    device_operations: DeviceOperationState,
    pairing: PairingState,
    trust: TrustState,
    forget: ForgetState,
    agent_prompt: AgentPromptView,
    discovery: DiscoveryState,
    snapshot: CoreSnapshot,
    initial_snapshot_pending: bool,
    initial_snapshot_events: VecDeque<BluezEvent>,
    initial_snapshot_overflowed: bool,
}

impl BluetoothCore {
    pub fn snapshot(&self) -> &CoreSnapshot {
        &self.snapshot
    }

    pub fn session_generation(&self) -> u64 {
        self.session_generation
    }

    pub fn bluez_generation(&self) -> u64 {
        self.bluez_generation
    }

    pub fn scan_owners(&self) -> &BTreeSet<String> {
        self.discovery.owners()
    }

    #[cfg(test)]
    pub(crate) fn pending_device_operations(&self) -> usize {
        self.device_operations.pending().count()
    }

    pub fn start(&mut self) -> Option<CoreAction> {
        if self.running {
            return None;
        }
        let generation = self.session_generation.checked_add(1)?;
        if !self.start_generation(generation) {
            return None;
        }
        Some(CoreAction::StartBus {
            session_generation: generation,
        })
    }

    pub fn start_generation(&mut self, session_generation: u64) -> bool {
        if session_generation <= self.session_generation {
            return false;
        }
        self.session_generation = session_generation;
        self.running = true;
        self.owner_known_for_session = false;
        self.owner = None;
        self.selected_adapter_path.clear();
        self.store.clear_generation(self.bluez_generation);
        self.power.clear_pending();
        self.device_operations.clear();
        self.clear_phase2_state();
        self.discovery.reset();
        self.initial_snapshot_pending = false;
        self.initial_snapshot_events.clear();
        self.initial_snapshot_overflowed = false;
        self.snapshot = CoreSnapshot {
            session_generation,
            state: ServiceState::Starting,
            ..CoreSnapshot::default()
        };
        true
    }

    pub fn stop(&mut self) {
        let generation = self.session_generation.saturating_add(1);
        self.stop_generation(generation);
    }

    pub fn stop_generation(&mut self, session_generation: u64) -> bool {
        if session_generation < self.session_generation {
            return false;
        }
        self.session_generation = session_generation;
        self.running = false;
        self.owner_known_for_session = false;
        self.owner = None;
        self.selected_adapter_path.clear();
        self.store.clear_generation(self.bluez_generation);
        self.power.clear_pending();
        self.device_operations.clear();
        self.clear_phase2_state();
        self.discovery.reset();
        self.initial_snapshot_pending = false;
        self.initial_snapshot_events.clear();
        self.initial_snapshot_overflowed = false;
        self.snapshot = CoreSnapshot {
            session_generation,
            ..CoreSnapshot::default()
        };
        true
    }

    pub fn bus_unavailable(&mut self, session_generation: u64, error: String) -> bool {
        if !self.accepts_session(session_generation) {
            return false;
        }
        self.snapshot = CoreSnapshot {
            session_generation,
            state: ServiceState::Unavailable,
            error: Some(error),
            ..CoreSnapshot::default()
        };
        true
    }

    pub fn connection_lost(&mut self, session_generation: u64, error: String) -> bool {
        if !self.accepts_session(session_generation) {
            return false;
        }
        let Some(generation) = self.bluez_generation.checked_add(1) else {
            self.fail("Bluetooth service generation exhausted".to_owned());
            return true;
        };
        self.bluez_generation = generation;
        self.owner_known_for_session = false;
        self.owner = None;
        self.store.clear_generation(generation);
        self.selected_adapter_path.clear();
        self.power.clear_pending();
        self.device_operations.clear();
        self.clear_phase2_state();
        self.discovery.set_adapter_ready(false);
        self.initial_snapshot_pending = false;
        self.initial_snapshot_events.clear();
        self.initial_snapshot_overflowed = false;
        self.snapshot = CoreSnapshot {
            session_generation,
            state: ServiceState::Unavailable,
            error: Some(error),
            ..CoreSnapshot::default()
        };
        true
    }

    /// Accepts only owner changes from the active engine session. A different
    /// unique owner, including disappearance, creates a new BlueZ generation.
    pub fn owner_changed(
        &mut self,
        session_generation: u64,
        owner: Option<String>,
    ) -> Vec<CoreAction> {
        if !self.accepts_session(session_generation)
            || (self.owner_known_for_session && self.owner == owner)
        {
            return Vec::new();
        }
        self.owner_known_for_session = true;
        let Some(generation) = self.bluez_generation.checked_add(1) else {
            self.fail("Bluetooth service generation exhausted".to_owned());
            return Vec::new();
        };
        self.bluez_generation = generation;
        self.owner = owner.clone();
        self.store.clear_generation(generation);
        self.selected_adapter_path.clear();
        self.power.clear_pending();
        self.device_operations.clear();
        self.clear_phase2_state();
        self.discovery.set_adapter_ready(false);
        self.initial_snapshot_events.clear();
        self.initial_snapshot_overflowed = false;
        if let Some(owner) = owner {
            self.initial_snapshot_pending = true;
            self.snapshot = CoreSnapshot {
                session_generation,
                state: ServiceState::Starting,
                ..CoreSnapshot::default()
            };
            vec![CoreAction::Probe {
                session_generation,
                bluez_generation: generation,
                owner,
            }]
        } else {
            self.initial_snapshot_pending = false;
            self.snapshot = CoreSnapshot {
                session_generation,
                state: ServiceState::Unavailable,
                error: Some("Bluetooth service is unavailable".to_owned()),
                ..CoreSnapshot::default()
            };
            Vec::new()
        }
    }

    pub fn managed_objects(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        objects: BTreeMap<String, InterfaceMap>,
    ) -> Option<Vec<CoreAction>> {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return None;
        }
        if self.initial_snapshot_overflowed {
            self.initial_snapshot_events.clear();
            self.initial_snapshot_overflowed = false;
            self.initial_snapshot_pending = true;
            return self.owner.clone().map(|owner| {
                vec![CoreAction::Probe {
                    session_generation,
                    bluez_generation,
                    owner,
                }]
            });
        }
        self.store.replace_all(bluez_generation, objects);
        self.initial_snapshot_pending = false;
        let pending_events = std::mem::take(&mut self.initial_snapshot_events);
        let mut refresh_actions = Vec::new();
        for event in pending_events {
            match event {
                BluezEvent::InterfacesAdded { path, interfaces } => {
                    self.store.interfaces_added(&path, interfaces);
                }
                BluezEvent::InterfacesRemoved { path, interfaces } => {
                    self.store.interfaces_removed(&path, &interfaces);
                }
                BluezEvent::PropertiesChanged {
                    path,
                    interface,
                    changed,
                    invalidated,
                } => {
                    if self
                        .store
                        .properties_changed(&path, &interface, changed, &invalidated)
                        && !invalidated.is_empty()
                        && let Some(action) =
                            self.store
                                .begin_refresh(&path, &interface)
                                .and_then(|token| {
                                    self.owner.clone().map(|owner| CoreAction::GetAll {
                                        session_generation,
                                        owner,
                                        token,
                                    })
                                })
                    {
                        refresh_actions.push(action);
                    }
                }
            }
        }
        self.refresh_snapshot();
        Some(refresh_actions)
    }

    pub fn interfaces_added(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        path: &str,
        interfaces: InterfaceMap,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        if self.initial_snapshot_pending {
            self.buffer_initial_snapshot_event(BluezEvent::InterfacesAdded {
                path: path.to_owned(),
                interfaces,
            });
            return true;
        }
        self.store.interfaces_added(path, interfaces);
        self.refresh_snapshot();
        true
    }

    pub fn interfaces_removed(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        path: &str,
        interfaces: &[String],
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        if self.initial_snapshot_pending {
            self.buffer_initial_snapshot_event(BluezEvent::InterfacesRemoved {
                path: path.to_owned(),
                interfaces: interfaces.to_vec(),
            });
            return true;
        }
        self.store.interfaces_removed(path, interfaces);
        self.refresh_snapshot();
        true
    }

    pub fn properties_changed(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        path: &str,
        interface: &str,
        changed: PropertyMap,
        invalidated: &[String],
    ) -> (bool, Option<CoreAction>) {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return (false, None);
        }
        if self.initial_snapshot_pending {
            self.buffer_initial_snapshot_event(BluezEvent::PropertiesChanged {
                path: path.to_owned(),
                interface: interface.to_owned(),
                changed,
                invalidated: invalidated.to_vec(),
            });
            return (true, None);
        }
        if !self
            .store
            .properties_changed(path, interface, changed, invalidated)
        {
            return (false, None);
        }
        let refresh = if invalidated.is_empty() {
            None
        } else {
            self.store.begin_refresh(path, interface).and_then(|token| {
                self.owner.clone().map(|owner| CoreAction::GetAll {
                    session_generation,
                    owner,
                    token,
                })
            })
        };
        self.refresh_snapshot();
        (true, refresh)
    }

    pub fn refresh_reply(
        &mut self,
        session_generation: u64,
        owner: &str,
        token: &InterfaceRefreshToken,
        properties: PropertyMap,
    ) -> bool {
        if !self.accepts_session(session_generation)
            || self.owner.as_deref() != Some(owner)
            || token.generation != self.bluez_generation
        {
            return false;
        }
        let replaced = self.store.replace_interface_if_revision(token, properties);
        if replaced {
            self.refresh_snapshot();
        }
        replaced
    }

    pub fn set_powered(&mut self, target: bool, now: Instant) -> Option<CoreAction> {
        if !self.snapshot.available || !self.snapshot.adapter_available {
            return None;
        }
        let operation_id = self.operation_ids.allocate()?;
        self.power.begin(operation_id, target, now);
        self.snapshot.power_pending = true;
        Some(CoreAction::SetPowered {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            owner: self.owner.clone()?,
            operation_id,
            adapter_path: self.selected_adapter_path.clone(),
            target,
        })
    }

    pub fn power_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        success: bool,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        match self.power.method_reply(operation_id, success) {
            PowerUpdate::Ignored | PowerUpdate::AwaitingAuthoritativeState => false,
            PowerUpdate::Failed => {
                self.snapshot.power_pending = false;
                self.fail(error.unwrap_or_else(|| "Bluetooth power update failed".to_owned()));
                true
            }
        }
    }

    pub fn request_scan(&mut self, owner: &str, now: Instant) -> Option<CoreAction> {
        if !self.discovery.request(owner) {
            return None;
        }
        self.discovery_action(now)
    }

    pub fn release_scan(&mut self, owner: &str, now: Instant) -> Option<CoreAction> {
        self.discovery.release(owner);
        self.discovery_action(now)
    }

    pub fn replace_scan_owners(
        &mut self,
        owners: BTreeSet<String>,
        now: Instant,
    ) -> Option<CoreAction> {
        self.discovery.replace_owners(owners);
        self.discovery_action(now)
    }

    pub fn connect_device(&mut self, object_path: &str, connect: bool) -> Option<CoreAction> {
        if !self.snapshot.available || object_path.is_empty() {
            return None;
        }
        if self.pairing_conflicts(object_path)
            || self.trust.is_pending(object_path)
            || self.forget.is_pending(object_path)
        {
            return None;
        }
        if connect && !self.snapshot.can_connect(object_path) {
            return None;
        }
        let operation_id = self.operation_ids.allocate()?;
        let operation = PendingDeviceOperation {
            operation_id,
            object_path: object_path.to_owned(),
            target_connected: connect,
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
        };
        self.device_operations.begin(operation);
        Some(CoreAction::Connect {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            owner: self.owner.clone()?,
            operation_id,
            object_path: object_path.to_owned(),
            connect,
        })
    }

    pub fn pair_device(&mut self, object_path: &str) -> Option<CoreAction> {
        let owner = self.owner.clone()?;
        let device = self.authoritative_device(object_path)?.clone();
        if !self.snapshot.available
            || !self.snapshot.adapter_available
            || !self.snapshot.powered
            || device.paired
            || self.pairing.active_identity().is_some()
            || self.trust.is_pending(object_path)
            || self.forget.is_pending(object_path)
            || self.has_device_conflict(object_path)
        {
            return None;
        }
        let operation_id = self.operation_ids.allocate()?;
        let identity = self
            .pairing
            .begin(
                operation_id,
                self.session_generation,
                self.bluez_generation,
                object_path,
                &device.name,
                Instant::now(),
            )
            .ok()?;
        self.refresh_snapshot();
        Some(CoreAction::RegisterAgent {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            owner,
            operation_id: identity.operation_id,
            pairing_epoch: identity.pairing_epoch,
            device_path: identity.device_path,
        })
    }

    pub fn agent_registered(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        pairing_epoch: u64,
    ) -> Option<CoreAction> {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return None;
        }
        let identity = self.pairing.active_identity()?.clone();
        if identity.operation_id != operation_id || identity.pairing_epoch != pairing_epoch {
            return None;
        }
        if !self.pairing.agent_registered(&identity) {
            return None;
        }
        let owner = self.owner.clone()?;
        Some(CoreAction::Pair {
            session_generation,
            bluez_generation,
            owner,
            operation_id,
            pairing_epoch,
            device_path: identity.device_path,
        })
    }

    pub fn agent_registration_failed(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        pairing_epoch: u64,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let Some(identity) = self.pairing.active_identity().cloned() else {
            return false;
        };
        if identity.operation_id != operation_id || identity.pairing_epoch != pairing_epoch {
            return false;
        }
        let failed = self.pairing.agent_registration_failed(&identity, error);
        self.refresh_snapshot();
        failed == PairingCompletion::Failed
    }

    #[allow(clippy::too_many_arguments)]
    pub fn pair_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        pairing_epoch: u64,
        object_path: &str,
        success: bool,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let identity = PairingIdentity {
            operation_id,
            pairing_epoch,
            session_generation,
            bluez_generation,
            device_path: object_path.to_owned(),
        };
        let result = self.pairing.pair_method_reply(&identity, success, error);
        self.refresh_snapshot();
        result == PairingCompletion::Failed
    }

    pub fn cancel_pairing(&mut self) -> Option<CoreAction> {
        let identity = self.pairing.active_identity().cloned()?;
        let owner = self.owner.clone()?;
        if self.pairing.cancel(&identity) == PairingCompletion::Ignored {
            return None;
        }
        self.agent_prompt = AgentPromptView::default();
        self.refresh_snapshot();
        Some(CoreAction::CancelPairing {
            session_generation: identity.session_generation,
            bluez_generation: identity.bluez_generation,
            owner,
            operation_id: identity.operation_id,
            pairing_epoch: identity.pairing_epoch,
            device_path: identity.device_path,
        })
    }

    pub fn set_device_trusted(&mut self, object_path: &str, target: bool) -> Option<CoreAction> {
        let owner = self.owner.clone()?;
        let device = self.authoritative_device(object_path)?.clone();
        if !self.snapshot.available
            || !self.snapshot.adapter_available
            || !device.paired
            || self.pairing_conflicts(object_path)
            || self.forget.is_pending(object_path)
            || self
                .device_operations
                .pending()
                .any(|operation| operation.object_path == object_path)
        {
            return None;
        }
        let operation_id = self.operation_ids.allocate()?;
        let identity = DeviceOperationIdentity {
            operation_id,
            object_path: object_path.to_owned(),
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
        };
        self.trust.begin(identity, target, Instant::now());
        self.snapshot.operation_error = None;
        Some(CoreAction::SetTrusted {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            owner,
            operation_id,
            object_path: object_path.to_owned(),
            target,
        })
    }

    #[allow(clippy::too_many_arguments)]
    pub fn trusted_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        object_path: &str,
        target: bool,
        success: bool,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let Some(operation) = self.trust.current(object_path) else {
            return false;
        };
        if operation.identity.operation_id != operation_id
            || operation.identity.session_generation != session_generation
            || operation.identity.bluez_generation != bluez_generation
            || operation.target_trusted != target
        {
            return false;
        }
        let result = self.trust.method_reply(&operation, success);
        if result == TrustCompletion::Failed {
            self.snapshot.operation_error = error;
        }
        result == TrustCompletion::Failed
    }

    pub fn forget_device(&mut self, object_path: &str) -> Option<CoreAction> {
        let owner = self.owner.clone()?;
        if !self.snapshot.available
            || !self.snapshot.adapter_available
            || self.authoritative_device(object_path).is_none()
            || self.pairing_conflicts(object_path)
            || self.trust.is_pending(object_path)
            || self.has_device_conflict(object_path)
        {
            return None;
        }
        let operation_id = self.operation_ids.allocate()?;
        let identity = DeviceOperationIdentity {
            operation_id,
            object_path: object_path.to_owned(),
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
        };
        self.forget.begin(identity, Instant::now()).ok()?;
        self.snapshot.operation_error = None;
        Some(CoreAction::RemoveDevice {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            owner,
            operation_id,
            adapter_path: self.selected_adapter_path.clone(),
            object_path: object_path.to_owned(),
        })
    }

    #[allow(clippy::too_many_arguments)]
    pub fn forget_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        object_path: &str,
        success: bool,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let Some(operation) = self.forget.current(object_path) else {
            return false;
        };
        if operation.identity.operation_id != operation_id
            || operation.identity.session_generation != session_generation
            || operation.identity.bluez_generation != bluez_generation
        {
            return false;
        }
        let result = self.forget.method_reply(&operation, success);
        if result == ForgetCompletion::Failed {
            self.snapshot.operation_error = error;
        }
        result == ForgetCompletion::Failed
    }

    pub fn agent_prompt_changed(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        prompt: AgentPromptView,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        if prompt.active {
            let Some(pairing) = self.pairing.active_identity() else {
                return false;
            };
            if pairing.session_generation != session_generation
                || pairing.bluez_generation != bluez_generation
                || prompt.session_generation != pairing.session_generation
                || prompt.bluez_generation != pairing.bluez_generation
                || prompt.pairing_epoch != pairing.pairing_epoch
                || prompt.device_path != pairing.device_path
            {
                return false;
            }
        }
        self.agent_prompt = prompt;
        self.refresh_snapshot();
        true
    }

    #[allow(clippy::too_many_arguments)]
    pub fn connect_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        object_path: &str,
        target_connected: bool,
        success: bool,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let operation = PendingDeviceOperation {
            operation_id,
            object_path: object_path.to_owned(),
            target_connected,
            session_generation,
            bluez_generation,
        };
        match self.device_operations.complete(&operation, success) {
            DeviceOperationCompletion::Ignored
            | DeviceOperationCompletion::AwaitingAuthoritativeState
            | DeviceOperationCompletion::Succeeded => false,
            DeviceOperationCompletion::Failed => {
                self.fail(error.unwrap_or_else(|| "Bluetooth device operation failed".to_owned()));
                true
            }
        }
    }

    pub fn discovery_reply(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        operation_id: u64,
        success: bool,
        now: Instant,
        error: Option<String>,
    ) -> (DiscoveryReply, Option<CoreAction>) {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return (DiscoveryReply::Ignored, None);
        }
        let result = self
            .discovery
            .operation_finished(operation_id, success, now);
        match result {
            DiscoveryReply::Succeeded => self.clear_error(),
            DiscoveryReply::Failed | DiscoveryReply::TimedOut => {
                self.fail(error.unwrap_or_else(|| "Bluetooth discovery update failed".to_owned()));
            }
            DiscoveryReply::Ignored => {}
        }
        (result, self.discovery_action(now))
    }

    /// Processes Rust-owned power and discovery deadlines and bounded retries.
    pub fn on_timer(&mut self, now: Instant) -> Vec<CoreAction> {
        if !self.running {
            return Vec::new();
        }
        let mut actions = Vec::new();
        if let Some((identity, PairingCompletion::TimedOut)) = self.pairing.expire(now) {
            self.agent_prompt = AgentPromptView::default();
            if let Some(owner) = self.owner.clone() {
                actions.push(CoreAction::CancelPairing {
                    session_generation: identity.session_generation,
                    bluez_generation: identity.bluez_generation,
                    owner,
                    operation_id: identity.operation_id,
                    pairing_epoch: identity.pairing_epoch,
                    device_path: identity.device_path,
                });
            }
        }
        if !self.trust.expire(now).is_empty() {
            self.snapshot.operation_error = Some("Bluetooth trust operation timed out".to_owned());
        }
        if !self.forget.expire(now).is_empty() {
            self.snapshot.operation_error = Some("Bluetooth forget operation timed out".to_owned());
        }
        if self.power.expire(now) {
            self.snapshot.power_pending = false;
            self.fail("Bluetooth power update timed out".to_owned());
        }
        if self.discovery.expire(now) {
            self.fail("Bluetooth discovery update timed out".to_owned());
        }
        if let Some(action) = self.discovery_action(now) {
            actions.push(action);
        }
        self.refresh_snapshot();
        actions
    }

    pub fn next_deadline(&self) -> Option<Instant> {
        [
            self.power.deadline(),
            self.discovery.pending().map(|op| op.deadline),
            self.discovery.retry_at(),
            self.pairing.next_deadline(),
            self.trust.next_deadline(),
            self.forget.next_deadline(),
        ]
        .into_iter()
        .flatten()
        .min()
    }

    pub fn operation_failed(
        &mut self,
        session_generation: u64,
        bluez_generation: u64,
        error: Option<String>,
    ) -> bool {
        if !self.accepts_bluez(session_generation, bluez_generation) {
            return false;
        }
        let error = error.unwrap_or_else(|| "Bluetooth operation failed".to_owned());
        if self.snapshot.available {
            self.fail(error);
        } else {
            self.snapshot.state = ServiceState::Unavailable;
            self.snapshot.error = Some(error);
        }
        true
    }

    /// Makes a best-effort StopDiscovery call before a service session stops or
    /// is replaced. The returned action carries the old identities; after the
    /// lifecycle transition, its completion is deliberately stale.
    pub fn stop_discovery_action(&mut self) -> Option<CoreAction> {
        let owner = self.owner.clone()?;
        if self.selected_adapter_path.is_empty() {
            self.discovery.reset();
            return None;
        }
        let operation = self.discovery.stop_for_shutdown(Instant::now())?;
        Some(CoreAction::Discovery {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            operation,
            owner,
            adapter_path: self.selected_adapter_path.clone(),
        })
    }

    fn discovery_action(&mut self, now: Instant) -> Option<CoreAction> {
        let command = self.discovery.drive(now)?;
        Some(CoreAction::Discovery {
            session_generation: self.session_generation,
            bluez_generation: self.bluez_generation,
            operation: command,
            owner: self.owner.clone()?,
            adapter_path: self.selected_adapter_path.clone(),
        })
    }

    fn buffer_initial_snapshot_event(&mut self, event: BluezEvent) {
        if self.initial_snapshot_events.len() < INITIAL_SNAPSHOT_EVENT_CAPACITY {
            self.initial_snapshot_events.push_back(event);
        } else {
            self.initial_snapshot_overflowed = true;
        }
    }

    fn refresh_snapshot(&mut self) {
        let current =
            (!self.selected_adapter_path.is_empty()).then_some(self.selected_adapter_path.as_str());
        let adapter_map = adapters(self.store.objects());
        let selected = select_adapter(&adapter_map, current).cloned();
        let selected_adapter_path = selected
            .as_ref()
            .map_or_else(String::new, |adapter| adapter.path.clone());
        let adapter_changed = self.selected_adapter_path != selected_adapter_path;
        if adapter_changed {
            // Replies and discovery leases are scoped to the adapter path as
            // well as the BlueZ generation. Retire both before projecting the
            // replacement adapter so a late old-path reply cannot converge a
            // new request.
            self.power.clear_pending();
            self.device_operations.clear();
            self.clear_phase2_state();
            self.discovery.set_adapter_ready(false);
        }
        self.selected_adapter_path = selected_adapter_path;
        let selected_info = selected.unwrap_or_else(AdapterInfo::default);
        let devices = project_devices(self.store.objects(), &self.selected_adapter_path);
        if let Some(operation) = self.pairing.active_identity().cloned()
            && devices
                .iter()
                .any(|device| device.object_path == operation.device_path && device.paired)
        {
            self.pairing.authoritative_paired(&operation, true);
            self.agent_prompt = AgentPromptView::default();
        }
        for operation in self.trust.pending().cloned().collect::<Vec<_>>() {
            if let Some(device) = devices
                .iter()
                .find(|device| device.object_path == operation.identity.object_path)
            {
                self.trust.authoritative(&operation, device.trusted);
            }
        }
        for operation in self.forget.pending().cloned().collect::<Vec<_>>() {
            if !devices
                .iter()
                .any(|device| device.object_path == operation.identity.object_path)
            {
                self.forget.authoritative_removed(&operation);
            }
        }
        for operation in self
            .device_operations
            .pending()
            .cloned()
            .collect::<Vec<_>>()
        {
            if let Some(device) = devices
                .iter()
                .find(|device| device.object_path == operation.object_path)
            {
                self.device_operations
                    .authoritative_connected(&operation, device.connected);
            }
        }
        let mut connected_count = 0;
        let mut connected_name = String::new();
        for device in &devices {
            if device.connected {
                connected_count += 1;
                if connected_name.is_empty() {
                    connected_name.clone_from(&device.name);
                }
            }
        }
        let daemon_available = self.owner.is_some();
        self.discovery.set_adapter_ready(
            daemon_available && !self.selected_adapter_path.is_empty() && selected_info.powered,
        );
        self.discovery
            .set_actual_discovering(selected_info.discovering);
        if !daemon_available || self.selected_adapter_path.is_empty() {
            self.power.clear_pending();
        }
        let powered = if self.selected_adapter_path.is_empty() {
            false
        } else {
            self.power.set_authoritative_powered(selected_info.powered);
            selected_info.powered
        };
        let pairing = self.pairing.snapshot();
        let agent_prompt = self.agent_prompt.clone();
        let agent_device_name = devices
            .iter()
            .find(|device| device.object_path == agent_prompt.device_path)
            .map_or_else(String::new, |device| device.name.clone());
        self.snapshot = CoreSnapshot {
            session_generation: self.session_generation,
            state: if daemon_available {
                ServiceState::Ready
            } else {
                ServiceState::Unavailable
            },
            available: daemon_available,
            ready: daemon_available,
            adapter_available: !self.selected_adapter_path.is_empty(),
            adapter_path: self.selected_adapter_path.clone(),
            adapter_name: selected_info.name,
            powered,
            power_pending: self.power.pending(),
            scanning: selected_info.discovering,
            connected_count,
            connected_name,
            error: self.snapshot.error.clone(),
            devices,
            pairing: pairing.pairing,
            pairing_device_path: pairing.pairing_device_path,
            pairing_device_name: pairing.pairing_device_name,
            pairing_error: pairing.pairing_error,
            agent_request_active: agent_prompt.active,
            agent_request_id: agent_prompt.request_id,
            agent_request_kind: agent_prompt.kind,
            agent_device_path: agent_prompt.device_path,
            agent_device_name,
            agent_passkey: agent_prompt.passkey,
            agent_entered: agent_prompt.entered,
            agent_service_uuid: agent_prompt.service_uuid,
            agent_display_pin: agent_prompt.display_pin,
            operation_error: self.snapshot.operation_error.clone(),
        };
    }

    fn clear_error(&mut self) {
        self.snapshot.error = None;
        if self.snapshot.available {
            self.snapshot.state = ServiceState::Ready;
        }
    }

    fn authoritative_device(&self, object_path: &str) -> Option<&BluetoothDevice> {
        if self.selected_adapter_path.is_empty()
            || !object_path.starts_with(&(self.selected_adapter_path.clone() + "/"))
        {
            return None;
        }
        self.snapshot
            .devices
            .iter()
            .find(|device| device.object_path == object_path)
    }

    fn has_device_conflict(&self, object_path: &str) -> bool {
        self.device_operations
            .pending()
            .any(|operation| operation.object_path == object_path)
    }

    fn pairing_conflicts(&self, object_path: &str) -> bool {
        self.pairing
            .active_identity()
            .is_some_and(|identity| identity.device_path == object_path)
    }

    fn clear_phase2_state(&mut self) {
        self.pairing.clear();
        self.trust.clear();
        self.forget.clear();
        self.agent_prompt = AgentPromptView::default();
    }

    fn fail(&mut self, error: String) {
        self.snapshot.error = Some(error);
        if self.snapshot.available {
            self.snapshot.state = ServiceState::Degraded;
        }
    }

    fn accepts_session(&self, session_generation: u64) -> bool {
        self.running && session_generation == self.session_generation
    }

    fn accepts_bluez(&self, session_generation: u64, bluez_generation: u64) -> bool {
        self.accepts_session(session_generation)
            && self.owner.is_some()
            && bluez_generation == self.bluez_generation
    }
}
