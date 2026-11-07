//! Art on a neutral tile of its own: a white tile turns graphite in the dark
//! theme, a flat graphite one white in the light theme, and a smooth neutral
//! backdrop is swapped for the standard ramp.

use crate::fit::{Fit, at};
use crate::look::{
    DARK_LIFT, GLYPH_DARK_L, MARGIN, TILE, Variant, dark_mid_l, layers, light_mid_l,
};
use crate::raster::{Img, Rgb, chroma, clamp255, luma};

pub type ColourMap<'a> = &'a dyn Fn(&Rgb) -> Rgb;

#[derive(Clone, Copy, Default, Debug)]
pub struct RimStats {
    pub n: usize,
    pub lum: f64,
    pub spread: f64,
    pub sat: f64,
}

/// The artwork's own backdrop, read off the band just inside the squircle.
pub fn rim_stats(layer: &Img) -> RimStats {
    let rp = &layers().rim_band_mask;
    let mut ls = Vec::new();
    let mut ss = 0.0;
    for y in (MARGIN..MARGIN + TILE).step_by(3) {
        for x in (MARGIN..MARGIN + TILE).step_by(3) {
            if rp.at(x, y) < 200 || layer.alpha(x, y) < 200 {
                continue;
            }
            let c = layer.rgb(x, y);
            let mx = c[0].max(c[1]).max(c[2]);
            ls.push(luma(&c));
            ss += if mx > 0.0 { chroma(&c) / mx } else { 0.0 };
        }
    }
    if ls.is_empty() {
        return RimStats::default();
    }
    let n = ls.len() as f64;
    let mean = ls.iter().sum::<f64>() / n;
    let var = ls.iter().map(|v| (v - mean) * (v - mean)).sum::<f64>() / n;
    RimStats { n: ls.len(), lum: mean, spread: var.sqrt(), sat: ss / n }
}

pub fn rim_is_light_neutral(s: &RimStats) -> bool {
    s.n >= 100 && s.lum > 0.82 && s.sat < 0.12
}

pub fn rim_is_dark_neutral(s: &RimStats) -> bool {
    s.n >= 100 && s.lum < 0.40 && s.sat < 0.12 && s.spread < 0.03
}

/// Near-neutral colours invert, aimed so white lands on the dark tile's mid
/// tone; saturated brand colours stay.
pub fn dark_px(c: &Rgb) -> Rgb {
    let mx = c[0].max(c[1]).max(c[2]);
    let ratio = if mx > 0.0 { chroma(c) / mx } else { 0.0 };
    let w = 1.0 - (ratio / 0.18).min(1.0);
    if w <= 0.0 {
        return *c;
    }
    let l = luma(c);
    let d = (DARK_LIFT - (DARK_LIFT - dark_mid_l()) * l) - l;
    c.map(|v| v + (clamp255(v + d * 255.0) - v) * w)
}

/// dark_px's mirror, with white landing on near-black.
pub fn light_px(c: &Rgb, k: f64) -> Rgb {
    let ch = chroma(c) / 255.0;
    let w = 1.0 - ((ch - 0.04).max(0.0) / 0.14).min(1.0);
    if w <= 0.0 {
        return *c;
    }
    let l = luma(c);
    let d = (GLYPH_DARK_L + k * (1.0 - l)) - l;
    c.map(|v| v + (clamp255(v + d * 255.0) - v) * w)
}

pub fn light_k(back_l: f64) -> f64 {
    (light_mid_l() - GLYPH_DARK_L) / (1.0 - back_l).max(0.05)
}

fn remap(layer: &Img, f: ColourMap) -> Img {
    let mut out = layer.clone();
    let mp = &layers().mask;
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            if mp.at(x, y) == 0 || out.alpha(x, y) < 250 {
                continue; // the artwork's own soft shadow: leave it dark
            }
            let m = f(&out.rgb(x, y));
            out.set_rgb(x, y, &m);
        }
    }
    out
}

pub fn darkify(layer: &Img) -> Img {
    remap(layer, &dark_px)
}

/// None when the coloured art then loses most of its contrast (a yellow logo
/// on white): keep it dark then.
pub fn lightify(layer: &Img, back_l: f64) -> Option<Img> {
    let mp = &layers().mask;
    let mid = light_mid_l();
    let (mut n, mut c0, mut c1) = (0.0, 0.0, 0.0);
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            if mp.at(x, y) == 0 || layer.alpha(x, y) < 250 {
                continue;
            }
            let c = layer.rgb(x, y);
            if chroma(&c) >= 0.18 * 255.0 {
                let l = luma(&c);
                n += 1.0;
                c0 += (l - back_l).abs();
                c1 += (l - mid).abs();
            }
        }
    }
    if n > 0.01 * (TILE * TILE) as f64 && c1 < 0.3 * n && c1 < 0.7 * c0 {
        return None;
    }
    let k = light_k(back_l);
    Some(remap(layer, &|c| light_px(c, k)))
}

/// Map the art through f, then move whatever shows its own tile onto the
/// standard one, by how much of each pixel is tile.
pub fn retile(layer: &Img, back: &Fit, variant: Variant, f: Option<ColourMap>) -> Img {
    let mut out = layer.clone();
    let mp = &layers().mask;
    let rows = &layers().tile_row[variant as usize];
    for y in MARGIN..MARGIN + TILE {
        let ty = rows[y];
        for x in MARGIN..MARGIN + TILE {
            if mp.at(x, y) == 0 {
                continue;
            }
            let b = at(back, x as f64, y as f64);
            let c = out.rgb(x, y);
            let mut al: f64 = 0.0;
            for i in 0..3 {
                let d = (c[i] - b[i]).abs() - 3.0;
                if d > 0.0 {
                    let denom = if c[i] > b[i] { 255.0 - b[i] } else { b[i] };
                    al = al.max(if denom > 0.0 { d / denom } else { 1.0 });
                }
            }
            let col = if al <= 0.0 {
                ty
            } else {
                let al = al.min(1.0);
                let (fp, fb) = match f {
                    Some(f) => (f(&c), f(&b)),
                    None => (c, b),
                };
                [0, 1, 2].map(|i| fp[i] + (1.0 - al) * (ty[i] - fb[i]))
            };
            out.set_rgb(x, y, &col);
        }
    }
    out
}

/// Which standard tile a neutral backdrop of this luma belongs on, if any.
pub fn tone(lum: f64) -> Option<Variant> {
    if lum > 0.8 {
        Some(Variant::Light)
    } else if lum < 0.45 {
        Some(Variant::Dark)
    } else {
        None
    }
}
