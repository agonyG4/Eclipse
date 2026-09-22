use astrea_system_backend::bluetooth::agent::{AgentPromptKind, AgentPromptView};
use astrea_system_backend::bluetooth::device::BluetoothDevice;
use astrea_system_backend::bluetooth::discovery::DiscoveryReply;
use astrea_system_backend::bluetooth::engine::{BluetoothCore, CoreAction, ServiceState};
use astrea_system_backend::bluetooth::object_store::{InterfaceMap, PropertyMap, PropertyValue};
use std::collections::BTreeMap;
use std::time::Instant;

fn props(entries: impl IntoIterator<Item = (&'static str, PropertyValue)>) -> PropertyMap {
    entries
        .into_iter()
        .map(|(key, value)| (String::from(key), value))
        .collect()
}

fn snapshot_objects() -> BTreeMap<String, InterfaceMap> {
    BTreeMap::from([
        (
            String::from("/org/bluez/hci0"),
            InterfaceMap::from([(
                String::from("org.bluez.Adapter1"),
                props([
                    ("Alias", PropertyValue::String(String::from("Astrea"))),
                    ("Powered", PropertyValue::Boolean(true)),
                    ("Discovering", PropertyValue::Boolean(false)),
                ]),
            )]),
        ),
        (
            String::from("/org/bluez/hci0/dev_AA"),
            InterfaceMap::from([(
                String::from("org.bluez.Device1"),
                props([
                    ("Address", PropertyValue::String(String::from("AA"))),
                    ("Paired", PropertyValue::Boolean(true)),
                    ("Alias", PropertyValue::String(String::from("Headphones"))),
                    ("Connected", PropertyValue::Boolean(false)),
                ]),
            )]),
        ),
    ])
}

fn unpaired_snapshot_objects() -> BTreeMap<String, InterfaceMap> {
    let mut objects = snapshot_objects();
    let device = objects
        .get_mut("/org/bluez/hci0/dev_AA")
        .and_then(|interfaces| interfaces.get_mut("org.bluez.Device1"))
        .expect("fixture device");
    device.insert(String::from("Paired"), PropertyValue::Boolean(false));
    device.insert(String::from("Trusted"), PropertyValue::Boolean(false));
    objects
}

fn start_ready(core: &mut BluetoothCore) -> (u64, u64) {
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start must open BlueZ discovery");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("registered owner must be probed");
    };
    assert!(
        core.managed_objects(session_generation, bluez_generation, snapshot_objects())
            .is_some()
    );
    (session_generation, bluez_generation)
}

#[test]
fn daemon_owner_change_invalidates_objects_and_all_older_events() {
    let mut core = BluetoothCore::default();
    let (session, old_bluez) = start_ready(&mut core);
    let old_device: BluetoothDevice = core.snapshot().devices[0].clone();
    assert_eq!(old_device.name, "Headphones");

    let vanished = core.owner_changed(session, None);
    assert!(vanished.is_empty());
    assert_eq!(core.snapshot().state, ServiceState::Unavailable);
    assert!(!core.snapshot().adapter_available);
    assert!(core.snapshot().devices.is_empty());
    assert!(
        core.managed_objects(session, old_bluez, snapshot_objects())
            .is_none()
    );
    assert!(!core.interfaces_added(session, old_bluez, "/org/bluez/hci9", InterfaceMap::new()));
}

