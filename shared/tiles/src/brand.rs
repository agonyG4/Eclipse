//! Art on a brand-colour tile of its own (Craft, Beeper, Claude): the tile is
//! fitted as a smooth surface, every pixel is read as bare tile, tile under a
//! translucent shade or glow, or art, and the tile is repainted as a Tahoe
//! ramp of its own colour. An icon that fails any test keeps the plain
//! full-bleed treatment, never a half-repainted tile.

use std::collections::HashMap;

use crate::fit::{Field, Fit, Sample, at, band_samples, off, surface};
use crate::look::{
    BRAND_HL_DARK, BRAND_HL_LIGHT, C, DARK_HL_SPAN, LIGHT_TOP, MARGIN, TILE, Variant, layers,
    tile_rows,
};
use crate::raster::{Img, Rgb, alpha_composite, chroma, composited, luma, resized};

const TOL: f64 = 10.0;
const NSEC: usize = 72;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Kind {
    None,
    Tile,
    Shade,
    Glow,
    Art,
}

fn classify(c: &Rgb, b: &Rgb) -> (Kind, f64) {
    if off(c, b) <= TOL {
        return (Kind::Tile, 0.0);
    }
    let n: f64 = b.iter().map(|v| v * v).sum();
    if n > 0.0 {
        let k = (c[0] * b[0] + c[1] * b[1] + c[2] * b[2]) / n;
        if (0.3..1.0).contains(&k) && (0..3).all(|i| (c[i] - k * b[i]).abs() <= TOL) {
            return (Kind::Shade, k);
        }
    }
    let w = b.map(|v| 255.0 - v);
    let n: f64 = w.iter().map(|v| v * v).sum();
    if n > 0.0 {
        let k = (0..3).map(|i| (c[i] - b[i]) * w[i]).sum::<f64>() / n;
        if k > 0.0 && k <= 0.7 && (0..3).all(|i| (c[i] - (b[i] + k * w[i])).abs() <= TOL) {
            return (Kind::Glow, k);
        }
    }
    (Kind::Art, 0.0)
}

fn median(mut v: Vec<f64>) -> f64 {
    v.sort_by(f64::total_cmp);
    let n = v.len();
    if n % 2 == 1 { v[n / 2] } else { (v[n / 2 - 1] + v[n / 2]) / 2.0 }
}

/// Deterministic xorshift: every run picks the same tile.
struct Rng(u64);

impl Rng {
    fn below(&mut self, n: usize) -> usize {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 % n as u64) as usize
    }
}

/// The tile as a quadratic surface from the edge band: sector medians, then
/// RANSAC over them, refined on the band samples it explains.
fn band_fit(samples: &[Sample]) -> Option<Fit> {
    let mut sec: Vec<Vec<&Sample>> = vec![Vec::new(); NSEC];
    let h = C as f64 / 2.0;
    for s in samples {
        let a = (s.y - h).atan2(s.x - h);
        let i = ((a + std::f64::consts::PI) / (2.0 * std::f64::consts::PI) * NSEC as f64) as usize % NSEC;
        sec[i].push(s);
    }
    let mut meds = Vec::new();
    for ss in &sec {
        if ss.len() < 8 {
            continue;
        }
        let m = [0, 1, 2].map(|i| median(ss.iter().map(|s| s.c[i]).collect()));
        let near = ss.iter().filter(|s| off(&s.c, &m) <= 12.0).count();
        if near as f64 >= 0.6 * ss.len() as f64 {
            let n = ss.len() as f64;
            meds.push(Sample {
                x: ss.iter().map(|s| s.x).sum::<f64>() / n,
                y: ss.iter().map(|s| s.y).sum::<f64>() / n,
                c: m,
            });
        }
    }
    if meds.len() < 12 {
        return None;
    }
    let mut rng = Rng(0x9e37_79b9_7f4a_7c15);
    let (mut best, mut most) = (None, -1i64);
    let mut index: Vec<usize> = (0..meds.len()).collect();
    for _ in 0..300 {
        for i in 0..8 {
            let j = i + rng.below(index.len() - i);
            index.swap(i, j);
        }
        let pick: Vec<Sample> = index[..8].iter().map(|&i| meds[i]).collect();
        let Some(fit) = surface(&pick, 6) else { continue };
        let n = meds.iter().filter(|m| off(&m.c, &at(&fit, m.x, m.y)) <= TOL).count() as i64;
        if n > most {
            best = Some(fit);
            most = n;
        }
    }
    let best = best?;
    let mut keep: Vec<Sample> =
        meds.iter().copied().filter(|m| off(&m.c, &at(&best, m.x, m.y)) <= TOL).collect();
    let mut fit = None;
    for _ in 0..3 {
        if keep.len() < 12 {
            return None;
        }
        let f = surface(&keep, 6)?;
        keep = samples.iter().copied().filter(|s| off(&s.c, &at(&f, s.x, s.y)) <= 12.0).collect();
        fit = Some(f);
    }
    fit
}

