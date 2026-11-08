//! macOS Tahoe app tiles for icons an Astrea icon theme has no artwork for.
//!
//! Everything is decided on a 512 px master: callers scale it down to the exact
//! size they present, so no size is ever upscaled. The classification and the
//! brand-tile analysis started from MacOS-Like's tile pass (GPL-3.0-or-later).
//!
//! Three source-art classes:
//!   1. full-bleed square art -> becomes the squircle itself; on a coloured
//!      tile of its own, that tile becomes a Tahoe brand tile
//!   2. solid disc/blob badge -> its own background is bled out to the squircle;
//!      a round badge with a one-colour rim instead sits on the theme's tile at
//!      80%, as macOS sets Spotify's
//!   3. shaped / transparent  -> centred on the theme's tile

mod brand;
mod fit;
mod look;
mod neutral;
mod raster;
mod style;

use look::{C, GLYPH_BOX, GLYPH_SHADOW_DARK, GLYPH_SHADOW_LIGHT, MARGIN, TILE, Variant, glyph_shadow, layers};
use raster::{Img, Plane, alpha_composite, blur_rgb, composited, resized};

/// Bumped whenever the look or the classification changes, so disk caches
/// made by an older build are not reused.
pub const ALGORITHM_VERSION: u32 = 3;
/// Bumped whenever a style's look changes.
pub const STYLE_VERSION: u32 = 2;
pub const MASTER_EXTENT: usize = C;

struct ShapeStats {
    coverage: f64,
    disc: f64,
    ring: f64,
}

/// Coverage of the alpha bbox, of the inscribed disc, and of its outer ring.
fn shape_stats(g: &Img) -> ShapeStats {
    let (w, h) = (g.w, g.h);
    let (cx, cy) = (w as f64 / 2.0, h as f64 / 2.0);
    let m = w.min(h) as f64;
    let (r_in, r_out) = ((m * 0.40).powi(2), (m * 0.46).powi(2));
    let (mut cov, mut disc, mut disc_op, mut ring, mut ring_op) = (0usize, 0usize, 0usize, 0usize, 0usize);
    for y in 0..h {
        for x in 0..w {
            let a = g.alpha(x, y);
            cov += (a >= 9) as usize;
            let d2 = (x as f64 - cx).powi(2) + (y as f64 - cy).powi(2);
            if d2 <= r_out {
                disc += 1;
                let op = (a > 8) as usize;
                disc_op += op;
                if d2 >= r_in {
                    ring += 1;
                    ring_op += op;
                }
            }
        }
    }
    ShapeStats {
        coverage: cov as f64 / (w * h) as f64,
        disc: disc_op as f64 / disc.max(1) as f64,
        ring: ring_op as f64 / ring.max(1) as f64,
    }
}

/// Scale to fill a box x box square, preserving aspect, centre-cropped.
fn cover_resize(g: &Img, b: usize) -> Img {
    let s = (b as f64 / g.w as f64).max(b as f64 / g.h as f64);
    let w = b.max((g.w as f64 * s).round() as usize);
    let h = b.max((g.h as f64 * s).round() as usize);
    let r = resized(g, w, h);
    let (ox, oy) = ((r.w - b) / 2, (r.h - b) / 2);
    r.cropped(ox as isize, oy as isize, (ox + b) as isize, (oy + b) as isize)
}

fn overscan(g: &Img, k: f64) -> Img {
    let b = (TILE as f64 * k).round() as usize;
    let r = cover_resize(g, b);
    let o = ((b - TILE) / 2) as isize;
    r.cropped(o, o, o + TILE as isize, o + TILE as isize)
}

/// Fraction of the squircle the artwork leaves transparent.
fn uncovered(layer: &Img) -> f64 {
    let mt = &layers().mask_tile;
    let mut miss = 0u64;
    for y in 0..TILE {
        for x in 0..TILE {
            let a = if layer.alpha(x, y) > 200 { 255 } else { 0 };
            miss += mt.at(x, y).saturating_sub(a) as u64;
        }
    }
    miss as f64 / (255.0 * (TILE * TILE) as f64)
}

