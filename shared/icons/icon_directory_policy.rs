//! Icon directory selection for the shared SDK.
use core::{ffi::c_void, slice};
use std::collections::HashSet;

mod icon_theme_index;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Text {
    data: *const u16,
    size: usize,
}

impl Text {
    fn view(units: &[u16]) -> Self {
        Self {
            data: units.as_ptr(),
            size: units.len(),
        }
    }

    unsafe fn units(&self) -> &[u16] {
        if self.data.is_null() || self.size == 0 || self.size > isize::MAX as usize / 2 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(self.data, self.size) }
        }
    }
}

fn valid_theme(units: &[u16]) -> bool {
    let ascii = |unit: u16| u8::try_from(unit).unwrap_or(0);
    (1..=128).contains(&units.len())
        && ascii(units[0]).is_ascii_alphanumeric()
        && units
            .iter()
            .copied()
            .map(ascii)
            .all(|byte| byte.is_ascii_alphanumeric() || b"._+-".contains(&byte))
        && !units.windows(2).any(|part| part == [46, 46])
}

/// # Safety
/// Name must refer to readable UTF-16 for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_valid_theme(name: Text) -> bool {
    let name = unsafe { name.units() };
    // The previous anchored Qt expression accepted one final LF via '$'.
    valid_theme(name.strip_suffix(&[u16::from(b'\n')]).unwrap_or(name))
}

/// # Safety
/// Path must refer to readable UTF-16 for this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_safe_relative_path(path: Text, absolute: bool) -> bool {
    let path = unsafe { path.units() };
    !absolute
        && !path.is_empty()
        && !path
            .split(|unit| *unit == u16::from(b'/') || *unit == u16::from(b'\\'))
            .any(|component| component == [46, 46])
}

fn walk(
    roots: impl IntoIterator<Item = (Vec<u16>, bool)>,
    defer_children: bool,
    mut visit: impl FnMut(&[u16], &mut Vec<Vec<u16>>),
) {
    let mut pending: Vec<_> = roots.into_iter().collect();
    pending.reverse();
    let mut seen = HashSet::new();
    while let Some((name, defer)) = pending.pop() {
        if !valid_theme(&name)
            || (defer && name.iter().copied().eq("hicolor".encode_utf16()))
            || !seen.insert(name.clone())
        {
            continue;
        }
        let mut children = Vec::new();
        visit(&name, &mut children);
        pending.extend(
            children
                .into_iter()
                .rev()
                .map(|child| (child, defer_children)),
        );
    }
}

type Visit =
    unsafe extern "C" fn(*mut c_void, Text, unsafe extern "C" fn(*mut c_void, Text), *mut c_void);

unsafe extern "C" fn child(context: *mut c_void, name: Text) {
    let children = unsafe { &mut *context.cast::<Vec<Vec<u16>>>() };
    children.push(unsafe { name.units() }.to_vec());
}

fn call_visit(visit: Visit, context: *mut c_void, name: &[u16], children: &mut Vec<Vec<u16>>) {
    // The host calls the child collector only during this synchronous visit.
    unsafe {
        visit(
            context,
            Text::view(name),
            child,
            (children as *mut Vec<Vec<u16>>).cast(),
        )
    }
}

/// # Safety
/// Root descriptors and the visitor must be valid throughout the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_walk_themes(
    roots: *const Text,
    count: usize,
    defer_children: bool,
    visit: Option<Visit>,
    context: *mut c_void,
) {
    let Some(visit) = visit else {
        return;
    };
    if roots.is_null() || count == 0 || count > isize::MAX as usize / size_of::<Text>() {
        return;
    }
    let roots = unsafe { slice::from_raw_parts(roots, count) };
    walk(
        roots
            .iter()
            .map(|root| (unsafe { root.units() }.to_vec(), false)),
        defer_children,
        |name, children| call_visit(visit, context, name, children),
    );
}

/// # Safety
/// Both strings must be readable; the visitor must be valid throughout the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_theme_family(
    theme: Text,
    fallback: Text,
    visit: Option<Visit>,
    context: *mut c_void,
) {
    let Some(visit) = visit else {
        return;
    };
    walk(
        [
            (unsafe { theme.units() }.to_vec(), false),
            (unsafe { fallback.units() }.to_vec(), true),
            ("hicolor".encode_utf16().collect(), false),
        ],
        true,
        |name, children| call_visit(visit, context, name, children),
    );
}