/// Refit on every squircle pixel that reads as bare tile, cubic once the
/// samples span it.
fn grow_fit(layer: &Img, mut fit: Fit) -> Fit {
    let mp = &layers().mask;
    let mut pts = Vec::new();
    for y in (MARGIN..MARGIN + TILE).step_by(3) {
        for x in (MARGIN..MARGIN + TILE).step_by(3) {
            if mp.at(x, y) == 255 && layer.alpha(x, y) >= 250 {
                pts.push(Sample { x: x as f64, y: y as f64, c: layer.rgb(x, y) });
            }
        }
    }
    for _ in 0..4 {
        let keep: Vec<Sample> =
            pts.iter().copied().filter(|p| off(&p.c, &at(&fit, p.x, p.y)) <= TOL).collect();
        if keep.len() < 60 {
            break;
        }
        match surface(&keep, if keep.len() >= 400 { 10 } else { 6 }) {
            Some(f) => fit = f,
            None => break,
        }
    }
    fit
}

pub struct Classes {
    pub kind: Vec<Kind>, // Kind::None outside the squircle
    pub value: Vec<f64>,
}

impl Classes {
    fn get(&self, x: isize, y: isize, outside: Kind) -> Kind {
        if x < 0 || y < 0 || x >= C as isize || y >= C as isize {
            return outside;
        }
        match self.kind[y as usize * C + x as usize] {
            Kind::None => outside,
            k => k,
        }
    }
}

/// classify() for every pixel of the squircle; specks of art, shade or glow
/// are tile grain, not design, and count as tile.
fn classes(layer: &Img, field: &Field) -> Classes {
    let mp = &layers().mask;
    let mut out = Classes { kind: vec![Kind::None; C * C], value: vec![0.0; C * C] };
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            if mp.at(x, y) != 0 {
                let (k, v) = classify(&layer.rgb(x, y), field.get(x, y));
                out.kind[y * C + x] = k;
                out.value[y * C + x] = v;
            }
        }
    }
    let mut seen = vec![false; C * C];
    let mut blob = Vec::new();
    let mut todo = Vec::new();
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            let p = y * C + x;
            let k = out.kind[p];
            if matches!(k, Kind::None | Kind::Tile) || seen[p] {
                continue;
            }
            blob.clear();
            blob.push(p);
            todo.clear();
            todo.push(p);
            seen[p] = true;
            while let Some(q) = todo.pop() {
                let (qx, qy) = (q % C, q / C);
                let nb = [(qx + 1, qy), (qx.wrapping_sub(1), qy), (qx, qy + 1), (qx, qy.wrapping_sub(1))];
                for (nx, ny) in nb {
                    if nx >= C || ny >= C {
                        continue;
                    }
                    let r = ny * C + nx;
                    if !seen[r] && out.kind[r] == k {
                        seen[r] = true;
                        blob.push(r);
                        todo.push(r);
                    }
                }
            }
            let speck = blob.len() < 8
                || (blob.len() < 64
                    && blob.iter().all(|&q| off(&layer.rgb(q % C, q / C), field.get(q % C, q / C)) <= 32.0));
            if speck {
                for &q in &blob {
                    out.kind[q] = Kind::Tile;
                    out.value[q] = 0.0;
                }
            }
        }
    }
    out
}