#[test]
fn daemon_return_creates_a_new_generation_and_resumes_scan_demand() {
    let mut core = BluetoothCore::default();
    let (session, first_generation) = start_ready(&mut core);
    let now = Instant::now();
    let start_scan = core.request_scan("topbar", now).expect("discovery start");
    let CoreAction::Discovery { operation, .. } = start_scan else {
        panic!("discovery action");
    };
    core.discovery_reply(session, first_generation, operation.id, true, now, None);
    core.owner_changed(session, None);

    let returned = core.owner_changed(session, Some(String::from(":1.84")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = returned[0]
    else {
        panic!("new probe");
    };
    assert_ne!(first_generation, bluez_generation);
    assert!(
        core.managed_objects(session, bluez_generation, snapshot_objects())
            .is_some()
    );
    let resumed = core.on_timer(now);
    assert!(matches!(
        resumed.first(),
        Some(CoreAction::Discovery { .. })
    ));
}

#[test]
fn stale_getall_reply_and_stale_session_events_cannot_replace_new_state() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let (_, refresh) = core.properties_changed(
        session,
        generation,
        "/org/bluez/hci0/dev_AA",
        "org.bluez.Device1",
        PropertyMap::default(),
        &[String::from("Alias")],
    );
    let Some(CoreAction::GetAll { owner, token, .. }) = refresh else {
        panic!("refresh queued");
    };
    core.properties_changed(
        session,
        generation,
        "/org/bluez/hci0/dev_AA",
        "org.bluez.Device1",
        props([("Alias", PropertyValue::String(String::from("Newer name")))]),
        &[],
    );
    assert!(!core.refresh_reply(
        session,
        &owner,
        &token,
        props([("Alias", PropertyValue::String(String::from("Old name")))]),
    ));
    assert_eq!(core.snapshot().devices[0].name, "Newer name");

    core.stop();
    core.start();
    assert!(
        core.managed_objects(session, generation, snapshot_objects())
            .is_none()
    );
}

#[test]
fn replacing_the_selected_adapter_retires_old_power_and_discovery_work() {
    let mut core = BluetoothCore::default();
    let (session, bluez_generation) = start_ready(&mut core);
    let now = Instant::now();
    let power = core.set_powered(false, now).expect("power operation");
    let CoreAction::SetPowered { operation_id, .. } = power else {
        panic!("power action");
    };
    let scan = core
        .request_scan("topbar", now)
        .expect("discovery operation");
    let CoreAction::Discovery { operation, .. } = scan else {
        panic!("discovery action");
    };

    assert!(core.interfaces_removed(
        session,
        bluez_generation,
        "/org/bluez/hci0",
        &[String::from("org.bluez.Adapter1")],
    ));
    assert!(!core.snapshot().power_pending);
    assert_eq!(
        core.discovery_reply(session, bluez_generation, operation.id, true, now, None,)
            .0,
        DiscoveryReply::Ignored
    );
    assert!(!core.power_reply(session, bluez_generation, operation_id, true, None,));

    assert!(core.interfaces_added(
        session,
        bluez_generation,
        "/org/bluez/hci1",
        InterfaceMap::from([(
            String::from("org.bluez.Adapter1"),
            props([
                ("Alias", PropertyValue::String(String::from("Replacement"))),
                ("Powered", PropertyValue::Boolean(true)),
                ("Discovering", PropertyValue::Boolean(false)),
            ]),
        )]),
    ));
    assert_eq!(core.snapshot().adapter_path, "/org/bluez/hci1");
    assert!(!core.snapshot().power_pending);
}

#[test]
fn newer_start_generation_replaces_a_running_session() {
    let mut core = BluetoothCore::default();
    let (old_session, old_bluez_generation) = start_ready(&mut core);
    assert!(!core.snapshot().devices.is_empty());

    let new_session = old_session + 2;
    assert!(core.start_generation(new_session));
    assert_eq!(core.snapshot().state, ServiceState::Starting);
    assert!(core.snapshot().devices.is_empty());
    assert!(
        core.managed_objects(old_session, old_bluez_generation, snapshot_objects())
            .is_none()
    );

    let probe = core.owner_changed(new_session, Some(String::from(":1.84")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("replacement session must probe BlueZ");
    };
    assert!(
        core.managed_objects(new_session, bluez_generation, snapshot_objects())
            .is_some()
    );
}

#[test]
fn initial_snapshot_replays_events_received_while_probe_is_pending() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start opens BlueZ");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("registered owner is probed");
    };

    assert!(
        core.properties_changed(
            session_generation,
            bluez_generation,
            "/org/bluez/hci0/dev_AA",
            "org.bluez.Device1",
            props([("Alias", PropertyValue::String(String::from("Newer name")))]),
            &[],
        )
        .0
    );
    assert!(core.interfaces_added(
        session_generation,
        bluez_generation,
        "/org/bluez/hci0/dev_BB",
        InterfaceMap::from([(
            String::from("org.bluez.Device1"),
            props([
                ("Address", PropertyValue::String(String::from("BB"))),
                ("Paired", PropertyValue::Boolean(false)),
                ("Alias", PropertyValue::String(String::from("Keyboard"))),
                ("Connected", PropertyValue::Boolean(false)),
            ]),
        )]),
    ));
    assert!(core.snapshot().devices.is_empty());

    let actions = core
        .managed_objects(session_generation, bluez_generation, snapshot_objects())
        .expect("current probe reply is accepted");
    assert!(actions.is_empty());
    assert_eq!(core.snapshot().state, ServiceState::Ready);
    assert!(
        core.snapshot()
            .devices
            .iter()
            .any(|device| device.name == "Newer name")
    );
    assert!(
        core.snapshot()
            .devices
            .iter()
            .any(|device| device.address == "BB")
    );
}

