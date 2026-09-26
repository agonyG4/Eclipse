//! Pure presentation semantics for BlueZ device identity evidence.

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[repr(i32)]
pub enum BluetoothDeviceKind {
    #[default]
    Unknown = 0,
    Audio = 1,
    Headset = 2,
    Headphones = 3,
    Speaker = 4,
    Keyboard = 5,
    Mouse = 6,
    Gamepad = 7,
    Phone = 8,
    Computer = 9,
    Tablet = 10,
    Printer = 11,
    Camera = 12,
    Network = 13,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum BluetoothDeviceNameSource {
    ExplicitAlias,
    RemoteName,
    NormalizedRemoteName,
    #[default]
    None,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RawBluetoothDisplayIdentity<'a> {
    pub alias: &'a str,
    pub remote_name: &'a str,
    pub address: &'a str,
    pub object_path: &'a str,
    pub icon: &'a str,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BluetoothDisplayIdentity {
    pub display_name: String,
    pub kind: BluetoothDeviceKind,
    pub name_source: BluetoothDeviceNameSource,
}

/// Resolve BlueZ's raw identity evidence into a concrete display name and kind.
///
/// A changed, non-empty Alias is treated as a user alias and is preserved even
/// when it resembles a transport token. Other candidates are normalized only
/// when they carry strong technical-name evidence.
pub fn resolve_bluetooth_display_identity(
    raw: RawBluetoothDisplayIdentity<'_>,
) -> BluetoothDisplayIdentity {
    let alias = sanitize_one_line(raw.alias);
    let remote_name = sanitize_one_line(raw.remote_name);
    let explicit_alias =
        !alias.is_empty() && (remote_name.is_empty() || raw.alias != raw.remote_name);

    let (display_name, name_source) = if explicit_alias {
        (alias, BluetoothDeviceNameSource::ExplicitAlias)
    } else if !remote_name.is_empty() {
        let (name, normalized) = resolve_remote_name(&remote_name, raw.address, raw.object_path);
        let source = if name.is_empty() {
            BluetoothDeviceNameSource::None
        } else if normalized {
            BluetoothDeviceNameSource::NormalizedRemoteName
        } else {
            BluetoothDeviceNameSource::RemoteName
        };
        (name, source)
    } else {
        (String::new(), BluetoothDeviceNameSource::None)
    };

    BluetoothDisplayIdentity {
        display_name,
        kind: bluetooth_device_kind_from_icon(raw.icon),
        name_source,
    }
}

fn resolve_remote_name(candidate: &str, address: &str, object_path: &str) -> (String, bool) {
    if let Some(suffix) = strip_transport_prefix(candidate) {
        let suffix = sanitize_one_line(suffix);
        if is_usable_name(&suffix, address, object_path) {
            return (suffix, true);
        }
        return (String::new(), false);
    }

    if is_usable_name(candidate, address, object_path) {
        (candidate.to_owned(), false)
    } else {
        (String::new(), false)
    }
}

fn strip_transport_prefix(value: &str) -> Option<&str> {
    ["BLE_", "LE_"].iter().find_map(|prefix| {
        value
            .get(..prefix.len())
            .filter(|candidate| candidate.eq_ignore_ascii_case(prefix))
            .and_then(|_| value.get(prefix.len()..))
            .filter(|suffix| !suffix.is_empty())
    })
}

fn is_usable_name(candidate: &str, address: &str, object_path: &str) -> bool {
    if candidate.is_empty()
        || is_generic_label(candidate)
        || is_current_identity(candidate, address, object_path)
    {
        return false;
    }

    if is_hardware_address(candidate) || is_bluez_device_token(candidate) || is_hex_token(candidate)
    {
        return false;
    }

    true
}

fn is_generic_label(value: &str) -> bool {
    [
        "unknown",
        "unknown device",
        "bluetooth device",
        "le device",
        "ble device",
    ]
    .iter()
    .any(|label| value.eq_ignore_ascii_case(label))
}

fn is_hardware_address(value: &str) -> bool {
    if value.len() == 12 && value.bytes().all(|byte| byte.is_ascii_hexdigit()) {
        return true;
    }

    [':', '-'].iter().any(|separator| {
        let mut groups = value.split(*separator);
        let valid = groups
            .by_ref()
            .all(|group| group.len() == 2 && group.bytes().all(|byte| byte.is_ascii_hexdigit()));
        valid && groups.count() == 0 && value.split(*separator).count() == 6
    })
}

fn is_bluez_device_token(value: &str) -> bool {
    let Some(address) = value
        .get(..4)
        .filter(|prefix| prefix.eq_ignore_ascii_case("dev_"))
        .and_then(|_| value.get(4..))
    else {
        return false;
    };

    let mut groups = address.split('_');
    groups.all(|group| group.len() == 2 && group.bytes().all(|byte| byte.is_ascii_hexdigit()))
        && groups.count() == 0
        && address.split('_').count() == 6
}

fn is_hex_token(value: &str) -> bool {
    (8..=40).contains(&value.len()) && value.bytes().all(|byte| byte.is_ascii_hexdigit())
}

fn is_current_identity(candidate: &str, address: &str, object_path: &str) -> bool {
    let path_leaf = object_path.rsplit('/').next().unwrap_or_default();
    [address, object_path, path_leaf]
        .iter()
        .filter(|identity| !identity.is_empty())
        .any(|identity| {
            candidate.eq_ignore_ascii_case(identity)
                || normalized_token(candidate) == normalized_token(identity)
        })
}

fn normalized_token(value: &str) -> String {
    value
        .chars()
        .filter(char::is_ascii_alphanumeric)
        .map(|character| character.to_ascii_lowercase())
        .collect()
}

fn sanitize_one_line(value: &str) -> String {
    let mut sanitized = String::with_capacity(value.len());
    let mut pending_separator = false;
    let mut consume_whitespace_after_control = false;

    for character in value.trim().chars() {
        if character.is_control() || matches!(character, '\u{2028}' | '\u{2029}') {
            pending_separator = true;
            consume_whitespace_after_control = true;
            continue;
        }

        if pending_separator {
            if !character.is_whitespace()
                && !sanitized.is_empty()
                && !sanitized.chars().last().is_some_and(char::is_whitespace)
            {
                sanitized.push(' ');
            }
            pending_separator = false;
        }

        if consume_whitespace_after_control && character.is_whitespace() {
            if sanitized.is_empty() || sanitized.chars().last().is_some_and(char::is_whitespace) {
                continue;
            }
        } else {
            consume_whitespace_after_control = false;
        }
        sanitized.push(character);
    }

    sanitized.trim().to_owned()
}

pub fn bluetooth_device_kind_from_icon(icon: &str) -> BluetoothDeviceKind {
    match icon.trim().to_ascii_lowercase().as_str() {
        "audio-headset" => BluetoothDeviceKind::Headset,
        "audio-headphones" => BluetoothDeviceKind::Headphones,
        "audio-speakers" => BluetoothDeviceKind::Speaker,
        "audio-card" | "multimedia-player" => BluetoothDeviceKind::Audio,
        "input-keyboard" => BluetoothDeviceKind::Keyboard,
        "input-mouse" => BluetoothDeviceKind::Mouse,
        "input-gaming" => BluetoothDeviceKind::Gamepad,
        "input-tablet" => BluetoothDeviceKind::Tablet,
        "phone" => BluetoothDeviceKind::Phone,
        "computer" => BluetoothDeviceKind::Computer,
        "printer" => BluetoothDeviceKind::Printer,
        "camera-photo" | "camera-video" => BluetoothDeviceKind::Camera,
        "network-wireless" | "modem" => BluetoothDeviceKind::Network,
        _ => BluetoothDeviceKind::Unknown,
    }
}
