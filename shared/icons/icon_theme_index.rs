//! Freedesktop icon index reader. Qt-specific INI encodings use the compatibility reader.
use super::{Directory, Text};
use std::{collections::HashMap, ffi::c_void, iter::Peekable, ptr, slice, str::Chars};

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

fn logical_lines(mut input: &str) -> impl Iterator<Item = &str> {
    std::iter::from_fn(move || {
        if input.is_empty() {
            return None;
        }
        let (mut escaped, mut quoted) = (false, false);
        let mut comment = input.trim_start_matches([' ', '\t']).starts_with('#');
        let mut chars = input.char_indices().peekable();
        while let Some((index, c)) = chars.next() {
            if matches!(c, '\r' | '\n') {
                let mut end = index + 1;
                if c == '\r' && chars.next_if(|(_, next)| *next == '\n').is_some() {
                    end += 1;
                }
                if escaped && !comment {
                    escaped = false;
                    continue;
                }
                let line = &input[..index];
                input = &input[end..];
                return Some(line);
            }
            if comment {
                continue;
            }
            if escaped {
                escaped = false;
                continue;
            }
            match c {
                '\\' => escaped = true,
                '"' => quoted = !quoted,
                ';' if !quoted => comment = true,
                _ => {}
            }
        }
        Some(std::mem::take(&mut input))
    })
}

fn numeric_escape(chars: &mut Peekable<Chars<'_>>, radix: u32, first: Option<u32>) -> Option<u16> {
    let mut result = first.map(|digit| digit as u16);
    while let Some(digit) = chars.peek().and_then(|c| c.to_digit(radix)) {
        chars.next();
        let value = result.unwrap_or(0);
        result = Some(value.wrapping_mul(radix as u16).wrapping_add(digit as u16));
    }
    result
}

fn escape(chars: &mut Peekable<Chars<'_>>) -> Result<Option<u16>, Error> {
    let Some(c) = chars.next() else {
        return Err(Error::NativeEncoding);
    };
    let decoded = match c {
        'a' => 7,
        'b' => 8,
        'f' => 12,
        'n' => 10,
        'r' => 13,
        't' => 9,
        'v' => 11,
        '"' | '\'' | '?' | '\\' => c as u16,
        'x' => return Ok(numeric_escape(chars, 16, None)),
        '0'..='7' => return Ok(numeric_escape(chars, 8, c.to_digit(8))),
        '\r' | '\n' => {
            if c == '\r' {
                chars.next_if_eq(&'\n');
            }
            return Ok(None);
        }
        // Unknown ASCII escapes drop the escaped character in QSettings.
        c if c.is_ascii() => return Ok(None),
        _ => return Err(Error::NativeEncoding),
    };
    Ok(Some(decoded))
}

fn values(input: &str) -> Result<Vec<String>, Error> {
    let mut items = Vec::new();
    let mut item = Vec::new();
    let mut quoted = false;
    let mut closed = false;
    let mut chars = input.chars().peekable();
    let push = |items: &mut Vec<String>, item: &mut Vec<u16>| {
        let decoded = String::from_utf16(item).map_err(|_| Error::NativeEncoding)?;
        let value = decoded.trim();
        if let Some(literal) = value.strip_prefix("@@") {
            items.push(format!("@{literal}"));
        } else if value.starts_with('@') {
            return Err(Error::NativeEncoding);
        } else {
            items.push(value.to_owned());
        }
        item.clear();
        Ok(())
    };
    while let Some(c) = chars.next() {
        match c {
            ';' if !quoted => break,
            ',' if !quoted => {
                push(&mut items, &mut item)?;
                closed = false;
            }
            '"' if quoted => {
                quoted = false;
                closed = true;
            }
            '"' if !closed
                && item.iter().all(|unit| {
                    char::from_u32(u32::from(*unit)).is_some_and(char::is_whitespace)
                }) =>
            {
                item.clear();
                quoted = true;
            }
            '"' => return Err(Error::NativeEncoding),
            c if closed && c.is_whitespace() => {}
            _ if closed => return Err(Error::NativeEncoding),
            '\\' => item.extend(escape(&mut chars)?),
            '\0' => return Err(Error::NativeEncoding),
            _ => item.extend_from_slice(c.encode_utf16(&mut [0; 2])),
        }
    }
    if quoted {
        return Err(Error::NativeEncoding);
    }
    push(&mut items, &mut item)?;
    Ok(items)
}