#[test]
fn overflowing_initial_snapshot_event_buffer_restarts_the_probe() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start opens BlueZ");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("registered owner is probed");
    };

    for index in 0..129 {
        assert!(core.interfaces_added(
            session_generation,
            bluez_generation,
            &format!("/org/bluez/hci0/dev_{index}"),
            InterfaceMap::new(),
        ));
    }
    let actions = core
        .managed_objects(session_generation, bluez_generation, snapshot_objects())
        .expect("current probe reply is accepted");
    assert!(matches!(
        actions.as_slice(),
        [CoreAction::Probe { bluez_generation: generation, .. }]
            if *generation == bluez_generation
    ));
    assert_eq!(core.snapshot().state, ServiceState::Starting);
    assert!(core.snapshot().devices.is_empty());
}

#[test]
fn daemon_generation_change_discards_events_buffered_for_the_old_probe() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start opens BlueZ");
    };
    let first_probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation: first_generation,
        ..
    } = first_probe[0]
    else {
        panic!("first owner is probed");
    };
    assert!(core.interfaces_added(
        session_generation,
        first_generation,
        "/org/bluez/hci0/dev_OLD",
        InterfaceMap::from([(
            String::from("org.bluez.Device1"),
            props([
                ("Address", PropertyValue::String(String::from("OLD"))),
                ("Paired", PropertyValue::Boolean(true)),
            ]),
        )]),
    ));

    core.owner_changed(session_generation, None);
    let second_probe = core.owner_changed(session_generation, Some(String::from(":1.84")));
    let CoreAction::Probe {
        bluez_generation: second_generation,
        ..
    } = second_probe[0]
    else {
        panic!("replacement owner is probed");
    };
    assert_ne!(first_generation, second_generation);
    assert!(
        core.managed_objects(session_generation, second_generation, snapshot_objects())
            .is_some()
    );
    assert!(
        core.snapshot()
            .devices
            .iter()
            .all(|device| device.address != "OLD")
    );
}

#[test]
fn failed_initial_snapshot_reports_unavailable_with_error() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start opens the D-Bus session");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("registered owner is probed");
    };

    assert!(core.operation_failed(
        session_generation,
        bluez_generation,
        Some(String::from("snapshot failed"))
    ));
    assert_eq!(core.snapshot().state, ServiceState::Unavailable);
    assert!(!core.snapshot().available);
    assert_eq!(core.snapshot().error.as_deref(), Some("snapshot failed"));
}

#[test]
fn connect_policy_rejects_unpaired_devices_and_does_not_optimistically_change_state() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let action = core.connect_device("/org/bluez/hci0/dev_AA", true);
    assert!(matches!(
        action,
        Some(CoreAction::Connect { connect: true, .. })
    ));
    assert!(!core.snapshot().devices[0].connected);

    core.interfaces_added(
        session,
        generation,
        "/org/bluez/hci0/dev_BB",
        InterfaceMap::from([(
            String::from("org.bluez.Device1"),
            props([
                ("Address", PropertyValue::String(String::from("BB"))),
                ("Paired", PropertyValue::Boolean(false)),
            ]),
        )]),
    );
    assert!(
        core.connect_device("/org/bluez/hci0/dev_BB", true)
            .is_none()
    );
}

