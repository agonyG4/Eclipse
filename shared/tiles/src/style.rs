//! macOS Tahoe icon styles (Dark, Clear, Tinted) over any app icon.
//!
//! The input is a finished 512 px icon: one of our tiles or the theme's own
//! art. It is taken apart into the squircle, the tile's own colour (a smooth
//! surface fitted to its edge band, as retiling does it) and the glyph, the
//! part that stands off that surface. The glyph is then set again on the
//! style's tile:
//!   Dark    dark tile, the glyph in its own colours; a white glyph on a
//!           coloured tile takes that colour, a black one turns light
//!   Clear   frosted glass (translucent, so the dock or panel blur behind it
//!           shows through), the glyph as one light or dark tone
//!   Tinted  the glyph as one tone of the tint, on a dark tile (dark mode) or
//!           on the tint itself (light mode)
//! Art that is not a tile (a shaped logo) is the glyph as a whole; a tile with
//! no smooth backdrop (a picture) keeps its picture, toned for Clear/Tinted.

use crate::fit::{self, Field};
use crate::look::{C, MARGIN, TILE, layers, rim_layer, squircle, tile_rows, DARK_BOT, DARK_HL, DARK_HL_SPAN, DARK_TOP};
use crate::raster::{Img, Plane, Rgb, alpha_composite, chroma, luma, to_byte};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Style {
    Dark = 1,
    Clear = 2,
    Tinted = 3,
}

impl Style {
    pub fn from_u32(v: u32) -> Option<Self> {
        match v {
            1 => Some(Self::Dark),
            2 => Some(Self::Clear),
            3 => Some(Self::Tinted),
            _ => None,
        }
    }
}

/// How the icon came apart.
struct Parts {
    /// squircle coverage, 0..255, of the tile being drawn
    mask: Plane,
    /// how much each pixel is glyph, 0..1
    weight: Vec<f64>,
    /// the tile's own mean colour (None for a shaped logo)
    backdrop: Option<Rgb>,
    /// glyph tone 0..1 per pixel: its luma, stretched over the glyph's range
    tone: Vec<f64>,
}

fn smoothstep(e0: f64, e1: f64, x: f64) -> f64 {
    let t = ((x - e0) / (e1 - e0)).clamp(0.0, 1.0);
    t * t * (3.0 - 2.0 * t)
}

fn mix(a: &Rgb, b: &Rgb, t: f64) -> Rgb {
    [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t]
}

fn scale(c: &Rgb, k: f64) -> Rgb {
    [c[0] * k, c[1] * k, c[2] * k]
}

/// `c` moved to luma `l` (0..1) keeping its hue, clipped into gamut.
fn with_luma(c: &Rgb, l: f64) -> Rgb {
    let cur = luma(c).max(1e-3);
    let k = l / cur;
    let mut out = scale(c, k);
    let over = out.iter().cloned().fold(0.0, f64::max);
    if over > 255.0 {
        // past white: blend toward it instead of clipping the hue away
        let white = [255.0; 3];
        let clipped = scale(&out, 255.0 / over);
        let need = (l - luma(&clipped)).max(0.0) / (1.0 - luma(&clipped)).max(1e-3);
        out = mix(&clipped, &white, need.clamp(0.0, 1.0));
    }
    out
}

