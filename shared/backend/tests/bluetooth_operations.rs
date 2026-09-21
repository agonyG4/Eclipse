use astrea_system_backend::bluetooth::operations::{
    DeviceOperationCompletion, DeviceOperationState, OperationIds, PendingDeviceOperation,
    PowerState, PowerUpdate,
};
use std::time::{Duration, Instant};

#[test]
fn power_request_does_not_optimistically_change_authoritative_power() {
    let now = Instant::now();
    let mut ids = OperationIds::default();
    let mut power = PowerState::default();
    let operation_id = ids.allocate().expect("operation id is available");

    power.begin(operation_id, true, now);

    assert!(!power.powered());
    assert!(power.pending());
    assert_eq!(power.pending_target(), Some(true));
    assert_eq!(power.deadline(), Some(now + Duration::from_millis(3000)));
}

#[test]
fn older_power_reply_cannot_settle_a_replacement_request() {
    let now = Instant::now();
    let mut power = PowerState::default();
    power.begin(1, true, now);
    power.begin(2, false, now);

    assert_eq!(power.method_reply(1, false), PowerUpdate::Ignored);
    assert!(power.pending());
    assert_eq!(power.pending_id(), Some(2));
}

#[test]
fn authoritative_property_convergence_retires_power_before_a_late_failure() {
    let now = Instant::now();
    let mut power = PowerState::default();
    power.begin(7, true, now);

    assert!(power.set_authoritative_powered(true));
    assert!(!power.pending());
    assert_eq!(power.method_reply(7, false), PowerUpdate::Ignored);
    assert!(power.powered());
}

#[test]
fn power_timeout_retires_identity_before_a_later_request() {
    let now = Instant::now();
    let mut power = PowerState::default();
    power.begin(10, true, now);
    assert!(power.expire(now + Duration::from_millis(3000)));
    power.begin(11, true, now + Duration::from_millis(3001));

    assert_eq!(power.method_reply(10, true), PowerUpdate::Ignored);
    assert_eq!(power.pending_id(), Some(11));
}

#[test]
fn operation_ids_are_monotonic_and_explicit() {
    let mut ids = OperationIds::default();

    assert_eq!(ids.allocate(), Some(1));
    assert_eq!(ids.allocate(), Some(2));
    assert_eq!(ids.allocate(), Some(3));
}

fn operation(path: &str, id: u64, target_connected: bool) -> PendingDeviceOperation {
    PendingDeviceOperation {
        operation_id: id,
        object_path: path.to_owned(),
        target_connected,
        session_generation: 7,
        bluez_generation: 11,
    }
}

#[test]
fn replacing_connect_with_disconnect_retires_the_older_completion() {
    let mut state = DeviceOperationState::default();
    let connect = operation("/org/bluez/hci0/dev_AA", 1, true);
    let disconnect = operation("/org/bluez/hci0/dev_AA", 2, false);

    state.begin(connect.clone());
    state.begin(disconnect.clone());

    assert_eq!(
        state.complete(&connect, false),
        DeviceOperationCompletion::Ignored
    );
    assert_eq!(
        state.complete(&disconnect, false),
        DeviceOperationCompletion::Failed
    );
}

#[test]
fn replacing_disconnect_with_connect_retires_the_older_completion() {
    let mut state = DeviceOperationState::default();
    let disconnect = operation("/org/bluez/hci0/dev_AA", 1, false);
    let connect = operation("/org/bluez/hci0/dev_AA", 2, true);

    state.begin(disconnect.clone());
    state.begin(connect.clone());

    assert_eq!(
        state.complete(&disconnect, false),
        DeviceOperationCompletion::Ignored
    );
    assert_eq!(
        state.complete(&connect, false),
        DeviceOperationCompletion::Failed
    );
}

#[test]
fn authoritative_convergence_retires_operation_before_a_late_failure() {
    let mut state = DeviceOperationState::default();
    let connect = operation("/org/bluez/hci0/dev_AA", 1, true);
    state.begin(connect.clone());

    assert!(state.authoritative_connected(&connect, true));
    assert_eq!(
        state.complete(&connect, false),
        DeviceOperationCompletion::Ignored
    );
}

#[test]
fn different_device_paths_keep_operations_independent() {
    let mut state = DeviceOperationState::default();
    let first = operation("/org/bluez/hci0/dev_AA", 1, true);
    let second = operation("/org/bluez/hci0/dev_BB", 2, false);
    state.begin(first.clone());
    state.begin(second.clone());

    assert_eq!(
        state.complete(&first, false),
        DeviceOperationCompletion::Failed
    );
    assert_eq!(
        state.complete(&second, false),
        DeviceOperationCompletion::Failed
    );
}

#[test]
fn clearing_generation_retires_all_pending_device_operations() {
    let mut state = DeviceOperationState::default();
    let first = operation("/org/bluez/hci0/dev_AA", 1, true);
    let second = operation("/org/bluez/hci0/dev_BB", 2, false);
    state.begin(first.clone());
    state.begin(second.clone());

    state.clear();

    assert_eq!(
        state.complete(&first, false),
        DeviceOperationCompletion::Ignored
    );
    assert_eq!(
        state.complete(&second, false),
        DeviceOperationCompletion::Ignored
    );
}
