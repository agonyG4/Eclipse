use astrea_system_backend::bluetooth::device::{
    AdapterInfo, BluetoothDevice, project_devices, select_adapter,
};
use astrea_system_backend::bluetooth::object_store::{InterfaceMap, PropertyMap, PropertyValue};
use std::collections::BTreeMap;

fn properties(entries: impl IntoIterator<Item = (&'static str, PropertyValue)>) -> PropertyMap {
    entries
        .into_iter()
        .map(|(name, value)| (String::from(name), value))
        .collect()
}

fn object(interfaces: impl IntoIterator<Item = (&'static str, PropertyMap)>) -> InterfaceMap {
    interfaces
        .into_iter()
        .map(|(name, properties)| (String::from(name), properties))
        .collect()
}

#[test]
fn adapter_selection_keeps_current_then_prefers_powered_then_uses_first_path() {
    let adapters = BTreeMap::from([
        (
            String::from("/org/bluez/hci0"),
            AdapterInfo {
                path: String::from("/org/bluez/hci0"),
                name: String::from("Off"),
                powered: false,
                discovering: false,
            },
        ),
        (
            String::from("/org/bluez/hci1"),
            AdapterInfo {
                path: String::from("/org/bluez/hci1"),
                name: String::from("On"),
                powered: true,
                discovering: false,
            },
        ),
    ]);

    assert_eq!(
        select_adapter(&adapters, Some("/org/bluez/hci0")).map(|adapter| adapter.path.as_str()),
        Some("/org/bluez/hci0")
    );
    assert_eq!(
        select_adapter(&adapters, Some("/org/bluez/hci9")).map(|adapter| adapter.path.as_str()),
        Some("/org/bluez/hci1")
    );
    let unpowered = adapters
        .into_iter()
        .map(|(path, mut adapter)| {
            adapter.powered = false;
            (path, adapter)
        })
        .collect();
    assert_eq!(
        select_adapter(&unpowered, None).map(|adapter| adapter.path.as_str()),
        Some("/org/bluez/hci0")
    );
}

#[test]
fn device_projection_keeps_selected_adapter_scope_and_bluez_fields() {
    let objects = BTreeMap::from([
        (
            String::from("/org/bluez/hci0"),
            object([(
                "org.bluez.Adapter1",
                properties([
                    ("Alias", PropertyValue::String(String::from("Desk"))),
                    ("Powered", PropertyValue::Boolean(true)),
                    ("Discovering", PropertyValue::Boolean(true)),
                ]),
            )]),
        ),
        (
            String::from("/org/bluez/hci0/dev_AA"),
            object([
                (
                    "org.bluez.Device1",
                    properties([
                        ("Address", PropertyValue::String(String::from("AA:BB"))),
                        ("Alias", PropertyValue::String(String::from("Headphones"))),
                        ("Name", PropertyValue::String(String::from("Fallback"))),
                        ("Paired", PropertyValue::Boolean(true)),
                        ("Trusted", PropertyValue::Boolean(true)),
                        ("Connected", PropertyValue::Boolean(true)),
                        (
                            "Icon",
                            PropertyValue::String(String::from("audio-headphones")),
                        ),
                        ("RSSI", PropertyValue::Integer(-42)),
                    ]),
                ),
                (
                    "org.bluez.Battery1",
                    properties([("Percentage", PropertyValue::Integer(80))]),
                ),
            ]),
        ),
        (
            String::from("/org/bluez/hci1/dev_BB"),
            object([(
                "org.bluez.Device1",
                properties([("Address", PropertyValue::String(String::from("CC:DD")))]),
            )]),
        ),
    ]);

    let devices = project_devices(&objects, "/org/bluez/hci0");
    assert_eq!(devices.len(), 1);
    assert_eq!(
        devices[0],
        BluetoothDevice {
            id: String::from("/org/bluez/hci0/dev_AA"),
            object_path: String::from("/org/bluez/hci0/dev_AA"),
            address: String::from("AA:BB"),
            name: String::from("Headphones"),
            paired: true,
            trusted: true,
            connected: true,
            discovered: true,
            icon: String::from("audio-headphones"),
            rssi: -42,
            battery_percent: 80,
        }
    );
}

#[test]
fn device_projection_uses_name_fallback_and_unknown_metrics() {
    let objects = BTreeMap::from([(
        String::from("/org/bluez/hci2/dev_CC"),
        object([(
            "org.bluez.Device1",
            properties([
                ("Address", PropertyValue::String(String::from("11:22"))),
                ("Alias", PropertyValue::String(String::new())),
                ("Name", PropertyValue::String(String::from("Keyboard"))),
                ("RSSI", PropertyValue::Unsupported),
            ]),
        )]),
    )]);

    let devices = project_devices(&objects, "/org/bluez/hci2");
    assert_eq!(devices[0].name, "Keyboard");
    assert_eq!(devices[0].rssi, -1);
    assert_eq!(devices[0].battery_percent, -1);
    assert!(!devices[0].paired);
    assert!(!devices[0].connected);
}