#[test]
fn stale_connect_failure_after_newer_disconnect_is_ignored() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let connect_action = core
        .connect_device("/org/bluez/hci0/dev_AA", true)
        .expect("connect action");
    let disconnect = core
        .connect_device("/org/bluez/hci0/dev_AA", false)
        .expect("disconnect action");
    let CoreAction::Connect {
        operation_id: connect_id,
        object_path: connect_path,
        connect: connect_target,
        session_generation: connect_session,
        bluez_generation: connect_generation,
        ..
    } = connect_action
    else {
        panic!("connect action");
    };
    assert!(!core.connect_reply(
        connect_session,
        connect_generation,
        connect_id,
        &connect_path,
        connect_target,
        false,
        Some(String::from("stale connect failure")),
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);

    let CoreAction::Connect {
        operation_id,
        object_path,
        connect,
        session_generation,
        bluez_generation,
        ..
    } = disconnect
    else {
        panic!("disconnect action");
    };
    assert!(core.connect_reply(
        session,
        generation,
        operation_id,
        &object_path,
        connect,
        false,
        Some(String::from("current disconnect failure")),
    ));
    assert_eq!(session_generation, session);
    assert_eq!(bluez_generation, generation);
    assert_eq!(core.snapshot().state, ServiceState::Degraded);
}

#[test]
fn stale_disconnect_failure_after_newer_connect_is_ignored() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let disconnect = core
        .connect_device("/org/bluez/hci0/dev_AA", false)
        .expect("disconnect action");
    let connect_action = core
        .connect_device("/org/bluez/hci0/dev_AA", true)
        .expect("connect action");
    let CoreAction::Connect {
        operation_id,
        object_path,
        connect: disconnect_target,
        ..
    } = disconnect
    else {
        panic!("disconnect action");
    };
    assert!(!core.connect_reply(
        session,
        generation,
        operation_id,
        &object_path,
        disconnect_target,
        false,
        Some(String::from("stale disconnect failure")),
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);

    let CoreAction::Connect {
        operation_id,
        object_path,
        connect,
        ..
    } = connect_action
    else {
        panic!("connect action");
    };
    assert!(!core.connect_reply(
        session,
        generation,
        operation_id,
        &object_path,
        connect,
        true,
        None,
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);
}

#[test]
fn authoritative_connected_convergence_ignores_late_method_failure() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let action = core
        .connect_device("/org/bluez/hci0/dev_AA", true)
        .expect("connect action");
    let CoreAction::Connect {
        operation_id,
        object_path,
        connect,
        ..
    } = action
    else {
        panic!("connect action");
    };

    let (accepted, refresh) = core.properties_changed(
        session,
        generation,
        &object_path,
        "org.bluez.Device1",
        props([("Connected", PropertyValue::Boolean(true))]),
        &[],
    );
    assert!(accepted);
    assert!(refresh.is_none());
    assert!(core.snapshot().devices[0].connected);
    assert!(!core.connect_reply(
        session,
        generation,
        operation_id,
        &object_path,
        connect,
        false,
        Some(String::from("late failure")),
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);
}

#[test]
fn device_operations_are_independent_and_retired_by_generation_changes() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    core.interfaces_added(
        session,
        generation,
        "/org/bluez/hci0/dev_BB",
        InterfaceMap::from([(
            String::from("org.bluez.Device1"),
            props([
                ("Address", PropertyValue::String(String::from("BB"))),
                ("Paired", PropertyValue::Boolean(true)),
                ("Connected", PropertyValue::Boolean(false)),
            ]),
        )]),
    );
    let first = core
        .connect_device("/org/bluez/hci0/dev_AA", true)
        .expect("first action");
    let second = core
        .connect_device("/org/bluez/hci0/dev_BB", true)
        .expect("second action");
    let (first_id, first_path, first_target) = match first {
        CoreAction::Connect {
            operation_id,
            object_path,
            connect,
            ..
        } => (operation_id, object_path, connect),
        _ => panic!("first connect action"),
    };
    let (second_id, second_path, second_target) = match second {
        CoreAction::Connect {
            operation_id,
            object_path,
            connect,
            ..
        } => (operation_id, object_path, connect),
        _ => panic!("second connect action"),
    };
    assert!(core.connect_reply(
        session,
        generation,
        first_id,
        &first_path,
        first_target,
        false,
        Some(String::from("first failure")),
    ));
    assert!(!core.connect_reply(
        session,
        generation,
        second_id,
        &second_path,
        second_target,
        true,
        None,
    ));

    let newer_session = session + 1;
    assert!(core.stop_generation(newer_session));
    assert!(!core.connect_reply(
        session,
        generation,
        second_id,
        &second_path,
        second_target,
        false,
        Some(String::from("stale after stop")),
    ));
}

