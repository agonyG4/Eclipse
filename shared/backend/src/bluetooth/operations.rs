use std::collections::BTreeMap;
use std::time::{Duration, Instant};

pub const OPERATION_TIMEOUT: Duration = Duration::from_millis(3000);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PowerUpdate {
    Ignored,
    AwaitingAuthoritativeState,
    Failed,
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct PendingPower {
    id: u64,
    target: bool,
    deadline: Instant,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PowerState {
    powered: bool,
    pending: Option<PendingPower>,
}

impl PowerState {
    pub fn powered(&self) -> bool {
        self.powered
    }

    pub fn pending(&self) -> bool {
        self.pending.is_some()
    }

    pub fn pending_id(&self) -> Option<u64> {
        self.pending.as_ref().map(|pending| pending.id)
    }

    pub fn pending_target(&self) -> Option<bool> {
        self.pending.as_ref().map(|pending| pending.target)
    }

    pub fn deadline(&self) -> Option<Instant> {
        self.pending.as_ref().map(|pending| pending.deadline)
    }

    pub fn begin(&mut self, id: u64, target: bool, now: Instant) {
        self.pending = Some(PendingPower {
            id,
            target,
            deadline: now + OPERATION_TIMEOUT,
        });
    }

    pub fn set_authoritative_powered(&mut self, powered: bool) -> bool {
        self.powered = powered;
        let converged = self
            .pending
            .as_ref()
            .is_some_and(|pending| pending.target == powered);
        if converged {
            self.pending = None;
        }
        converged
    }

    pub fn method_reply(&mut self, id: u64, success: bool) -> PowerUpdate {
        let Some(pending) = self.pending.as_ref() else {
            return PowerUpdate::Ignored;
        };
        if pending.id != id {
            return PowerUpdate::Ignored;
        }
        if success {
            PowerUpdate::AwaitingAuthoritativeState
        } else {
            self.pending = None;
            PowerUpdate::Failed
        }
    }

    pub fn expire(&mut self, now: Instant) -> bool {
        if self
            .pending
            .as_ref()
            .is_some_and(|pending| pending.deadline <= now)
        {
            self.pending = None;
            true
        } else {
            false
        }
    }

    pub fn clear_pending(&mut self) {
        self.pending = None;
    }
}

#[derive(Debug, Default)]
pub struct OperationIds {
    last_id: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PendingDeviceOperation {
    pub operation_id: u64,
    pub object_path: String,
    pub target_connected: bool,
    pub session_generation: u64,
    pub bluez_generation: u64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DeviceOperationCompletion {
    Ignored,
    AwaitingAuthoritativeState,
    Succeeded,
    Failed,
}

#[derive(Debug, Default)]
pub struct DeviceOperationState {
    pending: BTreeMap<String, PendingDeviceOperation>,
}

impl DeviceOperationState {
    pub fn begin(&mut self, operation: PendingDeviceOperation) {
        self.pending
            .insert(operation.object_path.clone(), operation);
    }

    pub fn authoritative_connected(
        &mut self,
        operation: &PendingDeviceOperation,
        connected: bool,
    ) -> bool {
        if operation.target_connected != connected || !self.is_current(operation) {
            return false;
        }
        self.pending.remove(&operation.object_path);
        true
    }

    pub fn complete(
        &mut self,
        operation: &PendingDeviceOperation,
        success: bool,
    ) -> DeviceOperationCompletion {
        if !self.is_current(operation) {
            return DeviceOperationCompletion::Ignored;
        }
        if success {
            DeviceOperationCompletion::AwaitingAuthoritativeState
        } else {
            self.pending.remove(&operation.object_path);
            DeviceOperationCompletion::Failed
        }
    }

    pub fn clear(&mut self) {
        self.pending.clear();
    }

    pub fn pending(&self) -> impl Iterator<Item = &PendingDeviceOperation> {
        self.pending.values()
    }

    fn is_current(&self, operation: &PendingDeviceOperation) -> bool {
        self.pending
            .get(&operation.object_path)
            .is_some_and(|current| current == operation)
    }
}

impl OperationIds {
    pub fn allocate(&mut self) -> Option<u64> {
        let next = self.last_id.checked_add(1)?;
        self.last_id = next;
        Some(next)
    }
}
