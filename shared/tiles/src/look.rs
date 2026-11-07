//! The tile's look (MacOS-Like's "# >>> look" block) and the shared squircle
//! layers built from it once.

use std::sync::OnceLock;

use crate::raster::{Img, Plane, Rgb, alpha_composite, composited, luma, rounded_rect, to_byte};

pub const C: usize = 512; // canvas
pub const MARGIN: usize = 32;
pub const TILE: usize = 448;
pub const RADIUS: f64 = 104.0; // = 4, 56, 13 on MacTahoe's 64 px grid
pub const GLYPH_BOX: f64 = 328.0; // glyph fits a centred box ~73% of the tile

const fn hex(v: u32) -> Rgb {
    [((v >> 16) & 0xff) as f64, ((v >> 8) & 0xff) as f64, (v & 0xff) as f64]
}

pub const DARK_TOP: Rgb = hex(0x2c2c2f);
pub const DARK_BOT: Rgb = hex(0x1c1c1e);
pub const DARK_HL: f64 = 0.10;
pub const DARK_HL_SPAN: f64 = 0.55;
pub const DARK_RIM: f64 = 0.35;
pub const LIGHT_TOP: Rgb = hex(0xfdfcfc);
pub const LIGHT_BOT: Rgb = hex(0xf1efeb);
pub const SHADOW_A: f64 = 70.0;
pub const SHADOW_BLUR: f64 = 10.0;
pub const SHADOW_DROP: usize = 8;
pub const OUTLINE_A: u8 = 22;
pub const BRAND_HL_LIGHT: f64 = 0.22;
pub const BRAND_HL_DARK: f64 = 0.12;
pub const BRAND_RIM: f64 = 0.45;

pub const DARK_LIFT: f64 = 1.02;
pub const GLYPH_DARK_L: f64 = 0.11;

pub fn dark_mid_l() -> f64 {
    (luma(&DARK_TOP) + luma(&DARK_BOT)) / 2.0
}

