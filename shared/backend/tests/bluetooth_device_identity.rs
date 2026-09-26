use astrea_system_backend::bluetooth::device_identity::{
    BluetoothDeviceKind, RawBluetoothDisplayIdentity, resolve_bluetooth_display_identity,
};

fn resolve(
    alias: &str,
    remote_name: &str,
    address: &str,
    object_path: &str,
    icon: &str,
) -> astrea_system_backend::bluetooth::device_identity::BluetoothDisplayIdentity {
    resolve_bluetooth_display_identity(RawBluetoothDisplayIdentity {
        alias,
        remote_name,
        address,
        object_path,
        icon,
    })
}

#[test]
fn explicit_alias_wins_even_when_it_looks_technical() {
    let identity = resolve(
        "LE_4A8C19FE",
        "WH-1000XM3",
        "AA:BB:CC:DD:EE:FF",
        "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF",
        "audio-headphones",
    );

    assert_eq!(identity.display_name, "LE_4A8C19FE");
    assert_eq!(identity.kind, BluetoothDeviceKind::Headphones);
}

#[test]
fn alias_equal_to_name_uses_ordinary_remote_name_resolution() {
    let identity = resolve(
        "LE_WH-1000XM3",
        "LE_WH-1000XM3",
        "AA:BB:CC:DD:EE:FF",
        "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF",
        "audio-headphones",
    );

    assert_eq!(identity.display_name, "WH-1000XM3");
}

#[test]
fn transport_prefixed_human_names_are_normalized_but_tokens_are_rejected() {
    assert_eq!(
        resolve("", "LE_WH-1000XM3", "", "/dev/one", "").display_name,
        "WH-1000XM3"
    );
    assert_eq!(
        resolve("", "LE_Galaxy Buds2 Pro", "", "/dev/two", "").display_name,
        "Galaxy Buds2 Pro"
    );
    assert_eq!(
        resolve("", "BLE_4A8C19FE", "", "/dev/three", "").display_name,
        ""
    );
    assert_eq!(
        resolve("", "LE_4A8C19FE", "", "/dev/four", "").display_name,
        ""
    );
    for technical in [
        "LE_AA:BB:CC:DD:EE:FF",
        "BLE_AA-BB-CC-DD-EE-FF",
        "LE_AA_BB_CC_DD_EE_FF",
        "LE_dev_AA_BB_CC_DD_EE_FF",
    ] {
        assert_eq!(
            resolve("", technical, "AA:BB:CC:DD:EE:FF", "/dev/four", "").display_name,
            "",
            "transport-prefixed identity {technical:?} should remain unresolved"
        );
    }
}

#[test]
fn model_like_names_are_preserved() {
    for name in [
        "WH-1000XM5",
        "MX Master 3S",
        "Galaxy Buds2 Pro",
        "MOMENTUM 4",
        "WH1000XM5_3C91",
        "EPOS IMPACT 1060",
    ] {
        assert_eq!(
            resolve("", name, "", "/dev/model", "").display_name,
            name,
            "name {name:?} should remain unchanged"
        );
    }
}

#[test]
fn address_path_and_generic_labels_do_not_become_concrete_names() {
    let address = "AA:BB:CC:DD:EE:FF";
    let path = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";
    for name in [
        address,
        "AA-BB-CC-DD-EE-FF",
        "AABBCCDDEEFF",
        "dev_AA_BB_CC_DD_EE_FF",
        path,
        "Unknown",
        "Unknown device",
        "Bluetooth device",
        "LE device",
        "BLE device",
    ] {
        assert_eq!(
            resolve("", name, address, path, "").display_name,
            "",
            "technical or generic label {name:?} should be unresolved"
        );
    }
}

#[test]
fn every_supported_bluez_icon_maps_to_its_semantic_kind() {
    for (icon, kind) in [
        ("audio-headset", BluetoothDeviceKind::Headset),
        ("audio-headphones", BluetoothDeviceKind::Headphones),
        ("audio-speakers", BluetoothDeviceKind::Speaker),
        ("audio-card", BluetoothDeviceKind::Audio),
        ("multimedia-player", BluetoothDeviceKind::Audio),
        ("input-keyboard", BluetoothDeviceKind::Keyboard),
        ("input-mouse", BluetoothDeviceKind::Mouse),
        ("input-gaming", BluetoothDeviceKind::Gamepad),
        ("input-tablet", BluetoothDeviceKind::Tablet),
        ("phone", BluetoothDeviceKind::Phone),
        ("computer", BluetoothDeviceKind::Computer),
        ("printer", BluetoothDeviceKind::Printer),
        ("camera-photo", BluetoothDeviceKind::Camera),
        ("camera-video", BluetoothDeviceKind::Camera),
        ("network-wireless", BluetoothDeviceKind::Network),
        ("modem", BluetoothDeviceKind::Network),
    ] {
        assert_eq!(
            resolve("", "", "", "/dev/kind", icon).kind,
            kind,
            "icon {icon:?} should map exactly"
        );
    }
    assert_eq!(
        resolve("", "", "", "/dev/unknown", "vendor-custom-audio-headset").kind,
        BluetoothDeviceKind::Unknown
    );
}

#[test]
fn sanitation_trims_and_collapses_control_whitespace_without_changing_unicode() {
    assert_eq!(
        resolve("", "  Caf\u{e9}\n  Audio\t", "", "/dev/sanitize", "").display_name,
        "Caf\u{e9} Audio"
    );
}
