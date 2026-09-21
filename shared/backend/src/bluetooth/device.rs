use std::collections::BTreeMap;

use super::object_store::{InterfaceMap, PropertyMap, PropertyValue};

const ADAPTER_INTERFACE: &str = "org.bluez.Adapter1";
const DEVICE_INTERFACE: &str = "org.bluez.Device1";
const BATTERY_INTERFACE: &str = "org.bluez.Battery1";

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AdapterInfo {
    pub path: String,
    pub name: String,
    pub powered: bool,
    pub discovering: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct BluetoothDevice {
    pub id: String,
    pub object_path: String,
    pub address: String,
    pub name: String,
    pub paired: bool,
    pub trusted: bool,
    pub connected: bool,
    pub discovered: bool,
    pub icon: String,
    pub rssi: i32,
    pub battery_percent: i32,
}

pub fn adapters(objects: &BTreeMap<String, InterfaceMap>) -> BTreeMap<String, AdapterInfo> {
    objects
        .iter()
        .filter_map(|(path, interfaces)| {
            let properties = interfaces.get(ADAPTER_INTERFACE)?;
            Some((
                path.clone(),
                AdapterInfo {
                    path: path.clone(),
                    name: string_property(properties, "Alias", "Name"),
                    powered: bool_property(properties, "Powered"),
                    discovering: bool_property(properties, "Discovering"),
                },
            ))
        })
        .collect()
}

pub fn select_adapter<'a>(
    adapters: &'a BTreeMap<String, AdapterInfo>,
    current_path: Option<&str>,
) -> Option<&'a AdapterInfo> {
    current_path
        .and_then(|path| adapters.get(path))
        .or_else(|| adapters.values().find(|adapter| adapter.powered))
        .or_else(|| adapters.values().next())
}

pub fn project_devices(
    objects: &BTreeMap<String, InterfaceMap>,
    selected_adapter_path: &str,
) -> Vec<BluetoothDevice> {
    if selected_adapter_path.is_empty() {
        return Vec::new();
    }
    let prefix = format!("{selected_adapter_path}/");
    objects
        .iter()
        .filter(|(path, _)| path.starts_with(&prefix))
        .filter_map(|(path, interfaces)| {
            let properties = interfaces.get(DEVICE_INTERFACE)?;
            let battery = interfaces.get(BATTERY_INTERFACE);
            Some(BluetoothDevice {
                id: path.clone(),
                object_path: path.clone(),
                address: string_property(properties, "Address", ""),
                name: string_property(properties, "Alias", "Name"),
                paired: bool_property(properties, "Paired"),
                trusted: bool_property(properties, "Trusted"),
                connected: bool_property(properties, "Connected"),
                discovered: true,
                icon: string_property(properties, "Icon", ""),
                rssi: integer_property(properties, "RSSI").unwrap_or(-1),
                battery_percent: battery
                    .and_then(|properties| integer_property(properties, "Percentage"))
                    .unwrap_or(-1),
            })
        })
        .collect()
}

fn string_property(properties: &PropertyMap, primary: &str, fallback: &str) -> String {
    let value = match properties.get(primary) {
        Some(PropertyValue::String(value)) => value,
        _ => "",
    };
    if value.is_empty() && !fallback.is_empty() {
        match properties.get(fallback) {
            Some(PropertyValue::String(value)) => value.clone(),
            _ => String::new(),
        }
    } else {
        value.to_owned()
    }
}

fn bool_property(properties: &PropertyMap, name: &str) -> bool {
    matches!(properties.get(name), Some(PropertyValue::Boolean(true)))
}

fn integer_property(properties: &PropertyMap, name: &str) -> Option<i32> {
    match properties.get(name) {
        Some(PropertyValue::Integer(value)) => Some(*value),
        _ => None,
    }
}