/// A second smooth saturated surface over a quarter of the tile or more: a
/// split design (Kontrast), not art on a tile.
fn two_tone(layer: &Img, fit: &Fit, samples: &[Sample]) -> bool {
    let rest: Vec<Sample> = samples
        .iter()
        .copied()
        .filter(|s| classify(&s.c, &at(fit, s.x, s.y)).0 == Kind::Art)
        .collect();
    if rest.len() < 30 {
        return false;
    }
    let mut bins: HashMap<[i32; 3], Vec<Sample>> = HashMap::new();
    let mut order = Vec::new();
    for s in &rest {
        let key = s.c.map(|v| v as i32 / 12);
        let bin = bins.entry(key).or_default();
        if bin.is_empty() {
            order.push(key);
        }
        bin.push(*s);
    }
    let mut keep: Vec<Sample> = Vec::new();
    for key in &order {
        if bins[key].len() > keep.len() {
            keep = bins[key].clone();
        }
    }
    let mut f2 = None;
    for _ in 0..3 {
        if keep.len() < 12 {
            return false;
        }
        let Some(f) = surface(&keep, 6) else { return false };
        keep = rest.iter().copied().filter(|s| off(&s.c, &at(&f, s.x, s.y)) <= TOL).collect();
        f2 = Some(f);
    }
    let f2 = f2.unwrap();
    let mp = &layers().mask;
    let (mut n, mut m) = (0usize, 0usize);
    for y in (MARGIN..MARGIN + TILE).step_by(4) {
        for x in (MARGIN..MARGIN + TILE).step_by(4) {
            if mp.at(x, y) == 255 {
                n += 1;
                m += (off(&layer.rgb(x, y), &at(&f2, x as f64, y as f64)) <= TOL) as usize;
            }
        }
    }
    let c = at(&f2, C as f64 / 2.0, C as f64 / 2.0);
    m as f64 >= 0.25 * n as f64 && chroma(&c) >= 0.12 * 255.0
}

/// Share of the squircle read as art yet within 48 levels of the tile.
fn stray_tile(layer: &Img, field: &Field, kind: &Classes) -> f64 {
    let (mut n, mut m) = (0usize, 0usize);
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            let k = kind.kind[y * C + x];
            if k == Kind::None || layer.alpha(x, y) < 250 {
                continue;
            }
            n += 1;
            m += (k == Kind::Art && off(&layer.rgb(x, y), field.get(x, y)) <= 48.0) as usize;
        }
    }
    m as f64 / n.max(1) as f64
}

/// 1 for black, white and greys up to a chroma of 0.10, 0 from 0.22.
fn neutral(c: &Rgb) -> f64 {
    1.0 - ((chroma(c) / 255.0 - 0.10) / 0.12).clamp(0.0, 1.0)
}

/// The neutral art is ink and paper (engravings, white logos), not grey
/// material: under half of it is mid grey.
fn ink_and_paper(layer: &Img, kind: &Classes) -> bool {
    let (mut n, mut mid) = (0usize, 0usize);
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            if kind.kind[y * C + x] != Kind::Art || layer.alpha(x, y) < 250 {
                continue;
            }
            let c = layer.rgb(x, y);
            if neutral(&c) >= 1.0 {
                n += 1;
                let l = luma(&c);
                mid += (0.18..=0.70).contains(&l) as usize;
            }
        }
    }
    if n == 0 { true } else { (mid as f64) < 0.5 * n as f64 }
}

// colorsys, as Python has it.
fn rgb_to_hls(c: &Rgb) -> (f64, f64, f64) {
    let (r, g, b) = (c[0] / 255.0, c[1] / 255.0, c[2] / 255.0);
    let maxc = r.max(g).max(b);
    let minc = r.min(g).min(b);
    let sumc = maxc + minc;
    let rangec = maxc - minc;
    let l = sumc / 2.0;
    if minc == maxc {
        return (0.0, l, 0.0);
    }
    let s = if l <= 0.5 { rangec / sumc } else { rangec / (2.0 - sumc) };
    let (rc, gc, bc) = ((maxc - r) / rangec, (maxc - g) / rangec, (maxc - b) / rangec);
    let h = if r == maxc {
        bc - gc
    } else if g == maxc {
        2.0 + rc - bc
    } else {
        4.0 + gc - rc
    };
    ((h / 6.0).rem_euclid(1.0), l, s)
}