#[test]
fn connection_loss_retires_pending_device_operations() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let action = core
        .connect_device("/org/bluez/hci0/dev_AA", true)
        .expect("connect action");
    let CoreAction::Connect {
        operation_id,
        object_path,
        connect,
        ..
    } = action
    else {
        panic!("connect action");
    };

    assert!(core.connection_lost(session, String::from("connection lost")));
    assert_eq!(core.snapshot().state, ServiceState::Unavailable);
    assert!(!core.connect_reply(
        session,
        generation,
        operation_id,
        &object_path,
        connect,
        false,
        Some(String::from("stale failure")),
    ));
}

#[test]
fn pair_policy_requires_authoritative_unpaired_selected_device_and_registers_first() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start action");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("probe action");
    };
    core.managed_objects(
        session_generation,
        bluez_generation,
        unpaired_snapshot_objects(),
    );

    let register = core
        .pair_device("/org/bluez/hci0/dev_AA")
        .expect("unpaired selected device can pair");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        device_path,
        ..
    } = register
    else {
        panic!("Pair must be preceded by RegisterAgent");
    };
    assert!(core.snapshot().pairing);
    assert_eq!(core.snapshot().pairing_device_path, device_path);
    assert!(core.pair_device("/org/bluez/hci0/dev_AA").is_none());

    let pair = core
        .agent_registered(
            session_generation,
            bluez_generation,
            operation_id,
            pairing_epoch,
        )
        .expect("registered agent produces Pair");
    assert!(matches!(pair, CoreAction::Pair { .. }));

    assert!(core.pair_device("/org/bluez/hci0/dev_unknown").is_none());
    assert!(
        core.pair_device("/org/bluez/hci1/dev_AA_BB_CC_DD_EE_FF")
            .is_none()
    );
}

#[test]
fn pairing_waits_for_authoritative_paired_and_keeps_user_errors_local() {
    let mut core = BluetoothCore::default();
    let (session, generation) = {
        let open = core.start().expect("start succeeds");
        let CoreAction::StartBus { session_generation } = open else {
            panic!("start action");
        };
        let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
        let CoreAction::Probe {
            bluez_generation, ..
        } = probe[0]
        else {
            panic!("probe action");
        };
        core.managed_objects(
            session_generation,
            bluez_generation,
            unpaired_snapshot_objects(),
        );
        (session_generation, bluez_generation)
    };
    let register = core
        .pair_device("/org/bluez/hci0/dev_AA")
        .expect("register");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        ..
    } = register
    else {
        panic!("register action");
    };
    let pair = core
        .agent_registered(session, generation, operation_id, pairing_epoch)
        .expect("pair action");
    let CoreAction::Pair { device_path, .. } = pair else {
        panic!("pair action");
    };
    assert!(!core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        &device_path,
        true,
        None,
    ));
    assert!(core.snapshot().pairing);
    assert!(
        core.properties_changed(
            session,
            generation,
            &device_path,
            "org.bluez.Device1",
            props([("Paired", PropertyValue::Boolean(true))]),
            &[],
        )
        .0
    );
    assert!(!core.snapshot().pairing);
    assert_eq!(core.snapshot().pairing_error, None);
    assert!(!core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        &device_path,
        false,
        Some(String::from("late authentication failure")),
    ));

    let register = core.pair_device(&device_path);
    assert!(
        register.is_none(),
        "authoritative Paired rejects a second Pair"
    );
}

