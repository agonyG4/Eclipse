use astrea_system_backend::bluetooth::discovery::{
    DISCOVERY_TIMEOUT, DiscoveryLease, DiscoveryOperationKind, DiscoveryReply, DiscoveryState,
};
use std::collections::BTreeSet;
use std::time::{Duration, Instant};

#[test]
fn discovery_keeps_demand_lease_and_actual_state_separate() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("topbar");
    state.request("bluetooth-popup");
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start requested");
    assert_eq!(start.kind, DiscoveryOperationKind::Start);
    assert!(state.has_demand());
    assert_eq!(state.lease(), DiscoveryLease::StartPending);
    assert!(!state.actual_discovering());

    assert_eq!(
        state.operation_finished(start.id, true, now),
        DiscoveryReply::Succeeded
    );
    assert_eq!(state.lease(), DiscoveryLease::Held);
    state.set_actual_discovering(true);
    state.release("topbar");
    assert!(state.drive(now).is_none());
    state.release("bluetooth-popup");
    let stop = state.drive(now).expect("stop after final owner releases");
    assert_eq!(stop.kind, DiscoveryOperationKind::Stop);
    assert!(state.actual_discovering());
}

#[test]
fn failed_start_retries_after_bounded_delay_and_late_reply_is_ignored() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("topbar");
    state.set_adapter_ready(true);
    let first = state.drive(now).expect("first start");
    assert_eq!(
        state.operation_finished(first.id, false, now),
        DiscoveryReply::Failed
    );
    assert_eq!(state.retry_at(), Some(now + Duration::from_millis(500)));
    assert!(state.drive(now + Duration::from_millis(499)).is_none());
    let retry = state
        .drive(now + Duration::from_millis(500))
        .expect("retry");
    assert_ne!(first.id, retry.id);
    assert_eq!(
        state.operation_finished(first.id, true, now),
        DiscoveryReply::Ignored
    );
}

#[test]
fn discovery_timeout_retires_old_identity_before_retry() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("popup");
    state.set_adapter_ready(true);
    let first = state.drive(now).expect("start");
    assert_eq!(
        state.pending().expect("pending").deadline,
        now + DISCOVERY_TIMEOUT
    );
    assert!(state.expire(now + DISCOVERY_TIMEOUT));
    assert_eq!(
        state.operation_finished(first.id, true, now + DISCOVERY_TIMEOUT),
        DiscoveryReply::Ignored
    );
    let retry_at = now + DISCOVERY_TIMEOUT + Duration::from_millis(500);
    let retry = state.drive(retry_at).expect("timed-out start retries");
    assert_ne!(first.id, retry.id);
}

#[test]
fn retry_cancels_when_demand_disappears_or_stop_is_no_longer_needed() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("popup");
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start");
    state.operation_finished(start.id, false, now);
    assert!(state.retry_at().is_some());
    state.release("popup");
    assert_eq!(state.retry_at(), None);

    state.request("popup");
    let start = state.drive(now).expect("start again");
    state.operation_finished(start.id, true, now);
    state.release("popup");
    let stop = state.drive(now).expect("stop");
    state.operation_finished(stop.id, false, now);
    assert!(state.retry_at().is_some());
    state.request("topbar");
    assert_eq!(state.retry_at(), None);
    assert_eq!(state.lease(), DiscoveryLease::Held);
}

#[test]
fn daemon_loss_retires_work_and_recovery_resumes_demand() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("topbar");
    state.set_adapter_ready(true);
    let old_start = state.drive(now).expect("start");
    state.set_adapter_ready(false);
    assert_eq!(state.lease(), DiscoveryLease::None);
    assert_eq!(
        state.operation_finished(old_start.id, true, now),
        DiscoveryReply::Ignored
    );
    assert!(state.has_demand());
    state.set_adapter_ready(true);
    let resumed = state.drive(now).expect("resume demand");
    assert_ne!(resumed.id, old_start.id);
}