fn parse(input: &str) -> Result<(Vec<String>, Vec<(String, Directory)>), Error> {
    let mut groups = HashMap::<&str, HashMap<&str, Vec<String>>>::new();
    let mut group = "";
    for line in logical_lines(input.trim_start_matches('\u{feff}')).map(str::trim) {
        if line.is_empty() || line.starts_with(['#', ';']) {
            continue;
        }
        if let Some(section) = line
            .strip_prefix('[')
            .and_then(|line| line.strip_suffix(']'))
        {
            if section.contains(['%', '[', ']', '"', '\\', ';', '\0']) {
                return Err(Error::NativeEncoding);
            }
            group = section.trim();
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            return Err(Error::NativeEncoding);
        };
        if key.contains(['%', '/', '"', '\\', ';', '\0']) {
            return Err(Error::NativeEncoding);
        }
        groups
            .entry(group)
            .or_default()
            .insert(key.trim(), values(value)?);
    }
    let theme = groups.get("Icon Theme").ok_or(Error::Invalid)?;
    let value = |key| theme.get(key).map_or(&[][..], Vec::as_slice);
    let has_text = |key| {
        let parts = value(key);
        parts.len() > 1 || parts.first().is_some_and(|part| !part.is_empty())
    };
    if !has_text("Name") || !has_text("Comment") {
        return Err(Error::Invalid);
    }
    let mut inherits = Vec::new();
    for theme in value("Inherits").iter().flat_map(|value| list(value)) {
        if !inherits.iter().any(|existing| existing == theme) {
            inherits.push(theme.to_owned());
        }
    }
    let mut directories = Vec::new();
    for path in value("Directories")
        .iter()
        .chain(value("ScaledDirectories"))
        .flat_map(|value| list(value))
    {
        if directories.iter().any(|(existing, _)| *existing == path) {
            continue;
        }
        if path.starts_with('/')
            || path.starts_with(":/")
            || path.split(['/', '\\']).any(|part| part == "..")
        {
            return Err(Error::Invalid);
        }
        // QSettings normalizes group separators; keep its behavior for
        // directory names where a literal group lookup is ambiguous.
        if path.contains('\\') || path.split('/').any(|part| part.is_empty() || part == ".") {
            return Err(Error::NativeEncoding);
        }
        let group = groups.get(path).ok_or(Error::Invalid)?;
        let number = |key, default| {
            group
                .get(key)
                .map_or(Ok(default), |parts| match parts.as_slice() {
                    [value] => value.parse::<i32>().map_err(|_| Error::NativeEncoding),
                    _ => Err(Error::NativeEncoding),
                })
        };
        let size = number("Size", 0)?;
        let scale = number("Scale", 1)?;
        let min_size = number("MinSize", size)?;
        let max_size = number("MaxSize", size)?;
        let threshold = number("Threshold", 2)?;
        let kind = match group.get("Type").map(Vec::as_slice) {
            Some([value]) => value.as_str(),
            _ => "",
        };
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
            path.to_owned(),
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
    fn numeric_escapes_decode_utf16_and_consume_the_full_digit_run() {
        assert_eq!(
            values(r"\xD83D\xDE00,\1012z,\x12345z,\xZ,\08")
                .ok()
                .unwrap(),
            ["😀", "Ȋz", "⍅z", "Z", "\u{0}8"]
        );
    }

    #[test]
    fn continuation_joins_crlf_but_leaves_comment_and_even_backslash_lines_separate() {
        let source = "[Icon Theme]\r\nName=Example\r\nComment=My\\\r\n icons\r\n\
                      # ignore\\\r\nDirectories=16/\\\r\napps\r\n\
                      [16/apps]\r\nSize=16 ; note\\\r\nScale=2\r\n";
        let (_, directories) = parse(source).ok().expect("continued INI values");
        assert_eq!(directories[0].0, "16/apps");
        assert_eq!(directories[0].1.scale, 2);
        assert_eq!(
            logical_lines("key=a\\\\\nnext=b\n").collect::<Vec<_>>(),
            ["key=a\\\\", "next=b"]
        );
    }
    #[test]
    fn quoted_values_and_inline_comments_are_parsed_in_rust() {
        let parsed = parse(
            "[Icon Theme]\nName=\"Example\"\nComment=\"Icons, for testing\" ; note\n\
             Inherits=\"Parent\", \"hicolor\"\nDirectories=\"16/apps\", \"32/apps\"\n\
             [16/apps]\nSize=\"16\"\n[32/apps]\nSize=16 ; pixels\nScale=2\nType=\"Fixed\"\n",
        );
        let (inherits, directories) = parsed.ok().expect("standard quoted values");
        assert_eq!(inherits, ["Parent", "hicolor"]);
        assert_eq!(directories[0].0, "16/apps");
        assert_eq!(directories[1].1.scale, 2);
    }

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
                .map(|(name, _)| name.as_str())
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
            parse("[Icon Theme]\nName=@Variant(unsupported)\n"),
            Err(Error::NativeEncoding)
        ));
        assert!(matches!(
            parse("[Icon Theme]\nName=X\nComment=X\nDirectories=../bad\n[../bad]\nSize=16"),
            Err(Error::Invalid)
        ));
    }
}