#[test]
fn active_agent_prompt_must_match_the_current_pairing_identity() {
    let mut core = BluetoothCore::default();
    let open = core.start().expect("start succeeds");
    let CoreAction::StartBus { session_generation } = open else {
        panic!("start action");
    };
    let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
    let CoreAction::Probe {
        bluez_generation, ..
    } = probe[0]
    else {
        panic!("probe action");
    };
    core.managed_objects(
        session_generation,
        bluez_generation,
        unpaired_snapshot_objects(),
    );
    let register = core
        .pair_device("/org/bluez/hci0/dev_AA")
        .expect("agent registration");
    let CoreAction::RegisterAgent { pairing_epoch, .. } = register else {
        panic!("registration action");
    };

    let valid_prompt = AgentPromptView {
        active: true,
        request_id: 19,
        pairing_epoch,
        session_generation,
        bluez_generation,
        kind: AgentPromptKind::PasskeyInput,
        device_path: String::from("/org/bluez/hci0/dev_AA"),
        ..AgentPromptView::default()
    };
    assert!(core.agent_prompt_changed(session_generation, bluez_generation, valid_prompt.clone()));

    let mut stale_epoch = valid_prompt.clone();
    stale_epoch.pairing_epoch += 1;
    assert!(!core.agent_prompt_changed(session_generation, bluez_generation, stale_epoch));
    assert_eq!(core.snapshot().agent_request_id, valid_prompt.request_id);

    let mut stale_session = valid_prompt.clone();
    stale_session.session_generation += 1;
    assert!(!core.agent_prompt_changed(session_generation, bluez_generation, stale_session));
    let mut stale_bluez = valid_prompt.clone();
    stale_bluez.bluez_generation += 1;
    assert!(!core.agent_prompt_changed(session_generation, bluez_generation, stale_bluez));

    let mut stale_device = valid_prompt.clone();
    stale_device.device_path = String::from("/org/bluez/hci0/dev_BB");
    assert!(!core.agent_prompt_changed(session_generation, bluez_generation, stale_device));
    assert_eq!(core.snapshot().agent_request_id, valid_prompt.request_id);

    assert!(core.cancel_pairing().is_some());
    assert!(!core.agent_prompt_changed(session_generation, bluez_generation, valid_prompt));
    assert!(!core.snapshot().agent_request_active);
}

#[test]
fn pairing_failure_cancel_and_generation_replacement_do_not_degrade_service() {
    let mut core = BluetoothCore::default();
    let (session, generation) = {
        let open = core.start().expect("start succeeds");
        let CoreAction::StartBus { session_generation } = open else {
            panic!("start action");
        };
        let probe = core.owner_changed(session_generation, Some(String::from(":1.42")));
        let CoreAction::Probe {
            bluez_generation, ..
        } = probe[0]
        else {
            panic!("probe action");
        };
        core.managed_objects(
            session_generation,
            bluez_generation,
            unpaired_snapshot_objects(),
        );
        (session_generation, bluez_generation)
    };
    let register = core
        .pair_device("/org/bluez/hci0/dev_AA")
        .expect("register");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        ..
    } = register
    else {
        panic!("register action");
    };
    let _pair = core
        .agent_registered(session, generation, operation_id, pairing_epoch)
        .expect("pair action");
    assert!(core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        "/org/bluez/hci0/dev_AA",
        false,
        Some(String::from("authentication rejected")),
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);
    assert_eq!(
        core.snapshot().pairing_error.as_deref(),
        Some("authentication rejected")
    );

    let register = core.pair_device("/org/bluez/hci0/dev_AA").expect("retry");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        ..
    } = register
    else {
        panic!("register action");
    };
    let cancel = core.cancel_pairing().expect("cancel action");
    assert!(matches!(cancel, CoreAction::CancelPairing { .. }));
    assert!(!core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        "/org/bluez/hci0/dev_AA",
        true,
        None,
    ));
    assert_eq!(core.snapshot().state, ServiceState::Ready);

    let register = core.pair_device("/org/bluez/hci0/dev_AA").expect("retry");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        ..
    } = register
    else {
        panic!("register action");
    };
    let timeout_actions = core.on_timer(Instant::now() + std::time::Duration::from_secs(121));
    assert!(matches!(
        timeout_actions.as_slice(),
        [CoreAction::CancelPairing { .. }]
    ));
    assert!(!core.snapshot().pairing);
    assert_eq!(
        core.snapshot().pairing_error.as_deref(),
        Some("Bluetooth pairing timed out")
    );
    assert!(!core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        "/org/bluez/hci0/dev_AA",
        true,
        None,
    ));

    let register = core.pair_device("/org/bluez/hci0/dev_AA").expect("retry");
    let CoreAction::RegisterAgent {
        operation_id,
        pairing_epoch,
        ..
    } = register
    else {
        panic!("register action");
    };
    core.owner_changed(session, None);
    assert!(!core.snapshot().pairing);
    assert!(!core.pair_reply(
        session,
        generation,
        operation_id,
        pairing_epoch,
        "/org/bluez/hci0/dev_AA",
        false,
        Some(String::from("stale")),
    ));
}