/// # Safety
/// The type descriptor must refer to readable UTF-16 for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_directory_type(
    kind: Text,
    valid_size: bool,
    size: i32,
    scale: i32,
    min_size: i32,
    max_size: i32,
    threshold: i32,
) -> u8 {
    if !valid_size
        || size <= 0
        || scale <= 0
        || min_size <= 0
        || max_size < min_size
        || threshold < 0
    {
        return u8::MAX;
    }
    let kind = unsafe { kind.units() };
    // Qt supplies the case-folded value to preserve native INI comparisons.
    let equals = |text: &str| kind.iter().copied().eq(text.encode_utf16());
    if equals("fixed") {
        0
    } else if equals("scalable") {
        1
    } else if kind.is_empty() || equals("threshold") {
        2
    } else {
        u8::MAX
    }
}

fn choose_variant(name: &[u8], light: bool, mut lookup: impl FnMut(u8) -> bool) -> bool {
    let explicit = name.ends_with(b"-dark") || name.ends_with(b"-light");
    (!explicit && lookup(if light { 2 } else { 3 })) || lookup(1)
}

fn choose_theme(platform_is_hicolor: bool, mut lookup: impl FnMut(u8) -> bool) -> u8 {
    if lookup(1) {
        return 1;
    }
    if lookup(2) {
        return 2;
    }
    let compatibility = lookup(3);
    if platform_is_hicolor && compatibility {
        return 3;
    }
    if lookup(4) {
        return 4;
    }
    if compatibility { 3 } else { 0 }
}

type ThemeLookup = unsafe extern "C" fn(*mut c_void, u8) -> bool;

/// # Safety
/// Name and callback context must remain readable for the synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_choose_variant(
    name: *const u8,
    length: usize,
    light: bool,
    lookup: Option<ThemeLookup>,
    context: *mut c_void,
) -> bool {
    let Some(lookup) = lookup else {
        return false;
    };
    if name.is_null() || length == 0 || length > isize::MAX as usize {
        return false;
    }
    let name = unsafe { slice::from_raw_parts(name, length) };
    choose_variant(name, light, |variant| unsafe { lookup(context, variant) })
}

/// # Safety
/// Callback and context must remain valid for the synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_choose_theme(
    platform_is_hicolor: bool,
    lookup: Option<ThemeLookup>,
    context: *mut c_void,
) -> u8 {
    let Some(lookup) = lookup else {
        return 0;
    };
    choose_theme(platform_is_hicolor, |source| unsafe {
        lookup(context, source)
    })
}

#[repr(C)]
pub struct Directory {
    size: i32,
    scale: i32,
    min_size: i32,
    max_size: i32,
    threshold: i32,
    kind: u8,
}

impl Directory {
    fn bounds(&self) -> (i64, i64) {
        let size = i64::from(self.size);
        match self.kind {
            1 => (i64::from(self.min_size), i64::from(self.max_size)),
            2 => (
                size - i64::from(self.threshold),
                size + i64::from(self.threshold),
            ),
            _ => (size, size),
        }
    }

    fn exact(&self, size: i32, scale: i32) -> bool {
        let (lower, upper) = self.bounds();
        self.scale == scale && (lower..=upper).contains(&i64::from(size))
    }

    fn rank(&self, requested: i64) -> (bool, i64) {
        let (mut lower, upper) = self.bounds();
        if self.kind == 2 {
            lower = lower.max(1);
        }
        let scale = i64::from(self.scale);
        let lower = lower * scale;
        let upper = upper * scale;
        let distance = if requested < lower {
            lower.saturating_sub(requested)
        } else {
            requested.saturating_sub(upper).max(0)
        };
        let undersized = distance > 0
            && requested > i64::from(self.size) * scale
            && (self.kind != 1 || requested > upper);
        (undersized, distance)
    }
}

fn choose(
    directories: &[Directory],
    size: i32,
    scale: i32,
    mut lookup: impl FnMut(usize) -> bool,
) -> bool {
    for (index, directory) in directories.iter().enumerate() {
        if directory.exact(size, scale) && lookup(index) {
            return true;
        }
    }
    let requested = i64::from(size) * i64::from(scale);
    let mut best = None;
    for (index, directory) in directories.iter().enumerate() {
        let rank = directory.rank(requested);
        // Keep the declaration order on ties and only publish available assets.
        if best.is_none_or(|best| rank < best) && lookup(index) {
            best = Some(rank);
        }
    }
    best.is_some()
}

