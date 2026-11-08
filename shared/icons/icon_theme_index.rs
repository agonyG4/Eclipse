//! Freedesktop icon index reader. Qt-specific INI encodings use the compatibility reader.
use super::{Directory, Text};
use std::{collections::HashMap, ffi::c_void, ptr, slice};

enum Error {
    Invalid,
    NativeEncoding,
}

fn list(value: &str) -> impl Iterator<Item = &str> {
    value
        .split(',')
        .map(str::trim)
        .filter(|part| !part.is_empty())
}

fn parse(input: &str) -> Result<(Vec<&str>, Vec<(&str, Directory)>), Error> {
    let mut groups = HashMap::<&str, HashMap<&str, &str>>::new();
    let mut group = "";
    for line in input.trim_start_matches('\u{feff}').lines().map(str::trim) {
        if line.is_empty() || line.starts_with(['#', ';']) {
            continue;
        }
        // QSettings supports quoting, escaping and typed values in addition to INI.
        if line.contains(['"', '\\', ';', '\0']) {
            return Err(Error::NativeEncoding);
        }
        if let Some(section) = line
            .strip_prefix('[')
            .and_then(|line| line.strip_suffix(']'))
        {
            if section.contains(['%', '[', ']']) {
                return Err(Error::NativeEncoding);
            }
            group = section.trim();
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            return Err(Error::NativeEncoding);
        };
        if key.contains(['%', '/']) || value.trim_start().starts_with('@') {
            return Err(Error::NativeEncoding);
        }
        groups
            .entry(group)
            .or_default()
            .insert(key.trim(), value.trim());
    }
    let theme = groups.get("Icon Theme").ok_or(Error::Invalid)?;
    let value = |key| theme.get(key).copied().unwrap_or_default();
    if value("Name").is_empty() || value("Comment").is_empty() {
        return Err(Error::Invalid);
    }
    let mut inherits = Vec::new();
    for theme in list(value("Inherits")) {
        if !inherits.contains(&theme) {
            inherits.push(theme);
        }
    }
    let mut directories = Vec::new();
    for path in list(value("Directories")).chain(list(value("ScaledDirectories"))) {
        if directories.iter().any(|(existing, _)| *existing == path) {
            continue;
        }
        if path.starts_with('/')
            || path.starts_with(":/")
            || path.split('/').any(|part| part == "..")
        {
            return Err(Error::Invalid);
        }
        let group = groups.get(path).ok_or(Error::Invalid)?;
        let number = |key, default| {
            group
                .get(key)
                .map_or(Ok(default), |value| value.parse::<i32>())
                .map_err(|_| Error::NativeEncoding)
        };
        let size = number("Size", 0)?;
        let scale = number("Scale", 1)?;
        let min_size = number("MinSize", size)?;
        let max_size = number("MaxSize", size)?;
        let threshold = number("Threshold", 2)?;
        let kind = group.get("Type").copied().unwrap_or_default();
        let kind = if kind.eq_ignore_ascii_case("fixed") {
            0
        } else if kind.eq_ignore_ascii_case("scalable") {
            1
        } else if kind.is_empty() || kind.eq_ignore_ascii_case("threshold") {
            2
        } else if !kind.is_ascii() {
            return Err(Error::NativeEncoding);
        } else {
            return Err(Error::Invalid);
        };
        if size <= 0 || scale <= 0 || min_size <= 0 || max_size < min_size || threshold < 0 {
            return Err(Error::Invalid);
        }
        directories.push((
            path,
            Directory {
                size,
                scale,
                min_size,
                max_size,
                threshold,
                kind,
            },
        ));
    }
    if directories.is_empty() {
        return Err(Error::Invalid);
    }
    Ok((inherits, directories))
}

type Entry = unsafe extern "C" fn(*mut c_void, Text, *const Directory);

/// # Safety
/// Input bytes and callback context must remain valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_parse_index(
    data: *const u8,
    len: usize,
    emit: Option<Entry>,
    context: *mut c_void,
) -> u8 {
    let Some(emit) = emit else {
        return 0;
    };
    if data.is_null() || len == 0 || len > isize::MAX as usize {
        return 0;
    }
    let Ok(input) = str::from_utf8(unsafe { slice::from_raw_parts(data, len) }) else {
        return 2;
    };
    let (inherits, directories) = match parse(input) {
        Ok(index) => index,
        Err(Error::Invalid) => return 0,
        Err(Error::NativeEncoding) => return 2,
    };
    for name in inherits {
        let name: Vec<_> = name.encode_utf16().collect();
        unsafe { emit(context, Text::view(&name), ptr::null()) };
    }
    for (name, directory) in directories {
        let name: Vec<_> = name.encode_utf16().collect();
        unsafe { emit(context, Text::view(&name), &directory) };
    }
    1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn standard_index_keeps_order_deduplicates_and_accepts_commas_in_comments() {
        let (inherits, directories) = parse(
            "\u{feff}[Icon Theme]\r\nName=Example\r\nComment=Icons, for testing\r\n\
             Inherits=Parent, hicolor, Parent\r\nDirectories=16/apps\r\n\
             ScaledDirectories=16/apps,32/apps\r\n[16/apps]\r\nSize=16\r\n\
             [32/apps]\r\nSize=16\r\nScale=2\r\nType=Fixed\r\n",
        )
        .ok()
        .unwrap();
        assert_eq!(inherits, ["Parent", "hicolor"]);
        assert_eq!(
            directories
                .iter()
                .map(|(name, _)| *name)
                .collect::<Vec<_>>(),
            ["16/apps", "32/apps"]
        );
        assert_eq!(directories[1].1.scale, 2);
    }

    #[test]
    fn malformed_indexes_do_not_publish_and_qt_encodings_use_compatibility() {
        assert!(matches!(
            parse("[Icon Theme]\nName=X\n"),
            Err(Error::Invalid)
        ));
        assert!(matches!(
            parse("[Icon Theme]\nName=\"X\"\n"),
            Err(Error::NativeEncoding)
        ));
        assert!(matches!(
            parse("[Icon Theme]\nName=X\nComment=X\nDirectories=../bad\n[../bad]\nSize=16"),
            Err(Error::Invalid)
        ));
    }
}