fn hls_value(m1: f64, m2: f64, hue: f64) -> f64 {
    let hue = hue.rem_euclid(1.0);
    if hue < 1.0 / 6.0 {
        m1 + (m2 - m1) * hue * 6.0
    } else if hue < 0.5 {
        m2
    } else if hue < 2.0 / 3.0 {
        m1 + (m2 - m1) * (2.0 / 3.0 - hue) * 6.0
    } else {
        m1
    }
}

fn hls_to_rgb(h: f64, l: f64, s: f64) -> Rgb {
    if s == 0.0 {
        return [l * 255.0; 3];
    }
    let m2 = if l <= 0.5 { l * (1.0 + s) } else { l + s - l * s };
    let m1 = 2.0 * l - m2;
    [hls_value(m1, m2, h + 1.0 / 3.0), hls_value(m1, m2, h), hls_value(m1, m2, h - 1.0 / 3.0)]
        .map(|v| v * 255.0)
}

fn hls(c: &Rgb, l: Option<f64>, dl: f64, ds: f64, dh: f64) -> Rgb {
    let (h, ll, s) = rgb_to_hls(c);
    let ll = l.unwrap_or(ll + dl);
    hls_to_rgb((h + dh).rem_euclid(1.0), ll.clamp(0.0, 1.0), (s + ds).clamp(0.0, 1.0)).map(f64::round)
}

/// The fitted tile spans several hues (a teal -> green ramp): a design of its
/// own, not one brand colour.
fn many_hued(fit: &Fit) -> bool {
    let mp = &layers().mask;
    let mut hs = Vec::new();
    for y in (MARGIN..MARGIN + TILE).step_by(16) {
        for x in (MARGIN..MARGIN + TILE).step_by(16) {
            let c = at(fit, x as f64, y as f64);
            if mp.at(x, y) == 255 && chroma(&c) >= 24.0 {
                hs.push(rgb_to_hls(&c).0); // hsv's hue is the same
            }
        }
    }
    if hs.is_empty() {
        return false;
    }
    let tau = 2.0 * std::f64::consts::PI;
    let n = hs.len() as f64;
    let sx = hs.iter().map(|h| (tau * h).cos()).sum::<f64>() / n;
    let sy = hs.iter().map(|h| (tau * h).sin()).sum::<f64>() / n;
    let r = sx.hypot(sy).clamp(1e-9, 1.0);
    (-2.0 * r.ln()).max(0.0).sqrt() > 0.25 // circular std, radians
}

pub struct Brand {
    field: Field,
    kind: Classes,
    b: Rgb,
    tint: bool,
    many: bool,
}

pub fn brand_tile(layer: &Img) -> Option<Brand> {
    let samples = band_samples(layer);
    if samples.len() < 100 {
        return None;
    }
    let fit = band_fit(&samples)?;
    if chroma(&at(&fit, C as f64 / 2.0, C as f64 / 2.0)) < 16.0 {
        return None; // neutral: backdrop()'s business
    }
    let not_art = samples.iter().filter(|s| classify(&s.c, &at(&fit, s.x, s.y)).0 != Kind::Art).count();
    if (not_art as f64) < 0.70 * samples.len() as f64 {
        return None; // not a tile along the edge: cover art, scenes
    }
    let fit = grow_fit(layer, fit);
    if two_tone(layer, &fit, &samples) {
        return None;
    }
    let field = Field::new(&fit);
    let kind = classes(layer, &field);
    if stray_tile(layer, &field, &kind) > 0.03 {
        return None;
    }
    let b = at(&fit, C as f64 / 2.0, C as f64 / 2.0).map(f64::round);
    let tint = ink_and_paper(layer, &kind);
    let many = many_hued(&fit);
    Some(Brand { field, kind, b, tint, many })
}