/// # Safety
/// Directories must hold count readable descriptors. The callback and its
/// context must be valid for the call and may not mutate the descriptors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_icon_choose_directory(
    directories: *const Directory,
    count: usize,
    size: i32,
    scale: i32,
    lookup: Option<unsafe extern "C" fn(*mut c_void, usize) -> bool>,
    context: *mut c_void,
) -> bool {
    let Some(lookup) = lookup else {
        return false;
    };
    if directories.is_null() || count == 0 || count > isize::MAX as usize / size_of::<Directory>() {
        return false;
    }
    let directories = unsafe { slice::from_raw_parts(directories, count) };
    choose(directories, size, scale, |index| unsafe {
        lookup(context, index)
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn theme_walk_is_depth_first_and_defers_hicolor_across_cycles() {
        let key = |name: &str| name.encode_utf16().collect::<Vec<_>>();
        let mut visited = Vec::new();
        walk(
            [
                (key("Theme"), false),
                (key("Fallback"), true),
                (key("hicolor"), false),
            ],
            true,
            |name, children| {
                let name = String::from_utf16(name).unwrap();
                match name.as_str() {
                    "Theme" => children.extend([key("hicolor"), key("Parent"), key("../bad")]),
                    "Parent" => children.extend([key("Theme"), key("Grandparent")]),
                    "Fallback" => children.push(key("Parent")),
                    _ => {}
                }
                visited.push(name);
            },
        );
        assert_eq!(
            visited,
            ["Theme", "Parent", "Grandparent", "Fallback", "hicolor"]
        );
    }

    #[test]
    fn theme_overrides_precede_platform_and_hicolor_uses_compatibility() {
        assert_eq!(choose_theme(true, |_| true), 1);
        assert_eq!(choose_theme(true, |source| source != 1), 2);
        assert_eq!(choose_theme(true, |source| source >= 3), 3);
        assert_eq!(choose_theme(false, |source| source >= 3), 4);
        assert_eq!(choose_theme(false, |source| source == 3), 3);
        assert_eq!(choose_theme(false, |_| false), 0);
    }

    #[test]
    fn explicit_variants_only_probe_the_requested_name() {
        let mut probes = Vec::new();
        assert!(!choose_variant(b"Theme-dark", true, |variant| {
            probes.push(variant);
            false
        }));
        assert_eq!(probes, [1]);
        probes.clear();
        assert!(choose_variant(b"Theme", true, |variant| {
            probes.push(variant);
            variant == 1
        }));
        assert_eq!(probes, [2, 1]);
    }

    fn fixed(size: i32, scale: i32) -> Directory {
        Directory {
            size,
            scale,
            min_size: size,
            max_size: size,
            threshold: 0,
            kind: 0,
        }
    }

    fn selected(dirs: &[Directory], size: i32, scale: i32, available: &[bool]) -> Option<usize> {
        let mut selected = None;
        choose(dirs, size, scale, |index| {
            if available[index] {
                selected = Some(index);
            }
            available[index]
        });
        selected
    }

    #[test]
    fn exact_scale_wins_before_fallback_and_missing_assets_are_skipped() {
        let dirs = [fixed(48, 1), fixed(24, 2), fixed(24, 2)];
        assert_eq!(selected(&dirs, 24, 2, &[true, false, true]), Some(2));
        assert_eq!(selected(&dirs, 24, 2, &[true, false, false]), Some(0));
    }

    #[test]
    fn fallback_prefers_downscaling_and_keeps_declaration_order_on_ties() {
        let dirs = [fixed(32, 1), fixed(64, 1), fixed(32, 2)];
        assert_eq!(selected(&dirs, 40, 1, &[true; 3]), Some(1));
    }

    #[test]
    fn scalable_and_threshold_bounds_match_at_the_requested_scale() {
        let scalable = Directory {
            kind: 1,
            min_size: 24,
            max_size: 64,
            ..fixed(48, 2)
        };
        let threshold = Directory {
            kind: 2,
            threshold: 4,
            ..fixed(24, 1)
        };
        assert!(scalable.exact(64, 2));
        assert!(!scalable.exact(64, 1));
        assert!(threshold.exact(20, 1));
        assert!(!threshold.exact(19, 1));
    }
}