#[test]
fn new_demand_during_pending_stop_waits_for_authoritative_operation_result() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("one");
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start");
    state.operation_finished(start.id, true, now);
    state.release("one");
    let stop = state.drive(now).expect("stop");
    state.request("two");
    assert!(state.drive(now).is_none());
    assert_eq!(state.lease(), DiscoveryLease::StopPending);
    assert_eq!(
        state.operation_finished(stop.id, true, now),
        DiscoveryReply::Succeeded
    );
    let restart = state.drive(now).expect("new lease for returning demand");
    assert_eq!(restart.kind, DiscoveryOperationKind::Start);
}

#[test]
fn late_stop_reply_cannot_settle_a_newer_stop_and_external_discovery_is_preserved() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("one");
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start");
    state.operation_finished(start.id, true, now);
    state.release("one");
    let old_stop = state.drive(now).expect("old stop");
    state.operation_finished(old_stop.id, false, now);
    let new_stop = state
        .drive(now + Duration::from_millis(500))
        .expect("retry stop");
    state.set_actual_discovering(true); // another BlueZ client can still hold discovery
    assert_eq!(
        state.operation_finished(old_stop.id, true, now),
        DiscoveryReply::Ignored
    );
    assert_eq!(state.lease(), DiscoveryLease::StopPending);
    assert!(state.actual_discovering());
    assert_eq!(
        state.operation_finished(new_stop.id, true, now),
        DiscoveryReply::Succeeded
    );
    assert!(state.actual_discovering());
    assert_eq!(state.lease(), DiscoveryLease::None);
}

#[test]
fn service_shutdown_releases_only_its_own_discovery_lease() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.request("topbar");
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start");
    assert_eq!(
        state.operation_finished(start.id, true, now),
        DiscoveryReply::Succeeded
    );
    state.set_actual_discovering(true);

    let stop = state.stop_for_shutdown(now).expect("release local lease");
    assert_eq!(stop.kind, DiscoveryOperationKind::Stop);
    assert!(!state.has_demand());
    assert_eq!(state.lease(), DiscoveryLease::StopPending);
    assert!(state.actual_discovering());
    assert_eq!(
        state.operation_finished(stop.id, true, now),
        DiscoveryReply::Succeeded
    );
    assert_eq!(state.lease(), DiscoveryLease::None);
    assert!(state.actual_discovering());

    let mut external = DiscoveryState::default();
    external.set_adapter_ready(true);
    external.set_actual_discovering(true);
    assert!(external.stop_for_shutdown(now).is_none());
    assert!(external.actual_discovering());
}

#[test]
fn coalesced_owner_state_replaces_demand_without_losing_independent_owners() {
    let now = Instant::now();
    let mut state = DiscoveryState::default();
    state.replace_owners(BTreeSet::from([
        String::from("topbar"),
        String::from("bluetooth-popup"),
    ]));
    state.set_adapter_ready(true);
    let start = state.drive(now).expect("start requested");
    assert_eq!(
        state.operation_finished(start.id, true, now),
        DiscoveryReply::Succeeded
    );

    state.replace_owners(BTreeSet::from([String::from("bluetooth-popup")]));
    assert!(state.has_demand());
    assert!(state.owners().contains("bluetooth-popup"));
    assert!(!state.owners().contains("topbar"));
    assert!(state.drive(now).is_none());

    state.replace_owners(BTreeSet::new());
    let stop = state.drive(now).expect("final owner release requests stop");
    assert_eq!(stop.kind, DiscoveryOperationKind::Stop);
}

#[test]
fn replacing_owner_state_with_an_absent_owner_release_is_harmless() {
    let mut state = DiscoveryState::default();
    state.replace_owners(BTreeSet::from([String::from("topbar")]));

    state.replace_owners(BTreeSet::from([String::from("topbar")]));

    assert_eq!(state.owners().len(), 1);
    assert!(state.owners().contains("topbar"));
}