pub fn light_mid_l() -> f64 {
    (luma(&LIGHT_TOP) + luma(&LIGHT_BOT)) / 2.0
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Variant {
    Light = 0,
    Dark = 1,
}

pub const VARIANTS: [Variant; 2] = [Variant::Light, Variant::Dark];

pub struct Layers {
    pub mask: Plane,
    pub mask_tile: Plane,
    pub mask_inner1: Plane, // the squircle 2 px in: its difference is the edge line
    pub rim_band_mask: Plane, // 16 px band just inside the squircle edge
    pub shadow: Img,
    pub border_darkline: Img,
    pub rim_glow: Img,
    pub tile_row: [Vec<Rgb>; 2],
    pub base: [Img; 2],
    pub brand_edge: [Img; 2],
    pub in128: Plane,
    pub pts128: Vec<(usize, usize)>,
}

pub fn squircle(inset: f64) -> Plane {
    let m = MARGIN as f64;
    let t = TILE as f64;
    rounded_rect(C, C, m + inset, m + inset, m + t - 1.0 - inset, m + t - 1.0 - inset, RADIUS - inset)
}

fn border(mask: &Plane, alpha: u8) -> Img {
    let inner = squircle(2.0);
    let mut out = Img::new(C, C);
    for y in 0..C {
        for x in 0..C {
            if mask.at(x, y) != 0 && inner.at(x, y) == 0 {
                let i = out.idx(x, y);
                out.p[i + 3] = alpha;
            }
        }
    }
    out
}

/// 255 at p0 falling linearly to 0 at p1, clamped beyond both ends.
fn ramp(x0: f64, y0: f64, x1: f64, y1: f64) -> Plane {
    let (dx, dy) = (x1 - x0, y1 - y0);
    let n = dx * dx + dy * dy;
    let mut g = Plane::new(C, C);
    for y in 0..C {
        for x in 0..C {
            let t = ((x as f64 - x0) * dx + (y as f64 - y0) * dy) / n;
            g.p[y * C + x] = if t >= 1.0 { 0 } else if t <= 0.0 { 255 } else { to_byte(255.0 * (1.0 - t)) };
        }
    }
    g
}

/// Tahoe's corner rim: a 1 unit band just inside the squircle edge, lit from
/// the top-left and bottom-right corners.
pub fn rim_layer(mask: &Plane, opacity: f64) -> Img {
    let k = C as f64 / 64.0;
    let ring = mask.subtract(&squircle(k));
    let ramps = [[9.82, 10.719, 40.0, 42.755], [57.0, 56.0, 48.0, 46.0]];
    let mut a = Plane::new(C, C);
    for r in ramps {
        let g = ramp(r[0] * k, r[1] * k, r[2] * k, r[3] * k);
        for i in 0..a.p.len() {
            let gi = (ring.p[i] as u32 * g.p[i] as u32 + 127) / 255;
            let add = (gi * (255 - a.p[i] as u32) + 127) / 255;
            a.p[i] = (a.p[i] as u32 + add).min(255) as u8;
        }
    }
    let a = a.blurred(0.28 * k);
    let mut out = Img::new(C, C);
    for i in 0..C * C {
        out.p[i * 4..i * 4 + 3].copy_from_slice(&[255, 255, 255]);
        out.p[i * 4 + 3] = to_byte(a.p[i] as f64 * opacity);
    }
    out
}

/// The tile's colour on every canvas row (clamped above and below it).
pub fn tile_rows(top: &Rgb, bot: &Rgb, hl: f64, hl_span: f64) -> Vec<Rgb> {
    (0..C)
        .map(|y| {
            let t = ((y as f64 - MARGIN as f64) / TILE as f64).clamp(0.0, 1.0);
            let mut col = [0.0; 3];
            for c in 0..3 {
                col[c] = (top[c] + (bot[c] - top[c]) * t).round();
            }
            if hl > 0.0 {
                let a = hl * (1.0 - t / hl_span).max(0.0);
                for v in &mut col {
                    *v = (*v + (255.0 - *v) * a).round();
                }
            }
            col
        })
        .collect()
}

fn build() -> Layers {
    let mask = squircle(0.0);
    let mut mask_tile = Plane::new(TILE, TILE);
    for y in 0..TILE {
        for x in 0..TILE {
            mask_tile.p[y * TILE + x] = mask.at(x + MARGIN, y + MARGIN);
        }
    }
    let rim_band_mask = mask.subtract(&squircle(16.0));

    let soft = mask.blurred(SHADOW_BLUR);
    let mut shadow = Img::new(C, C);
    for y in 0..C - SHADOW_DROP {
        for x in 0..C {
            let i = shadow.idx(x, y + SHADOW_DROP);
            shadow.p[i + 3] = to_byte(SHADOW_A * soft.at(x, y) as f64 / 255.0);
        }
    }
    let border_darkline = border(&mask, OUTLINE_A);
    let rim_glow = rim_layer(&mask, DARK_RIM);
    let rim_brand = rim_layer(&mask, BRAND_RIM);
    let tile_row = [
        tile_rows(&LIGHT_TOP, &LIGHT_BOT, 0.0, 1.0),
        tile_rows(&DARK_TOP, &DARK_BOT, DARK_HL, DARK_HL_SPAN),
    ];
    let base = [0usize, 1].map(|v| {
        let mut grad = Img::new(C, C);
        for y in MARGIN..MARGIN + TILE {
            let col = tile_row[v][y];
            for x in MARGIN..MARGIN + TILE {
                let i = grad.idx(x, y);
                grad.p[i] = col[0] as u8;
                grad.p[i + 1] = col[1] as u8;
                grad.p[i + 2] = col[2] as u8;
                grad.p[i + 3] = mask.at(x, y);
            }
        }
        let mut b = composited(&shadow, &grad);
        alpha_composite(&mut b, if v == 0 { &border_darkline } else { &rim_glow }, 0, 0);
        b
    });
    let brand_edge = [composited(&rim_brand, &border_darkline), rim_brand.clone()];

    let in128 = rounded_rect(128, 128, 10.0, 10.0, 117.0, 117.0, 24.0);
    let pts128 = (0..128)
        .step_by(2)
        .flat_map(|y| (0..128).step_by(2).map(move |x| (x, y)))
        .filter(|&(x, y)| in128.at(x, y) == 255)
        .collect();

    let mask_inner1 = squircle(2.0);
    Layers { mask, mask_tile, mask_inner1, rim_band_mask, shadow, border_darkline, rim_glow, tile_row, base,
             brand_edge, in128, pts128 }
}

pub fn layers() -> &'static Layers {
    static LAYERS: OnceLock<Layers> = OnceLock::new();
    LAYERS.get_or_init(build)
}