/// The finished icon's own tile as a smooth surface. Like fit::backdrop, but
/// sampled a few pixels in from the edge, past the rim light and outline a
/// finished icon carries there, and more tolerant of the art's own sheen.
fn icon_backdrop(src: &Img) -> Option<fit::Fit> {
    use std::sync::OnceLock;
    static BAND: OnceLock<Plane> = OnceLock::new();
    let band = BAND.get_or_init(|| squircle(6.0).subtract(&squircle(24.0)));
    let mut samples = Vec::new();
    for y in (MARGIN..MARGIN + TILE).step_by(2) {
        for x in (MARGIN..MARGIN + TILE).step_by(2) {
            if band.at(x, y) >= 200 && src.alpha(x, y) >= 250 {
                samples.push(fit::Sample { x: x as f64, y: y as f64, c: src.rgb(x, y) });
            }
        }
    }
    if samples.len() < 100 {
        return None;
    }
    let mut keep = samples.clone();
    let mut fitted = None;
    for _ in 0..3 {
        let f = fit::surface(&keep, 6)?;
        keep = samples.iter().copied().filter(|s| fit::off(&s.c, &fit::at(&f, s.x, s.y)) <= 16.0).collect();
        if (keep.len() as f64) < 0.70 * samples.len() as f64 {
            return None;
        }
        fitted = Some(f);
    }
    fitted
}

fn decompose(src: &Img) -> Parts {
    let l = layers();
    let n = C * C;
    // Tile art covers the squircle; anything else is a shaped logo.
    let mut inside = 0u64;
    let mut miss = 0u64;
    for y in 0..C {
        for x in 0..C {
            let m = l.mask.at(x, y) as u64;
            inside += m;
            if src.alpha(x, y) <= 200 {
                miss += m;
            }
        }
    }
    let tile_art = (miss as f64) <= 0.03 * inside as f64;

    let mut weight = vec![0.0; n];
    let mut backdrop = None;
    if tile_art {
        match icon_backdrop(src) {
            Some(f) => {
                let field = Field::new(&f);
                let mut sum = [0.0; 3];
                let mut cnt = 0.0;
                for y in MARGIN..MARGIN + TILE {
                    for x in MARGIN..MARGIN + TILE {
                        let i = y * C + x;
                        if l.mask.at(x, y) == 0 {
                            continue;
                        }
                        let bg = field.get(x, y);
                        let px = src.rgb(x, y);
                        let w = smoothstep(12.0, 44.0, fit::off(&px, bg));
                        weight[i] = w * src.alpha(x, y) as f64 / 255.0;
                        if w < 0.5 {
                            for c in 0..3 {
                                sum[c] += bg[c];
                            }
                            cnt += 1.0;
                        }
                    }
                }
                if cnt > 0.0 {
                    backdrop = Some(scale(&sum, 1.0 / cnt));
                }
            }
            None => {
                // a picture: all of it is glyph
                for y in 0..C {
                    for x in 0..C {
                        weight[y * C + x] = src.alpha(x, y) as f64 / 255.0;
                    }
                }
            }
        }
    } else {
        for y in 0..C {
            for x in 0..C {
                weight[y * C + x] = src.alpha(x, y) as f64 / 255.0;
            }
        }
    }

    // Tone: luma stretched between the glyph's 5th and 95th percentile, so a
    // one-colour glyph is one tone and a drawn one keeps its shading.
    let mut lumas: Vec<f64> = (0..n)
        .filter(|&i| weight[i] > 0.6)
        .map(|i| luma(&src.rgb(i % C, i / C)))
        .collect();
    lumas.sort_by(f64::total_cmp);
    let (lo, hi) = if lumas.len() > 20 {
        (lumas[lumas.len() / 20], lumas[lumas.len() * 19 / 20])
    } else {
        (0.0, 1.0)
    };
    let tone = (0..n)
        .map(|i| {
            if weight[i] <= 0.0 {
                return 1.0;
            }
            if hi - lo < 0.12 {
                1.0
            } else {
                ((luma(&src.rgb(i % C, i / C)) - lo) / (hi - lo)).clamp(0.0, 1.0)
            }
        })
        .collect();

    Parts { mask: l.mask.clone(), weight, backdrop, tone }
}