/// Art that is already a rounded tile: scale it up until its silhouette covers
/// the squircle. None when no sane overscan closes the corners.
fn fit_to_squircle(g: &Img) -> Option<Img> {
    let first = overscan(g, 1.0);
    if uncovered(&first) <= 0.002 {
        return Some(first);
    }
    let (mut lo, mut hi) = (1.0, 1.30);
    if uncovered(&overscan(g, hi)) > 0.002 {
        return None;
    }
    for _ in 0..6 {
        let mid = (lo + hi) / 2.0;
        if uncovered(&overscan(g, mid)) <= 0.002 {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    Some(overscan(g, hi))
}

/// A round badge's diameter on the theme tile: macOS sets a round logo such as
/// Spotify's on its tile at about 80% of the tile.
const DISC: usize = 360;

/// Below this alpha-bbox coverage, square art that passes the disc test is a
/// circle (pi/4 = 0.785), not a rounded square: corners that round need a
/// radius over ~40% of the side.
const ROUND_BADGE: f64 = 0.85;

/// Extend the badge's own background out to the squircle edge: every pixel
/// beyond the outline takes the colour the badge has at its outline along the
/// same angle. `src` is TILE x TILE, the badge placed where it will sit.
fn bleed_backdrop(src: &Img) -> Img {
    const NA: usize = 720;
    let c = TILE as f64 / 2.0;
    let pi = std::f64::consts::PI;
    let bucket = |th: f64| ((th + pi) / (2.0 * pi) * NA as f64) as usize % NA;
    let mut edge = vec![0.0f64; NA];
    for y in 0..TILE {
        let dy = y as f64 - c + 0.5;
        for x in 0..TILE {
            if src.alpha(x, y) > 200 {
                let dx = x as f64 - c + 0.5;
                let a = bucket(dy.atan2(dx));
                edge[a] = edge[a].max(dx.hypot(dy));
            }
        }
    }
    const K: isize = 12;
    let sm: Vec<f64> = (0..NA as isize)
        .map(|a| (-K..=K).map(|i| edge[(a + i).rem_euclid(NA as isize) as usize]).sum::<f64>() / (2 * K + 1) as f64)
        .collect();
    let mut back = Img::new(TILE, TILE);
    for y in 0..TILE {
        let dy = y as f64 - c + 0.5;
        for x in 0..TILE {
            let dx = x as f64 - c + 0.5;
            let d = dx.hypot(dy);
            let th = dy.atan2(dx);
            let r = d.min((sm[bucket(th)] - 4.0).max(1.0));
            let sx = ((c + th.cos() * r) as isize).clamp(0, TILE as isize - 1) as usize;
            let sy = ((c + th.sin() * r) as isize).clamp(0, TILE as isize - 1) as usize;
            let s = src.idx(sx, sy);
            let o = back.idx(x, y);
            back.p[o..o + 3].copy_from_slice(&src.p[s..s + 3]);
            back.p[o + 3] = 255;
        }
    }
    blur_rgb(&mut back, 3.0);
    back
}

/// Spread of the colour just inside a round badge's rim, above which the rim
/// is a picture (a textured or many-coloured disc) that only reads blown up to
/// cover the tile.
const RIM_SPREAD: f64 = 14.0;

/// A round badge at DISC size, clipped to a clean circle (no soft shadow or
/// fringe under it), to set on the theme's tile. None when its rim is not one
/// colour.
fn round_badge(glyph: &Img) -> Option<Img> {
    let mut disc = cover_resize(&glyph.alpha_crop_above(200), DISC);
    let r = DISC as f64 / 2.0;
    let (mut sum, mut sq, mut n) = ([0.0f64; 3], [0.0f64; 3], 0usize);
    for y in 0..DISC {
        let dy = y as f64 + 0.5 - r;
        for x in 0..DISC {
            let d = (x as f64 + 0.5 - r).hypot(dy);
            if (r - 10.0..r - 4.0).contains(&d) && disc.alpha(x, y) > 200 {
                let i = disc.idx(x, y);
                for c in 0..3 {
                    let v = disc.p[i + c] as f64;
                    sum[c] += v;
                    sq[c] += v * v;
                }
                n += 1;
            }
        }
    }
    if n < 100 {
        return None;
    }
    let spread = ((0..3).map(|c| sq[c] / n as f64 - (sum[c] / n as f64).powi(2)).sum::<f64>() / 3.0)
        .max(0.0)
        .sqrt();
    if spread > RIM_SPREAD {
        return None;
    }
    let edge = r - 1.0;
    for y in 0..DISC {
        let dy = y as f64 + 0.5 - r;
        for x in 0..DISC {
            let cover = (edge - (x as f64 + 0.5 - r).hypot(dy) + 0.5).clamp(0.0, 1.0);
            let i = disc.idx(x, y) + 3;
            disc.p[i] = (disc.p[i] as f64 * cover).round() as u8;
        }
    }
    Some(disc)
}

/// `g` placed at (ox, oy) on the theme tiles, over its own soft shadow.
fn on_theme_tiles(g: &Img, ox: isize, oy: isize) -> [Img; 2] {
    let mut cover = Plane::new(C, C);
    for y in 0..g.h {
        for x in 0..g.w {
            let (cx, cy) = (x as isize + ox, y as isize + oy);
            if (0..C as isize).contains(&cx) && (0..C as isize).contains(&cy) {
                cover.p[cy as usize * C + cx as usize] = g.alpha(x, y);
            }
        }
    }
    let l = layers();
    look::VARIANTS.map(|v| {
        let mut icon = l.base[v as usize].clone();
        let strength = if v == Variant::Dark { GLYPH_SHADOW_DARK } else { GLYPH_SHADOW_LIGHT };
        alpha_composite(&mut icon, &glyph_shadow(&cover, strength), 0, 0);
        alpha_composite(&mut icon, g, ox, oy);
        icon
    })
}

/// Both variants of one app's 512 px tile, [light, dark].
pub fn render(glyph: Img, keep_dark: bool) -> Option<[Img; 2]> {
    let l = layers();
    let glyph = glyph.alpha_bbox_crop();
    if glyph.is_empty() {
        return None;
    }
    let (w, h) = (glyph.w, glyph.h);
    let st = shape_stats(&glyph);
    let aspect = w as f64 / h as f64;
    let square = (0.93..=1.07).contains(&aspect);

    if square && (st.coverage >= 0.90 || (st.disc >= 0.98 && st.ring >= 0.98)) {
        let mut layer = Img::new(C, C);
        let m = MARGIN as isize;
        if st.coverage < ROUND_BADGE {
            if let Some(disc) = round_badge(&glyph) {
                let o = ((C - DISC) / 2) as isize;
                return Some(on_theme_tiles(&disc, o, o));
            }
        }
        let fitted = if st.coverage < 0.995 { fit_to_squircle(&glyph) } else { None };
        match fitted {
            Some(f) => alpha_composite(&mut layer, &f, m, m),
            None => {
                let art = cover_resize(&glyph, TILE);
                if st.coverage < 0.995 {
                    alpha_composite(&mut layer, &bleed_backdrop(&art), m, m);
                }
                alpha_composite(&mut layer, &art, m, m);
            }
        }
        for (i, a) in l.mask.p.iter().enumerate() {
            let p = &mut layer.p[i * 4 + 3];
            *p = ((*p as u32 * *a as u32 + 127) / 255) as u8;
        }

        let mut branded: [Option<Img>; 2] = [None, None];
        if let Some(b) = brand::brand_tile(&layer) {
            branded = look::VARIANTS.map(|v| brand::brand_variant(&layer, &b, v));
            if let [Some(_), Some(_)] = &branded {
                let [a, b] = branded;
                return Some([a.unwrap(), b.unwrap()]);
            }
        }

        let stats = neutral::rim_stats(&layer);
        let own = if stats.n >= 100 && stats.sat < 0.12 { neutral::tone(stats.lum) } else { None };
        let back = own.and_then(|_| fit::backdrop(&layer));
        let lit = if neutral::rim_is_dark_neutral(&stats) && !keep_dark {
            neutral::lightify(&layer, stats.lum)
        } else {
            None
        };
        let light = match (&lit, &back, own) {
            (Some(_), Some(back), _) => {
                let k = neutral::light_k(stats.lum);
                neutral::retile(&layer, back, Variant::Light, Some(&|c| neutral::light_px(c, k)))
            }
            (Some(lit), None, _) => lit.clone(),
            (None, Some(back), Some(own)) => neutral::retile(&layer, back, own, None),
            _ => layer.clone(),
        };
        let dark = if neutral::rim_is_light_neutral(&stats) {
            match &back {
                Some(back) => neutral::retile(&layer, back, Variant::Dark, Some(&neutral::dark_px)),
                None => neutral::darkify(&layer),
            }
        } else {
            match (&back, own) {
                (Some(back), Some(own)) => neutral::retile(&layer, back, own, None),
                _ => layer.clone(),
            }
        };

        // every tile but a light one gets the corner glass, in both themes
        let mut light_out = composited(&l.shadow, &light);
        if neutral::rim_stats(&light).lum < 0.8 {
            alpha_composite(&mut light_out, &l.rim_glow, 0, 0);
        }
        alpha_composite(&mut light_out, &l.border_darkline, 0, 0);
        let mut dark_out = composited(&l.shadow, &dark);
        alpha_composite(&mut dark_out, &l.rim_glow, 0, 0);
        let [bl, bd] = branded;
        return Some([bl.unwrap_or(light_out), bd.unwrap_or(dark_out)]);
    }

    // 3. shaped / transparent logo: centred on the theme's tile
    let scale = (GLYPH_BOX / w as f64).min(GLYPH_BOX / h as f64);
    let g = resized(&glyph, ((w as f64 * scale).round() as usize).max(1), ((h as f64 * scale).round() as usize).max(1));
    let ox = ((C - g.w) / 2) as isize;
    let oy = ((C - g.h) / 2) as isize;
    Some(on_theme_tiles(&g, ox, oy))
}

// --- C ABI ----------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn astrea_app_tiles_master_extent() -> u32 {
    MASTER_EXTENT as u32
}

#[unsafe(no_mangle)]
pub extern "C" fn astrea_app_tiles_version() -> u32 {
    ALGORITHM_VERSION
}

/// Builds both 512 px variants of a tile from straight-alpha RGBA8 artwork.
/// `light_out` and `dark_out` each receive extent² × 4 bytes of straight RGBA8.
/// Returns 1 on success, 0 when the artwork is empty or an argument is invalid.
///
/// # Safety
/// `rgba` must point to `height` rows of `stride` bytes, each holding at least
/// `width` × 4 bytes; both outputs must be writable for extent² × 4 bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_app_tiles_render(
    rgba: *const u8,
    width: u32,
    height: u32,
    stride: u32,
    keep_dark: i32,
    light_out: *mut u8,
    dark_out: *mut u8,
) -> i32 {
    if rgba.is_null() || light_out.is_null() || dark_out.is_null() || width == 0 || height == 0
        || (stride as u64) < width as u64 * 4
    {
        return 0;
    }
    let (w, h, stride) = (width as usize, height as usize, stride as usize);
    let mut glyph = Img::new(w, h);
    for y in 0..h {
        // SAFETY: the caller guarantees `height` rows of `stride` readable bytes.
        let row = unsafe { std::slice::from_raw_parts(rgba.add(y * stride), w * 4) };
        glyph.p[y * w * 4..(y + 1) * w * 4].copy_from_slice(row);
    }
    let result = std::panic::catch_unwind(|| render(glyph, keep_dark != 0));
    let Ok(Some([light, dark])) = result else { return 0 };
    let n = C * C * 4;
    // SAFETY: the caller guarantees both outputs hold extent² × 4 bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(light.p.as_ptr(), light_out, n);
        std::ptr::copy_nonoverlapping(dark.p.as_ptr(), dark_out, n);
    }
    1
}

