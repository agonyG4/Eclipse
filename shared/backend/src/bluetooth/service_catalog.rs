/// Stable semantic categories used only to describe a Bluetooth service.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum BluetoothServiceCategory {
    #[default]
    Unknown,
    Audio,
    MediaControl,
    Calls,
    HumanInterface,
    FileTransfer,
    Contacts,
    Network,
    Battery,
    Serial,
}

/// Classifies curated Bluetooth SIG service UUIDs into user-facing categories.
///
/// The assignments below are a deliberately small subset of the official
/// Bluetooth SIG Assigned Numbers list, reviewed on 2026-09-24:
/// <https://www.bluetooth.com/specifications/assigned-numbers/>.
/// This is not a complete copy of that database. Unknown values remain valid
/// protocol identities and must not affect Agent authorization.
pub fn classify_service_uuid(uuid: &str) -> BluetoothServiceCategory {
    let Some(value) = bluetooth_sig_alias(uuid) else {
        return BluetoothServiceCategory::Unknown;
    };

    match value {
        0x1101 => BluetoothServiceCategory::Serial,
        0x1105 | 0x1106 => BluetoothServiceCategory::FileTransfer,
        0x1108 | 0x1112 | 0x111e | 0x111f => BluetoothServiceCategory::Calls,
        0x110a | 0x110b | 0x110d | 0x1203 => BluetoothServiceCategory::Audio,
        0x110c | 0x110e => BluetoothServiceCategory::MediaControl,
        0x1115..=0x1117 => BluetoothServiceCategory::Network,
        0x1124 | 0x1812 => BluetoothServiceCategory::HumanInterface,
        0x112f => BluetoothServiceCategory::Contacts,
        0x180f => BluetoothServiceCategory::Battery,
        0x184b..=0x184d | 0x1855 => BluetoothServiceCategory::Calls,
        0x1843..=0x1845 | 0x184e..=0x184f | 0x1850..=0x1856 | 0x1858 => {
            BluetoothServiceCategory::Audio
        }
        0x1848..=0x1849 => BluetoothServiceCategory::MediaControl,
        _ => BluetoothServiceCategory::Unknown,
    }
}

fn bluetooth_sig_alias(uuid: &str) -> Option<u32> {
    if !uuid.is_ascii() {
        return None;
    }

    let bytes = uuid.as_bytes();
    match bytes.len() {
        4 => parse_hex(bytes),
        8 => parse_hex(bytes),
        36 if bytes[8] == b'-'
            && bytes[9..13] == *b"0000"
            && bytes[13] == b'-'
            && bytes[14..18] == *b"1000"
            && bytes[18] == b'-'
            && bytes[19..23] == *b"8000"
            && bytes[23] == b'-'
            && bytes[24..].eq_ignore_ascii_case(b"00805f9b34fb") =>
        {
            parse_hex(&bytes[..8])
        }
        _ => None,
    }
}

fn parse_hex(bytes: &[u8]) -> Option<u32> {
    bytes.iter().try_fold(0_u32, |value, byte| {
        let digit = match byte {
            b'0'..=b'9' => u32::from(byte - b'0'),
            b'a'..=b'f' => u32::from(byte - b'a' + 10),
            b'A'..=b'F' => u32::from(byte - b'A' + 10),
            _ => return None,
        };
        value.checked_mul(16)?.checked_add(digit)
    })
}

#[cfg(test)]
mod tests {
    use super::{BluetoothServiceCategory, classify_service_uuid};

    #[test]
    fn bluetooth_base_uuid_aliases_classify_identically() {
        let aliases = [
            "110e",
            "0000110e",
            "0000110e-0000-1000-8000-00805f9b34fb",
            "0000110E-0000-1000-8000-00805F9B34FB",
        ];

        for alias in aliases {
            assert_eq!(
                classify_service_uuid(alias),
                BluetoothServiceCategory::MediaControl,
                "alias {alias}"
            );
        }
    }

    #[test]
    fn custom_uuid_containing_a_sig_alias_remains_unknown() {
        assert_eq!(
            classify_service_uuid("f000110e-0451-4000-b000-000000000000"),
            BluetoothServiceCategory::Unknown
        );
    }

    #[test]
    fn malformed_and_unsupported_values_fail_safely() {
        for value in [
            "",
            "garbage",
            "110",
            "0000110e-0000-1000-8000-00805f9b34f",
            "000g110e",
            "0000110e-0000-0000-8000-00805f9b34fb",
            "1000010e",
        ] {
            assert_eq!(
                classify_service_uuid(value),
                BluetoothServiceCategory::Unknown,
                "value {value:?}"
            );
        }
    }

    #[test]
    fn representative_assigned_numbers_map_to_stable_categories() {
        let cases = [
            ("110b", BluetoothServiceCategory::Audio),
            ("1848", BluetoothServiceCategory::MediaControl),
            ("111e", BluetoothServiceCategory::Calls),
            ("1812", BluetoothServiceCategory::HumanInterface),
            ("1106", BluetoothServiceCategory::FileTransfer),
            ("112f", BluetoothServiceCategory::Contacts),
            ("1116", BluetoothServiceCategory::Network),
            ("180f", BluetoothServiceCategory::Battery),
            ("1101", BluetoothServiceCategory::Serial),
            ("1234", BluetoothServiceCategory::Unknown),
        ];

        for (uuid, expected) in cases {
            assert_eq!(classify_service_uuid(uuid), expected, "uuid {uuid}");
        }
    }

    #[test]
    fn modern_audio_media_and_call_services_use_stable_categories() {
        let cases = [
            ("1843", BluetoothServiceCategory::Audio),
            ("1844", BluetoothServiceCategory::Audio),
            ("1845", BluetoothServiceCategory::Audio),
            ("1848", BluetoothServiceCategory::MediaControl),
            ("1849", BluetoothServiceCategory::MediaControl),
            ("184b", BluetoothServiceCategory::Calls),
            ("184c", BluetoothServiceCategory::Calls),
            ("184d", BluetoothServiceCategory::Calls),
            ("184e", BluetoothServiceCategory::Audio),
            ("184f", BluetoothServiceCategory::Audio),
            ("1850", BluetoothServiceCategory::Audio),
            ("1851", BluetoothServiceCategory::Audio),
            ("1852", BluetoothServiceCategory::Audio),
            ("1853", BluetoothServiceCategory::Audio),
            ("1854", BluetoothServiceCategory::Audio),
            ("1855", BluetoothServiceCategory::Calls),
            ("1856", BluetoothServiceCategory::Audio),
            ("1858", BluetoothServiceCategory::Audio),
        ];

        for (uuid, expected) in cases {
            assert_eq!(classify_service_uuid(uuid), expected, "uuid {uuid}");
        }
    }

    #[test]
    fn canonical_uuid_hexadecimal_case_is_ignored() {
        assert_eq!(
            classify_service_uuid("0000110B-0000-1000-8000-00805F9B34FB"),
            BluetoothServiceCategory::Audio
        );
        assert_eq!(
            classify_service_uuid("0000110b-0000-1000-8000-00805f9b34fb"),
            BluetoothServiceCategory::Audio
        );
    }
}