/// A tile filled row by row with `rows`, at `alpha` (0..1) per row.
fn fill_tile(mask: &Plane, rows: &[Rgb], alpha: &dyn Fn(usize) -> f64) -> Img {
    let mut out = Img::new(C, C);
    for y in 0..C {
        let a = alpha(y);
        for x in 0..C {
            let m = mask.at(x, y);
            if m == 0 {
                continue;
            }
            let i = out.idx(x, y);
            out.p[i] = to_byte(rows[y][0]);
            out.p[i + 1] = to_byte(rows[y][1]);
            out.p[i + 2] = to_byte(rows[y][2]);
            out.p[i + 3] = to_byte(m as f64 * a);
        }
    }
    out
}

fn shadow(alpha_scale: f64) -> Img {
    let mut s = layers().shadow.clone();
    for px in s.p.chunks_mut(4) {
        px[3] = to_byte(px[3] as f64 * alpha_scale);
    }
    s
}

/// A light 1 px line just inside the squircle: the glass's edge.
fn glass_edge(mask: &Plane, top: f64, bottom: f64) -> Img {
    let l = layers();
    let mut out = Img::new(C, C);
    for y in 0..C {
        let t = ((y as f64 - MARGIN as f64) / TILE as f64).clamp(0.0, 1.0);
        let a = top + (bottom - top) * t;
        for x in 0..C {
            let ring = mask.at(x, y) as f64 - l.mask_inner1.at(x, y) as f64;
            if ring > 0.0 {
                let i = out.idx(x, y);
                out.p[i..i + 3].copy_from_slice(&[255, 255, 255]);
                out.p[i + 3] = to_byte(ring * a);
            }
        }
    }
    out
}

/// The glyph's colour at pixel `i` for a style.
fn glyph_colour(style: Style, dark: bool, tint: &Rgb, src_px: &Rgb, tone: f64, backdrop: Option<&Rgb>) -> Rgb {
    match style {
        Style::Dark => {
            let c = chroma(src_px);
            let l = luma(src_px);
            match backdrop {
                // a white glyph on a coloured tile takes the tile's colour
                Some(bg) if c < 28.0 && l > 0.78 && chroma(bg) > 60.0 => with_luma(bg, (0.48 + 0.4 * tone).max(luma(bg))),
                // a black or dark grey glyph turns light, its shading kept
                _ if c < 28.0 && l < 0.32 => {
                    let v = 255.0 * (0.94 - l * 0.6);
                    [v, v, v]
                }
                _ => *src_px,
            }
        }
        Style::Clear => {
            let v = if dark { 255.0 * (0.78 + 0.22 * tone) } else { 255.0 * (0.30 - 0.14 * tone) };
            [v, v, v]
        }
        Style::Tinted => {
            if dark {
                let bright = with_luma(tint, 0.62_f64.max(luma(tint)).min(0.85));
                mix(&with_luma(tint, 0.28), &bright, 0.35 + 0.65 * tone)
            } else {
                mix(&mix(tint, &[255.0; 3], 0.55), &[255.0; 3], 0.4 + 0.6 * tone)
            }
        }
    }
}

