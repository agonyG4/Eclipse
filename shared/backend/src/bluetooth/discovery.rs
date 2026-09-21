use std::collections::BTreeSet;
use std::time::{Duration, Instant};

pub const DISCOVERY_TIMEOUT: Duration = Duration::from_millis(3000);
const RETRY_DELAYS: [Duration; 4] = [
    Duration::from_millis(500),
    Duration::from_millis(1000),
    Duration::from_millis(2000),
    Duration::from_millis(5000),
];

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum DiscoveryLease {
    #[default]
    None,
    StartPending,
    Held,
    StopPending,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DiscoveryOperationKind {
    Start,
    Stop,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DiscoveryOperation {
    pub id: u64,
    pub kind: DiscoveryOperationKind,
    pub deadline: Instant,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DiscoveryCommand {
    pub id: u64,
    pub kind: DiscoveryOperationKind,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DiscoveryReply {
    Ignored,
    Succeeded,
    Failed,
    TimedOut,
}

#[derive(Default)]
pub struct DiscoveryState {
    owners: BTreeSet<String>,
    lease: DiscoveryLease,
    adapter_ready: bool,
    actual_discovering: bool,
    pending: Option<DiscoveryOperation>,
    retry_at: Option<Instant>,
    retry_attempt: usize,
    last_operation_id: u64,
}

impl DiscoveryState {
    pub fn reset(&mut self) {
        self.owners.clear();
        self.lease = DiscoveryLease::None;
        self.adapter_ready = false;
        self.actual_discovering = false;
        self.pending = None;
        self.retry_at = None;
        self.retry_attempt = 0;
    }

    pub fn request(&mut self, owner: &str) -> bool {
        if owner.is_empty() {
            return false;
        }
        self.owners.insert(owner.to_owned());
        self.cancel_unneeded_retry();
        true
    }

    pub fn replace_owners(&mut self, owners: BTreeSet<String>) {
        self.owners = owners
            .into_iter()
            .filter(|owner| !owner.is_empty())
            .collect();
        self.cancel_unneeded_retry();
    }

    pub fn release(&mut self, owner: &str) -> bool {
        let removed = self.owners.remove(owner);
        self.cancel_unneeded_retry();
        removed
    }

    /// Retires local demand and returns one best-effort release command for a
    /// lease that this service instance owns. Authoritative Discovering state
    /// is intentionally left untouched because another D-Bus client may keep
    /// BlueZ discovery active.
    pub fn stop_for_shutdown(&mut self, now: Instant) -> Option<DiscoveryCommand> {
        let owns_lease = self.lease != DiscoveryLease::None;
        self.owners.clear();
        self.pending = None;
        self.retry_at = None;
        self.retry_attempt = 0;
        if !owns_lease {
            self.lease = DiscoveryLease::None;
            return None;
        }
        let id = self.last_operation_id.checked_add(1)?;
        self.last_operation_id = id;
        self.lease = DiscoveryLease::StopPending;
        self.pending = Some(DiscoveryOperation {
            id,
            kind: DiscoveryOperationKind::Stop,
            deadline: now + DISCOVERY_TIMEOUT,
        });
        Some(DiscoveryCommand {
            id,
            kind: DiscoveryOperationKind::Stop,
        })
    }

    pub fn set_adapter_ready(&mut self, ready: bool) {
        self.adapter_ready = ready;
        if !ready {
            // All work submitted through the old adapter is retired. Demand is
            // retained so a new BlueZ generation can resume it after probing.
            self.pending = None;
            self.lease = DiscoveryLease::None;
            self.retry_at = None;
            self.retry_attempt = 0;
        }
    }

    pub fn set_actual_discovering(&mut self, discovering: bool) {
        self.actual_discovering = discovering;
    }

    pub fn lease(&self) -> DiscoveryLease {
        self.lease
    }

    pub fn has_demand(&self) -> bool {
        !self.owners.is_empty()
    }

    pub fn owners(&self) -> &BTreeSet<String> {
        &self.owners
    }

    pub fn actual_discovering(&self) -> bool {
        self.actual_discovering
    }

    pub fn pending(&self) -> Option<DiscoveryOperation> {
        self.pending
    }

    pub fn retry_at(&self) -> Option<Instant> {
        self.retry_at
    }

    pub fn wants_start(&self) -> bool {
        self.adapter_ready && self.has_demand() && self.lease == DiscoveryLease::None
    }

    pub fn wants_stop(&self) -> bool {
        !self.has_demand() && self.lease == DiscoveryLease::Held
    }

    /// Starts one operation when policy permits. The caller sends the returned
    /// command through its bounded worker queue and reports the matching reply.
    pub fn drive(&mut self, now: Instant) -> Option<DiscoveryCommand> {
        if let Some(pending) = self.pending {
            if pending.deadline <= now {
                let _ = self.finish(pending.id, false, now, true);
            } else {
                return None;
            }
        }
        if self.retry_at.is_some_and(|deadline| deadline > now) {
            return None;
        }
        if self.retry_at.is_some() {
            self.retry_at = None;
        }
        let kind = if self.wants_stop() {
            DiscoveryOperationKind::Stop
        } else if self.wants_start() {
            DiscoveryOperationKind::Start
        } else {
            return None;
        };
        self.last_operation_id = self.last_operation_id.checked_add(1)?;
        let id = self.last_operation_id;
        self.lease = match kind {
            DiscoveryOperationKind::Start => DiscoveryLease::StartPending,
            DiscoveryOperationKind::Stop => DiscoveryLease::StopPending,
        };
        self.pending = Some(DiscoveryOperation {
            id,
            kind,
            deadline: now + DISCOVERY_TIMEOUT,
        });
        Some(DiscoveryCommand { id, kind })
    }

    pub fn operation_finished(&mut self, id: u64, success: bool, now: Instant) -> DiscoveryReply {
        self.finish(id, success, now, false)
    }

    pub fn expire(&mut self, now: Instant) -> bool {
        let Some(operation) = self.pending else {
            return false;
        };
        if operation.deadline > now {
            return false;
        }
        self.finish(operation.id, false, now, true) == DiscoveryReply::TimedOut
    }

    fn finish(&mut self, id: u64, success: bool, now: Instant, timed_out: bool) -> DiscoveryReply {
        let Some(operation) = self.pending else {
            return DiscoveryReply::Ignored;
        };
        if operation.id != id {
            return DiscoveryReply::Ignored;
        }
        self.pending = None;
        self.lease = match (operation.kind, success) {
            (DiscoveryOperationKind::Start, true) => DiscoveryLease::Held,
            (DiscoveryOperationKind::Start, false) => DiscoveryLease::None,
            (DiscoveryOperationKind::Stop, true) => DiscoveryLease::None,
            (DiscoveryOperationKind::Stop, false) => DiscoveryLease::Held,
        };
        if success {
            self.retry_at = None;
            self.retry_attempt = 0;
            DiscoveryReply::Succeeded
        } else {
            self.schedule_retry(now);
            if timed_out {
                DiscoveryReply::TimedOut
            } else {
                DiscoveryReply::Failed
            }
        }
    }

    fn schedule_retry(&mut self, now: Instant) {
        self.cancel_unneeded_retry();
        if !self.adapter_ready || (!self.wants_start() && !self.wants_stop()) {
            return;
        }
        let delay = RETRY_DELAYS[self.retry_attempt.min(RETRY_DELAYS.len() - 1)];
        self.retry_attempt = self.retry_attempt.saturating_add(1);
        self.retry_at = Some(now + delay);
    }

    fn cancel_unneeded_retry(&mut self) {
        if !self.wants_start() && !self.wants_stop() {
            self.retry_at = None;
            self.retry_attempt = 0;
        }
    }
}
