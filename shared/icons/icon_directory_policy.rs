//! Icon directory selection for the shared SDK.
#![cfg_attr(not(test), no_std)]

use core::{ffi::c_void, slice};

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