/// One app icon in `style`; `dark` picks the style's dark-mode form.
pub fn restyle(src: &Img, style: Style, dark: bool, tint: Rgb) -> Img {
    let parts = decompose(src);
    let mask = &parts.mask;
    if style == Style::Dark && parts.backdrop.is_none() {
        // a picture or a shaped logo has no tile colour to darken: as it is
        return src.clone();
    }

    // The tile.
    let mut out = match style {
        Style::Dark => shadow(1.0),
        Style::Clear => shadow(0.45),
        Style::Tinted => shadow(if dark { 1.0 } else { 0.8 }),
    };
    let tile = match style {
        Style::Dark => fill_tile(mask, &tile_rows(&DARK_TOP, &DARK_BOT, DARK_HL, DARK_HL_SPAN), &|_| 1.0),
        Style::Clear => {
            let (top, bot) = if dark { ([46.0, 46.0, 50.0], [22.0, 22.0, 26.0]) } else { ([255.0; 3], [236.0, 238.0, 242.0]) };
            let (at, ab) = if dark { (0.42, 0.56) } else { (0.52, 0.36) };
            fill_tile(mask, &tile_rows(&top, &bot, 0.0, 1.0), &|y| {
                let t = ((y as f64 - MARGIN as f64) / TILE as f64).clamp(0.0, 1.0);
                at + (ab - at) * t
            })
        }
        Style::Tinted => {
            let rows = if dark {
                tile_rows(&mix(&DARK_TOP, &tint, 0.10), &mix(&DARK_BOT, &tint, 0.06), DARK_HL, DARK_HL_SPAN)
            } else {
                tile_rows(&mix(&tint, &[255.0; 3], 0.22), &scale(&tint, 0.86), 0.08, 0.6)
            };
            fill_tile(mask, &rows, &|_| 1.0)
        }
    };
    alpha_composite(&mut out, &tile, 0, 0);

    // The glyph.
    let mut glyph = Img::new(C, C);
    for y in 0..C {
        for x in 0..C {
            let i = y * C + x;
            let m = mask.at(x, y) as f64 / 255.0;
            let w = parts.weight[i] * m;
            if w <= 0.0 {
                continue;
            }
            let c = glyph_colour(style, dark, &tint, &src.rgb(x, y), parts.tone[i], parts.backdrop.as_ref());
            glyph.set_rgb(x, y, &c);
            let o = glyph.idx(x, y) + 3;
            glyph.p[o] = to_byte(255.0 * w);
        }
    }
    alpha_composite(&mut out, &glyph, 0, 0);

    // The glass.
    let l = layers();
    match style {
        Style::Clear => {
            alpha_composite(&mut out, &rim_layer(mask, if dark { 0.55 } else { 0.85 }), 0, 0);
            alpha_composite(&mut out, &glass_edge(mask, if dark { 0.30 } else { 0.75 }, if dark { 0.10 } else { 0.35 }), 0, 0);
        }
        Style::Tinted if !dark => {
            alpha_composite(&mut out, &l.brand_edge[0], 0, 0);
        }
        _ => alpha_composite(&mut out, &l.rim_glow, 0, 0),
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn icon(tile: [u8; 3], glyph: [u8; 3]) -> Img {
        let l = layers();
        let mut img = Img::new(C, C);
        for y in 0..C {
            for x in 0..C {
                let i = img.idx(x, y);
                let in_glyph = (200..312).contains(&x) && (200..312).contains(&y);
                let c = if in_glyph { glyph } else { tile };
                img.p[i..i + 3].copy_from_slice(&c);
                img.p[i + 3] = l.mask.at(x, y);
            }
        }
        img
    }

    #[test]
    fn dark_style_takes_brand_colour_into_white_glyph() {
        let out = restyle(&icon([40, 200, 80], [255, 255, 255]), Style::Dark, true, [0.0; 3]);
        let tile = out.rgb(100, 400);
        assert!(luma(&tile) < 0.2, "tile {tile:?}");
        let g = out.rgb(256, 256);
        assert!(g[1] > g[0] + 60.0, "glyph keeps the brand green {g:?}");
    }

    #[test]
    fn clear_style_is_translucent_with_one_tone_glyph() {
        let out = restyle(&icon([220, 40, 40], [255, 255, 255]), Style::Clear, true, [0.0; 3]);
        let a = out.alpha(100, 100);
        assert!(a > 60 && a < 200, "glass alpha {a}");
        let g = out.rgb(256, 256);
        assert!(chroma(&g) < 4.0 && luma(&g) > 0.9, "white glyph {g:?}");
    }

    #[test]
    fn tinted_light_sits_on_the_tint() {
        let tint = [10.0, 132.0, 255.0];
        let out = restyle(&icon([250, 250, 250], [20, 20, 20]), Style::Tinted, false, tint);
        let tile = out.rgb(256, 120);
        assert!(tile[2] > tile[0] + 80.0, "blue tile {tile:?}");
        let g = out.rgb(256, 256);
        assert!(luma(&g) > 0.8, "light glyph {g:?}");
    }
}
