use astrea_system_backend::bluetooth::object_store::{
    InterfaceMap, ObjectStore, PropertyMap, PropertyValue,
};
use std::collections::BTreeMap;

fn properties(name: &str, value: PropertyValue) -> PropertyMap {
    BTreeMap::from([(String::from(name), value)])
}

fn interfaces(entries: impl IntoIterator<Item = (String, PropertyMap)>) -> InterfaceMap {
    entries.into_iter().collect()
}

#[test]
fn interfaces_added_merges_interfaces_and_properties_changed_patches_them() {
    let mut store = ObjectStore::default();
    let path = "/org/bluez/hci0/dev_AA";
    store.interfaces_added(
        path,
        interfaces([
            (
                String::from("org.bluez.Device1"),
                properties("Paired", PropertyValue::Boolean(true)),
            ),
            (
                String::from("org.bluez.Battery1"),
                properties("Percentage", PropertyValue::Integer(80)),
            ),
        ]),
    );

    assert_eq!(store.objects()[path].len(), 2);
    assert!(store.properties_changed(
        path,
        "org.bluez.Device1",
        properties("Connected", PropertyValue::Boolean(true)),
        &[],
    ));
    assert_eq!(
        store.objects()[path]["org.bluez.Device1"]["Connected"],
        PropertyValue::Boolean(true)
    );
}

#[test]
fn stale_invalidated_property_reply_is_rejected_after_properties_changed() {
    let mut store = ObjectStore::default();
    let path = "/org/bluez/hci0/dev_AA";
    let interface = "org.bluez.Device1";
    store.interfaces_added(
        path,
        interfaces([(
            String::from(interface),
            properties("Connected", PropertyValue::Boolean(true)),
        )]),
    );
    let token = store
        .begin_refresh(path, interface)
        .expect("interface exists");

    assert!(store.properties_changed(
        path,
        interface,
        properties("Connected", PropertyValue::Boolean(false)),
        &[],
    ));
    assert!(!store.replace_interface_if_revision(
        &token,
        properties("Connected", PropertyValue::Boolean(true)),
    ));
    assert_eq!(
        store.objects()[path][interface]["Connected"],
        PropertyValue::Boolean(false)
    );
}

#[test]
fn invalidated_property_is_removed_and_refreshed_against_its_revision() {
    let mut store = ObjectStore::default();
    let path = "/org/bluez/hci0/dev_AA";
    let interface = "org.bluez.Device1";
    store.interfaces_added(
        path,
        interfaces([(
            String::from(interface),
            properties("RSSI", PropertyValue::Integer(-20)),
        )]),
    );

    assert!(store.properties_changed(
        path,
        interface,
        PropertyMap::default(),
        &[String::from("RSSI")],
    ));
    assert!(!store.objects()[path][interface].contains_key("RSSI"));
    let token = store
        .begin_refresh(path, interface)
        .expect("interface remains");
    assert!(store.properties_changed(
        path,
        interface,
        properties("RSSI", PropertyValue::Integer(-40)),
        &[],
    ));
    assert!(!store.replace_interface_if_revision(
        &token,
        properties("RSSI", PropertyValue::Integer(-30)),
    ));
    assert_eq!(
        store.objects()[path][interface]["RSSI"],
        PropertyValue::Integer(-40)
    );
}

#[test]
fn stale_refresh_is_rejected_after_interface_removal_and_recreation() {
    let mut store = ObjectStore::default();
    let path = "/org/bluez/hci0/dev_AA";
    let interface = "org.bluez.Device1";
    store.interfaces_added(
        path,
        interfaces([(
            String::from(interface),
            properties("Connected", PropertyValue::Boolean(true)),
        )]),
    );
    let token = store
        .begin_refresh(path, interface)
        .expect("interface exists");

    store.interfaces_removed(path, &[String::from(interface)]);
    store.interfaces_added(
        path,
        interfaces([(
            String::from(interface),
            properties("Connected", PropertyValue::Boolean(false)),
        )]),
    );

    assert!(!store.replace_interface_if_revision(
        &token,
        properties("Connected", PropertyValue::Boolean(true)),
    ));
    assert_eq!(
        store.objects()[path][interface]["Connected"],
        PropertyValue::Boolean(false)
    );
}

#[test]
fn unrelated_interface_revisions_are_independent() {
    let mut store = ObjectStore::default();
    let path = "/org/bluez/hci0/dev_AA";
    store.interfaces_added(
        path,
        interfaces([
            (
                String::from("org.bluez.Device1"),
                properties("Connected", PropertyValue::Boolean(true)),
            ),
            (
                String::from("org.bluez.Battery1"),
                properties("Percentage", PropertyValue::Integer(80)),
            ),
        ]),
    );
    let battery_token = store
        .begin_refresh(path, "org.bluez.Battery1")
        .expect("battery interface exists");

    assert!(store.properties_changed(
        path,
        "org.bluez.Device1",
        properties("Connected", PropertyValue::Boolean(false)),
        &[],
    ));
    assert!(store.replace_interface_if_revision(
        &battery_token,
        properties("Percentage", PropertyValue::Integer(75)),
    ));
    assert_eq!(
        store.objects()[path]["org.bluez.Battery1"]["Percentage"],
        PropertyValue::Integer(75)
    );
}