/// Restyles one finished 512 x 512 app icon (straight RGBA, `stride` bytes per
/// row) into `out` (512 x 512 straight RGBA, tightly packed). `style`: 1 dark,
/// 2 clear, 3 tinted; `dark` picks the style's dark-mode form; `tint` is
/// 0xRRGGBB. Returns 1 on success, 0 on bad input.
///
/// # Safety
/// `rgba` must point to `stride * 512` readable bytes and `out` to
/// 512 * 512 * 4 writable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn astrea_app_tiles_style(
    rgba: *const u8,
    w: u32,
    h: u32,
    stride: u32,
    style: u32,
    dark: i32,
    tint: u32,
    out: *mut u8,
) -> i32 {
    let Some(style) = style::Style::from_u32(style) else { return 0 };
    if rgba.is_null() || out.is_null() || w as usize != C || h as usize != C || (stride as usize) < C * 4 {
        return 0;
    }
    let mut src = Img::new(C, C);
    for y in 0..C {
        let row = unsafe { std::slice::from_raw_parts(rgba.add(y * stride as usize), C * 4) };
        src.p[y * C * 4..(y + 1) * C * 4].copy_from_slice(row);
    }
    let rgb = [((tint >> 16) & 0xff) as f64, ((tint >> 8) & 0xff) as f64, (tint & 0xff) as f64];
    let img = style::restyle(&src, style, dark != 0, rgb);
    unsafe { std::ptr::copy_nonoverlapping(img.p.as_ptr(), out, C * C * 4) };
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn astrea_app_tiles_style_version() -> u32 {
    STYLE_VERSION
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A hollow ring: a shaped logo, not a badge.
    fn ring(colour: [u8; 3], size: usize) -> Img {
        let mut img = Img::new(size, size);
        let c = size as f64 / 2.0;
        for y in 0..size {
            for x in 0..size {
                let d = (x as f64 + 0.5 - c).hypot(y as f64 + 0.5 - c);
                if d <= c * 0.6 && d >= c * 0.3 {
                    let i = img.idx(x, y);
                    img.p[i..i + 4].copy_from_slice(&[colour[0], colour[1], colour[2], 255]);
                }
            }
        }
        img
    }

    #[test]
    fn shaped_logo_sits_on_theme_tiles() {
        let [light, dark] = render(ring([220, 40, 40], 256), false).unwrap();
        // the ring's hole shows the theme ramp
        assert!(light.rgb(256, 256)[0] > 230.0, "light hole {:?}", light.rgb(256, 256));
        assert!(dark.rgb(256, 256)[0] < 80.0, "dark hole {:?}", dark.rgb(256, 256));
        // the glyph itself is unchanged
        assert_eq!(light.rgb(256, 120), [220.0, 40.0, 40.0]);
        assert_eq!(dark.rgb(256, 120), [220.0, 40.0, 40.0]);
        // outside the squircle stays transparent (shadow aside)
        assert_eq!(light.alpha(2, 2), 0);
    }

    #[test]
    fn white_full_bleed_tile_turns_graphite_in_dark() {
        let mut art = Img::new(256, 256);
        for px in art.p.chunks_mut(4) {
            px.copy_from_slice(&[250, 250, 250, 255]);
        }
        let [light, dark] = render(art, false).unwrap();
        assert!(light.rgb(256, 256)[0] > 230.0);
        assert!(dark.rgb(256, 256)[0] < 90.0, "dark centre {:?}", dark.rgb(256, 256));
    }

    #[test]
    fn brand_tile_keeps_its_colour() {
        let mut art = Img::new(256, 256);
        for px in art.p.chunks_mut(4) {
            px.copy_from_slice(&[30, 120, 230, 255]);
        }
        let [light, dark] = render(art, false).unwrap();
        for img in [&light, &dark] {
            let c = img.rgb(256, 300);
            assert!(c[2] > c[0] + 80.0, "brand hue kept {c:?}");
        }
    }

    /// A solid disc with a soft shadow under it, as spotify-launcher ships.
    fn shadowed_disc(colour: [u8; 3], size: usize) -> Img {
        let mut img = Img::new(size, size);
        let r = size as f64 * 0.45;
        let c = size as f64 / 2.0;
        for y in 0..size {
            for x in 0..size {
                let d = (x as f64 + 0.5 - c).hypot(y as f64 + 0.5 - c);
                let shadow = (x as f64 + 0.5 - c).hypot(y as f64 + 0.5 - c - 8.0);
                let i = img.idx(x, y);
                if d <= r {
                    img.p[i..i + 4].copy_from_slice(&[colour[0], colour[1], colour[2], 255]);
                } else if shadow <= r + 4.0 {
                    img.p[i..i + 4].copy_from_slice(&[0, 0, 0, 60]);
                }
            }
        }
        img
    }

    #[test]
    fn round_badge_sits_on_theme_tile_at_disc_size() {
        let [light, dark] = render(shadowed_disc([30, 215, 96], 256), false).unwrap();
        for img in [&light, &dark] {
            // the disc keeps its own colour, unrecoloured
            assert_eq!(img.rgb(256, 256), [30.0, 215.0, 96.0]);
            // just outside the disc is the theme tile, not the badge bled out
            let tile = img.rgb(256, 256 - DISC / 2 - 12);
            assert!(tile[1] < tile[0] + 20.0, "tile shows {tile:?}");
        }
        assert!(light.rgb(256, 64)[0] > 230.0);
        assert!(dark.rgb(256, 64)[0] < 80.0);
        // no shadow arc under the disc: below it is plain tile
        let below = dark.rgb(256, 256 + DISC / 2 + 6);
        assert!(below[1] < 60.0 && (below[0] - below[1]).abs() < 12.0, "below {below:?}");
    }

    #[test]
    fn empty_art_is_rejected() {
        let out = unsafe {
            astrea_app_tiles_render(std::ptr::null(), 0, 0, 0, 0, std::ptr::null_mut(), std::ptr::null_mut())
        };
        assert_eq!(out, 0);
    }
}
