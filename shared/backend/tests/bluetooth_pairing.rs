use std::time::{Duration, Instant};

use astrea_system_backend::bluetooth::operations::OPERATION_TIMEOUT;
use astrea_system_backend::bluetooth::pairing::{
    ForgetCompletion, ForgetState, PairingCompletion, PairingPhase, PairingStartError,
    PairingState, TrustCompletion, TrustState,
};

fn identity(
    operation_id: u64,
    path: &str,
) -> astrea_system_backend::bluetooth::pairing::DeviceOperationIdentity {
    astrea_system_backend::bluetooth::pairing::DeviceOperationIdentity {
        operation_id,
        object_path: path.to_owned(),
        session_generation: 7,
        bluez_generation: 11,
    }
}

#[test]
fn pairing_is_single_session_and_waits_for_authoritative_paired() {
    let now = Instant::now();
    let mut state = PairingState::default();
    let first = state
        .begin(1, 7, 11, "/org/bluez/hci0/dev_AA", "Headphones", now)
        .expect("first pairing session");

    assert_eq!(state.phase(), PairingPhase::EnsuringAgent);
    assert_eq!(state.snapshot().pairing_device_name, "Headphones");
    assert_eq!(
        state.begin(2, 7, 11, "/org/bluez/hci0/dev_BB", "Keyboard", now),
        Err(PairingStartError::AlreadyActive)
    );
    assert!(state.agent_registered(&first));
    assert_eq!(state.phase(), PairingPhase::Pairing);
    assert_eq!(
        state.pair_method_reply(&first, true, None),
        PairingCompletion::AwaitingAuthoritativeState
    );
    assert_eq!(state.phase(), PairingPhase::AwaitingAuthoritativePaired);
    assert_eq!(
        state.authoritative_paired(&first, false),
        PairingCompletion::Ignored
    );
    assert_eq!(
        state.authoritative_paired(&first, true),
        PairingCompletion::Succeeded
    );
    assert_eq!(state.phase(), PairingPhase::Idle);
}

#[test]
fn pairing_convergence_beats_a_late_method_failure() {
    let now = Instant::now();
    let mut state = PairingState::default();
    let operation = state
        .begin(1, 7, 11, "/org/bluez/hci0/dev_AA", "Headphones", now)
        .expect("pairing session");
    assert!(state.agent_registered(&operation));
    assert_eq!(
        state.authoritative_paired(&operation, true),
        PairingCompletion::Succeeded
    );
    assert_eq!(
        state.pair_method_reply(&operation, false, Some("authentication failed".to_owned())),
        PairingCompletion::Ignored
    );
    assert_eq!(state.snapshot().pairing_error, None);
}

#[test]
fn pairing_cancel_and_timeout_retire_identity_without_reusing_it() {
    let now = Instant::now();
    let mut state = PairingState::default();
    let first = state
        .begin(1, 7, 11, "/org/bluez/hci0/dev_AA", "Headphones", now)
        .expect("pairing session");
    assert_eq!(state.cancel(&first), PairingCompletion::Canceled);
    assert_eq!(
        state.pair_method_reply(&first, true, None),
        PairingCompletion::Ignored
    );

    let second = state
        .begin(2, 7, 11, "/org/bluez/hci0/dev_AA", "Headphones", now)
        .expect("replacement pairing session");
    assert_ne!(first.pairing_epoch, second.pairing_epoch);
    assert_eq!(
        state.expire(now + Duration::from_secs(120)),
        Some(PairingCompletion::TimedOut)
    );
    assert_eq!(
        state.pair_method_reply(&second, true, None),
        PairingCompletion::Ignored
    );
}

#[test]
fn pairing_failure_is_pairing_specific_and_does_not_leak_secrets() {
    let now = Instant::now();
    let mut state = PairingState::default();
    let operation = state
        .begin(1, 7, 11, "/org/bluez/hci0/dev_AA", "Headphones", now)
        .expect("pairing session");
    assert!(state.agent_registered(&operation));
    assert_eq!(
        state.pair_method_reply(
            &operation,
            false,
            Some("authentication rejected".to_owned())
        ),
        PairingCompletion::Failed
    );
    assert_eq!(
        state.snapshot().pairing_error.as_deref(),
        Some("authentication rejected")
    );
    let debug = format!("{state:?}");
    assert!(!debug.contains("123456"));
}

#[test]
fn trust_waits_for_authoritative_value_and_ignores_late_failure() {
    let now = Instant::now();
    let operation = identity(1, "/org/bluez/hci0/dev_AA");
    let mut state = TrustState::default();
    let pending = state.begin(operation, true, now);
    assert_eq!(
        state.method_reply(&pending, true),
        TrustCompletion::AwaitingAuthoritativeState
    );
    assert_eq!(
        state.authoritative(&pending, false),
        TrustCompletion::Ignored
    );
    assert_eq!(
        state.authoritative(&pending, true),
        TrustCompletion::Succeeded
    );
    assert_eq!(
        state.method_reply(&pending, false),
        TrustCompletion::Ignored
    );

    let second = state.begin(identity(2, "/org/bluez/hci0/dev_AA"), false, now);
    assert_eq!(state.method_reply(&second, false), TrustCompletion::Failed);
}

#[test]
fn forget_waits_for_authoritative_removal_and_is_terminal_per_device() {
    let now = Instant::now();
    let operation = identity(1, "/org/bluez/hci0/dev_AA");
    let mut state = ForgetState::default();
    let pending = state.begin(operation, now).expect("forget operation");
    assert_eq!(
        state.method_reply(&pending, true),
        ForgetCompletion::AwaitingAuthoritativeRemoval
    );
    assert_eq!(
        state.authoritative_removed(&pending),
        ForgetCompletion::Succeeded
    );
    assert_eq!(
        state.method_reply(&pending, false),
        ForgetCompletion::Ignored
    );

    let pending = state
        .begin(identity(2, "/org/bluez/hci0/dev_AA"), now)
        .expect("new forget after removal");
    assert_eq!(
        state.method_reply(&pending, false),
        ForgetCompletion::Failed
    );
    assert_eq!(
        state.authoritative_removed(&pending),
        ForgetCompletion::Ignored
    );
}

#[test]
fn trust_and_forget_deadlines_are_bounded() {
    let now = Instant::now();
    let mut trust = TrustState::default();
    let pending = trust.begin(identity(1, "/org/bluez/hci0/dev_AA"), true, now);
    assert_eq!(pending.deadline, now + OPERATION_TIMEOUT);

    let mut forget = ForgetState::default();
    let pending = forget
        .begin(identity(2, "/org/bluez/hci0/dev_AA"), now)
        .expect("forget operation");
    assert_eq!(pending.deadline, now + OPERATION_TIMEOUT);
}