/// (ramp top, ramp bottom, ink, paper), the same in both themes.
fn brand_palette(b: &Rgb) -> (Rgb, Rgb, Rgb, Rgb) {
    (hls(b, None, 0.08, 0.04, -0.012), hls(b, None, -0.10, 0.0, 0.012), hls(b, Some(0.16), 0.0, 0.1, 0.0),
     LIGHT_TOP)
}

fn ink_map(ink: Rgb, paper: Rgb) -> impl Fn(&Rgb) -> Rgb {
    move |c: &Rgb| {
        let w = neutral(c);
        let t = ((luma(c) - 0.05) / 0.85).clamp(0.0, 1.0);
        [0, 1, 2].map(|i| c[i] + (ink[i] + (paper[i] - ink[i]) * t - c[i]) * w)
    }
}

/// The repainted tile, shade and glow pixels.
fn brand_ground(brand: &Brand, variant: Variant) -> Vec<Rgb> {
    let (top, bot, _, _) = brand_palette(&brand.b);
    let hl = if variant == Variant::Light { BRAND_HL_LIGHT } else { BRAND_HL_DARK };
    let rows = tile_rows(&top, &bot, hl, DARK_HL_SPAN);
    let mut res = vec![[0.0; 3]; C * C];
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            let i = y * C + x;
            let k = brand.kind.kind[i];
            if matches!(k, Kind::None | Kind::Art) {
                continue;
            }
            let t = if brand.many {
                let c = brand.field.get(x, y);
                let a = hl * (1.0 - ((y as f64 - MARGIN as f64) / TILE as f64).clamp(0.0, 1.0) / DARK_HL_SPAN)
                    .max(0.0);
                c.map(|u| u + (255.0 - u) * a)
            } else {
                rows[y]
            };
            let v = brand.kind.value[i];
            res[i] = match k {
                Kind::Shade => t.map(|u| u * v),
                Kind::Glow => t.map(|u| u + (255.0 - u) * v),
                _ => t,
            };
        }
    }
    res
}

/// The layer on its brand tile, its art mapped through `tone`.
fn rebrand(layer: &Img, brand: &Brand, ground: &[Rgb], tone: Option<&dyn Fn(&Rgb) -> Rgb>) -> Img {
    let tone_of = |c: &Rgb| tone.map_or(*c, |f| f(c));
    let kind = &brand.kind;
    let mut res = ground.to_vec();
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            let i = y * C + x;
            if kind.kind[i] != Kind::Art {
                continue;
            }
            let (xi, yi) = (x as isize, y as isize);
            let c = layer.rgb(x, y);
            let mut bg = Vec::new();
            for dx in -1..=1 {
                for dy in -1..=1 {
                    if kind.get(xi + dx, yi + dy, Kind::Art) != Kind::Art {
                        bg.push(((xi + dx) as usize, (yi + dy) as usize));
                    }
                }
            }
            if bg.is_empty() {
                res[i] = tone_of(&c); // inside the art: opaque, whatever its colour
                continue;
            }
            let n = bg.len() as f64;
            let mx = bg.iter().map(|q| q.0 as f64).sum::<f64>() / n;
            let my = bg.iter().map(|q| q.1 as f64).sum::<f64>() / n;
            let mut q = bg[0];
            let mut qd = f64::MAX;
            for &p in &bg {
                let d = (p.0 as f64 - mx).powi(2) + (p.1 as f64 - my).powi(2);
                if d < qd {
                    qd = d;
                    q = p;
                }
            }
            let pb = layer.rgb(q.0, q.1);
            let mut a_col = c;
            let mut a_dist = -1.0;
            for dx in -2..=2isize {
                for dy in -2..=2isize {
                    if kind.get(xi + dx, yi + dy, Kind::None) != Kind::Art {
                        continue;
                    }
                    let a = layer.rgb((xi + dx) as usize, (yi + dy) as usize);
                    let d: f64 = (0..3).map(|j| (a[j] - pb[j]).powi(2)).sum();
                    if d > a_dist {
                        a_dist = d;
                        a_col = a;
                    }
                }
            }
            let d = [0, 1, 2].map(|j| a_col[j] - pb[j]);
            let nn: f64 = d.iter().map(|t| t * t).sum();
            let al = if nn == 0.0 {
                1.0
            } else {
                ((0..3).map(|j| (c[j] - pb[j]) * d[j]).sum::<f64>() / nn).clamp(0.0, 1.0)
            };
            let ta = tone_of(&a_col);
            let rq = ground[q.1 * C + q.0];
            res[i] = [0, 1, 2].map(|j| al * ta[j] + (1.0 - al) * rq[j]);
        }
    }
    let mut out = layer.clone();
    for y in MARGIN..MARGIN + TILE {
        for x in MARGIN..MARGIN + TILE {
            if kind.kind[y * C + x] != Kind::None {
                out.set_rgb(x, y, &res[y * C + x]);
            }
        }
    }
    out
}

