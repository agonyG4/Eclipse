use std::collections::BTreeMap;
use std::time::{Duration, Instant};

use super::operations::OPERATION_TIMEOUT;

pub const PAIRING_TIMEOUT: Duration = Duration::from_secs(120);
pub const AGENT_PROMPT_TIMEOUT: Duration = Duration::from_secs(60);

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DeviceOperationIdentity {
    pub operation_id: u64,
    pub object_path: String,
    pub session_generation: u64,
    pub bluez_generation: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct PairingOperation {
    identity: PairingIdentity,
    device_name: String,
    deadline: Instant,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PairingIdentity {
    pub operation_id: u64,
    pub pairing_epoch: u64,
    pub session_generation: u64,
    pub bluez_generation: u64,
    pub device_path: String,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum PairingPhase {
    #[default]
    Idle,
    EnsuringAgent,
    Pairing,
    AwaitingAuthoritativePaired,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PairingCompletion {
    Ignored,
    AwaitingAuthoritativeState,
    Succeeded,
    Failed,
    Canceled,
    TimedOut,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PairingStartError {
    AlreadyActive,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PairingSnapshot {
    pub pairing: bool,
    pub pairing_device_path: String,
    pub pairing_device_name: String,
    pub pairing_error: Option<String>,
}

#[derive(Debug, Default)]
pub struct PairingState {
    active: Option<PairingOperation>,
    next_pairing_epoch: u64,
    phase: PairingPhase,
    pairing_error: Option<String>,
}

impl PairingState {
    #[cfg(test)]
    pub(crate) fn expire_now_for_test(&mut self) {
        if let Some(active) = self.active.as_mut() {
            active.deadline = Instant::now();
        }
    }

    pub fn phase(&self) -> PairingPhase {
        self.phase
    }

    pub fn active_identity(&self) -> Option<&PairingIdentity> {
        self.active.as_ref().map(|operation| &operation.identity)
    }

    pub fn snapshot(&self) -> PairingSnapshot {
        let Some(active) = &self.active else {
            return PairingSnapshot {
                pairing_error: self.pairing_error.clone(),
                ..PairingSnapshot::default()
            };
        };
        PairingSnapshot {
            pairing: true,
            pairing_device_path: active.identity.device_path.clone(),
            pairing_device_name: active.device_name.clone(),
            pairing_error: self.pairing_error.clone(),
        }
    }

    pub fn begin(
        &mut self,
        operation_id: u64,
        session_generation: u64,
        bluez_generation: u64,
        device_path: &str,
        device_name: &str,
        now: Instant,
    ) -> Result<PairingIdentity, PairingStartError> {
        if self.active.is_some() {
            return Err(PairingStartError::AlreadyActive);
        }
        let Some(pairing_epoch) = self.next_pairing_epoch.checked_add(1) else {
            return Err(PairingStartError::AlreadyActive);
        };
        self.next_pairing_epoch = pairing_epoch;
        self.pairing_error = None;
        let identity = PairingIdentity {
            operation_id,
            pairing_epoch,
            session_generation,
            bluez_generation,
            device_path: device_path.to_owned(),
        };
        self.active = Some(PairingOperation {
            identity: identity.clone(),
            device_name: device_name.to_owned(),
            deadline: now + PAIRING_TIMEOUT,
        });
        self.phase = PairingPhase::EnsuringAgent;
        Ok(identity)
    }

    pub fn agent_registered(&mut self, identity: &PairingIdentity) -> bool {
        if !self.is_current(identity) || self.phase != PairingPhase::EnsuringAgent {
            return false;
        }
        self.phase = PairingPhase::Pairing;
        true
    }

    pub fn agent_registration_failed(
        &mut self,
        identity: &PairingIdentity,
        error: Option<String>,
    ) -> PairingCompletion {
        if !self.is_current(identity) || self.phase != PairingPhase::EnsuringAgent {
            return PairingCompletion::Ignored;
        }
        self.retire_with_error(error);
        PairingCompletion::Failed
    }

    pub fn pair_method_reply(
        &mut self,
        identity: &PairingIdentity,
        success: bool,
        error: Option<String>,
    ) -> PairingCompletion {
        if !self.is_current(identity) {
            return PairingCompletion::Ignored;
        }
        if success {
            self.phase = PairingPhase::AwaitingAuthoritativePaired;
            PairingCompletion::AwaitingAuthoritativeState
        } else {
            self.retire_with_error(error);
            PairingCompletion::Failed
        }
    }

    pub fn authoritative_paired(
        &mut self,
        identity: &PairingIdentity,
        paired: bool,
    ) -> PairingCompletion {
        if !paired || !self.is_current(identity) {
            return PairingCompletion::Ignored;
        }
        self.active = None;
        self.phase = PairingPhase::Idle;
        self.pairing_error = None;
        PairingCompletion::Succeeded
    }

    pub fn cancel(&mut self, identity: &PairingIdentity) -> PairingCompletion {
        if !self.is_current(identity) {
            return PairingCompletion::Ignored;
        }
        self.active = None;
        self.phase = PairingPhase::Idle;
        self.pairing_error = Some(String::from("Bluetooth pairing canceled"));
        PairingCompletion::Canceled
    }

    pub fn expire(&mut self, now: Instant) -> Option<(PairingIdentity, PairingCompletion)> {
        if self
            .active
            .as_ref()
            .is_some_and(|operation| operation.deadline <= now)
        {
            let identity = self
                .active
                .as_ref()
                .expect("active pairing checked above")
                .identity
                .clone();
            self.active = None;
            self.phase = PairingPhase::Idle;
            self.pairing_error = Some(String::from("Bluetooth pairing timed out"));
            Some((identity, PairingCompletion::TimedOut))
        } else {
            None
        }
    }

    pub fn next_deadline(&self) -> Option<Instant> {
        self.active.as_ref().map(|operation| operation.deadline)
    }

    pub fn retire_generation(&mut self, session_generation: u64, bluez_generation: u64) -> bool {
        let Some(identity) = self.active_identity().cloned() else {
            return false;
        };
        if identity.session_generation == session_generation
            && identity.bluez_generation == bluez_generation
        {
            return false;
        }
        self.active = None;
        self.phase = PairingPhase::Idle;
        self.pairing_error = None;
        true
    }

    pub fn clear(&mut self) {
        self.active = None;
        self.phase = PairingPhase::Idle;
        self.pairing_error = None;
    }

    fn is_current(&self, identity: &PairingIdentity) -> bool {
        self.active
            .as_ref()
            .is_some_and(|operation| operation.identity == *identity)
    }

    fn retire_with_error(&mut self, error: Option<String>) {
        self.active = None;
        self.phase = PairingPhase::Idle;
        self.pairing_error =
            Some(error.unwrap_or_else(|| String::from("Bluetooth pairing failed")));
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PendingTrustOperation {
    pub identity: DeviceOperationIdentity,
    pub target_trusted: bool,
    pub deadline: Instant,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TrustCompletion {
    Ignored,
    AwaitingAuthoritativeState,
    Succeeded,
    Failed,
}

#[derive(Debug, Default)]
pub struct TrustState {
    pending: BTreeMap<String, PendingTrustOperation>,
}

impl TrustState {
    pub fn is_pending(&self, object_path: &str) -> bool {
        self.pending.contains_key(object_path)
    }

    pub fn current(&self, object_path: &str) -> Option<PendingTrustOperation> {
        self.pending.get(object_path).cloned()
    }

    pub fn begin(
        &mut self,
        identity: DeviceOperationIdentity,
        target_trusted: bool,
        now: Instant,
    ) -> PendingTrustOperation {
        let operation = PendingTrustOperation {
            identity: identity.clone(),
            target_trusted,
            deadline: now + OPERATION_TIMEOUT,
        };
        self.pending.insert(identity.object_path, operation.clone());
        operation
    }

    pub fn method_reply(
        &mut self,
        operation: &PendingTrustOperation,
        success: bool,
    ) -> TrustCompletion {
        if !self.is_current(operation) {
            return TrustCompletion::Ignored;
        }
        if success {
            TrustCompletion::AwaitingAuthoritativeState
        } else {
            self.pending.remove(&operation.identity.object_path);
            TrustCompletion::Failed
        }
    }

    pub fn authoritative(
        &mut self,
        operation: &PendingTrustOperation,
        trusted: bool,
    ) -> TrustCompletion {
        if !self.is_current(operation) || trusted != operation.target_trusted {
            return TrustCompletion::Ignored;
        }
        self.pending.remove(&operation.identity.object_path);
        TrustCompletion::Succeeded
    }

    pub fn expire(&mut self, now: Instant) -> Vec<PendingTrustOperation> {
        let expired = self
            .pending
            .values()
            .filter(|operation| operation.deadline <= now)
            .cloned()
            .collect::<Vec<_>>();
        for operation in &expired {
            self.pending.remove(&operation.identity.object_path);
        }
        expired
    }

    pub fn next_deadline(&self) -> Option<Instant> {
        self.pending
            .values()
            .map(|operation| operation.deadline)
            .min()
    }

    pub fn pending(&self) -> impl Iterator<Item = &PendingTrustOperation> {
        self.pending.values()
    }

    pub fn clear(&mut self) {
        self.pending.clear();
    }

    fn is_current(&self, operation: &PendingTrustOperation) -> bool {
        self.pending
            .get(&operation.identity.object_path)
            .is_some_and(|current| current == operation)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PendingForgetOperation {
    pub identity: DeviceOperationIdentity,
    pub deadline: Instant,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ForgetCompletion {
    Ignored,
    AwaitingAuthoritativeRemoval,
    Succeeded,
    Failed,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ForgetStartError {
    AlreadyActive,
}

#[derive(Debug, Default)]
pub struct ForgetState {
    pending: BTreeMap<String, PendingForgetOperation>,
}

impl ForgetState {
    pub fn is_pending(&self, object_path: &str) -> bool {
        self.pending.contains_key(object_path)
    }

    pub fn current(&self, object_path: &str) -> Option<PendingForgetOperation> {
        self.pending.get(object_path).cloned()
    }

    pub fn begin(
        &mut self,
        identity: DeviceOperationIdentity,
        now: Instant,
    ) -> Result<PendingForgetOperation, ForgetStartError> {
        if self.pending.contains_key(&identity.object_path) {
            return Err(ForgetStartError::AlreadyActive);
        }
        let operation = PendingForgetOperation {
            identity: identity.clone(),
            deadline: now + OPERATION_TIMEOUT,
        };
        self.pending.insert(identity.object_path, operation.clone());
        Ok(operation)
    }

    pub fn method_reply(
        &mut self,
        operation: &PendingForgetOperation,
        success: bool,
    ) -> ForgetCompletion {
        if !self.is_current(operation) {
            return ForgetCompletion::Ignored;
        }
        if success {
            ForgetCompletion::AwaitingAuthoritativeRemoval
        } else {
            self.pending.remove(&operation.identity.object_path);
            ForgetCompletion::Failed
        }
    }

    pub fn authoritative_removed(
        &mut self,
        operation: &PendingForgetOperation,
    ) -> ForgetCompletion {
        if !self.is_current(operation) {
            return ForgetCompletion::Ignored;
        }
        self.pending.remove(&operation.identity.object_path);
        ForgetCompletion::Succeeded
    }

    pub fn expire(&mut self, now: Instant) -> Vec<PendingForgetOperation> {
        let expired = self
            .pending
            .values()
            .filter(|operation| operation.deadline <= now)
            .cloned()
            .collect::<Vec<_>>();
        for operation in &expired {
            self.pending.remove(&operation.identity.object_path);
        }
        expired
    }

    pub fn next_deadline(&self) -> Option<Instant> {
        self.pending
            .values()
            .map(|operation| operation.deadline)
            .min()
    }

    pub fn pending(&self) -> impl Iterator<Item = &PendingForgetOperation> {
        self.pending.values()
    }

    pub fn clear(&mut self) {
        self.pending.clear();
    }

    fn is_current(&self, operation: &PendingForgetOperation) -> bool {
        self.pending
            .get(&operation.identity.object_path)
            .is_some_and(|current| current == operation)
    }
}