#[test]
fn trust_converges_authoritatively_and_forget_waits_for_removal() {
    let mut core = BluetoothCore::default();
    let (session, generation) = start_ready(&mut core);
    let trust = core
        .set_device_trusted("/org/bluez/hci0/dev_AA", true)
        .expect("paired device can be trusted");
    let CoreAction::SetTrusted {
        operation_id,
        object_path,
        target,
        ..
    } = trust
    else {
        panic!("trusted action");
    };
    assert!(!core.trusted_reply(
        session,
        generation,
        operation_id,
        &object_path,
        target,
        true,
        None,
    ));
    assert!(
        core.properties_changed(
            session,
            generation,
            &object_path,
            "org.bluez.Device1",
            props([("Trusted", PropertyValue::Boolean(true))]),
            &[],
        )
        .0
    );
    assert!(core.snapshot().devices[0].trusted);
    assert!(!core.trusted_reply(
        session,
        generation,
        operation_id,
        &object_path,
        target,
        false,
        Some(String::from("late trust failure")),
    ));

    let forget = core
        .forget_device(&object_path)
        .expect("selected authoritative device can be forgotten");
    let CoreAction::RemoveDevice {
        operation_id,
        object_path,
        ..
    } = forget
    else {
        panic!("forget action");
    };
    assert!(core.set_device_trusted(&object_path, false).is_none());
    assert!(!core.forget_reply(session, generation, operation_id, &object_path, true, None,));
    assert_eq!(core.snapshot().devices.len(), 1);
    assert!(core.interfaces_removed(
        session,
        generation,
        &object_path,
        &[String::from("org.bluez.Device1")],
    ));
    assert!(core.snapshot().devices.is_empty());
    assert!(!core.forget_reply(
        session,
        generation,
        operation_id,
        &object_path,
        false,
        Some(String::from("late remove failure")),
    ));

    let mut unpaired = BluetoothCore::default();
    let (unpaired_session, unpaired_generation) = {
        let open = unpaired.start().expect("start succeeds");
        let CoreAction::StartBus { session_generation } = open else {
            panic!("start action");
        };
        let probe = unpaired.owner_changed(session_generation, Some(String::from(":1.42")));
        let CoreAction::Probe {
            bluez_generation, ..
        } = probe[0]
        else {
            panic!("probe action");
        };
        unpaired.managed_objects(
            session_generation,
            bluez_generation,
            unpaired_snapshot_objects(),
        );
        (session_generation, bluez_generation)
    };
    assert!(
        unpaired
            .set_device_trusted("/org/bluez/hci0/dev_AA", true)
            .is_none()
    );
    assert!(
        unpaired
            .set_device_trusted("/org/bluez/hci0/dev_unknown", true)
            .is_none()
    );
    assert!(
        unpaired
            .properties_changed(
                unpaired_session,
                unpaired_generation,
                "/org/bluez/hci0/dev_AA",
                "org.bluez.Device1",
                props([("Paired", PropertyValue::Boolean(true))]),
                &[],
            )
            .0
    );
}