struct Step {
    p: usize,
    q: usize,
    d: i32,
}

fn luma_plane(im: &Img) -> Vec<i32> {
    (0..im.w * im.h)
        .map(|i| {
            let p = &im.p[i * 4..i * 4 + 3];
            (p[0] as i32 * 299 + p[1] as i32 * 587 + p[2] as i32 * 114 + 500) / 1000
        })
        .collect()
}

fn small(im: &Img) -> Img {
    resized(im, 128, 128)
}

/// Luma steps of 0.15 and up between pixels 2 px apart inside the squircle.
fn edge_steps(im: &Img) -> Vec<Step> {
    let l = luma_plane(im);
    let lay = layers();
    let mut out = Vec::new();
    for &(x, y) in &lay.pts128 {
        for (qx, qy) in [(x + 2, y), (x, y + 2)] {
            if qx < 128 && qy < 128 && lay.in128.at(qx, qy) == 255 {
                let d = (l[y * 128 + x] - l[qy * 128 + qx]).abs();
                if d >= 38 {
                    out.push(Step { p: y * 128 + x, q: qy * 128 + qx, d });
                }
            }
        }
    }
    out
}

/// The outlines that made the icon read still show.
fn legible(r: &Img, steps: &[Step], art: &[bool]) -> bool {
    if steps.len() < 40 {
        return true;
    }
    let l = luma_plane(r);
    let (mut kept, mut inner, mut inner_kept) = (0usize, 0usize, 0usize);
    for s in steps {
        let ok = ((l[s.p] - l[s.q]).abs() as f64) >= 20f64.max(s.d as f64 / 3.0);
        kept += ok as usize;
        if art[s.p] && art[s.q] {
            inner += 1;
            inner_kept += ok as usize;
        }
    }
    kept as f64 >= 0.6 * steps.len() as f64
        || (inner as f64 >= 0.25 * steps.len() as f64 && inner_kept as f64 >= 0.8 * inner as f64)
}

/// The icon on its brand tile for one theme, or None when its art would not
/// read there.
pub fn brand_variant(layer: &Img, brand: &Brand, variant: Variant) -> Option<Img> {
    let (_, _, ink, paper) = brand_palette(&brand.b);
    let ground = brand_ground(brand, variant);
    let mut art = vec![false; 128 * 128];
    for &(x, y) in &layers().pts128 {
        let mut sum = 0u32;
        for dy in 0..4 {
            for dx in 0..4 {
                sum += if brand.kind.kind[(y * 4 + dy) * C + x * 4 + dx] == Kind::Art { 255 } else { 0 };
            }
        }
        art[y * 128 + x] = (sum + 8) / 16 >= 128;
    }
    let steps = edge_steps(&small(layer));
    let mut out = rebrand(layer, brand, &ground, None);
    if brand.tint {
        let map = ink_map(ink, paper);
        let cand = rebrand(layer, brand, &ground, Some(&map));
        if legible(&small(&cand), &steps, &art) {
            out = cand;
        }
    }
    if !legible(&small(&out), &steps, &art) {
        return None;
    }
    let mut result = composited(&layers().shadow, &out);
    alpha_composite(&mut result, &layers().brand_edge[variant as usize], 0, 0);
    Some(result)
}
