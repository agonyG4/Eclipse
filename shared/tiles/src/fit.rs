//! Least-squares quadratic / cubic colour surfaces over the canvas: a tile's
//! own gradient, in any direction.

use crate::look::{C, MARGIN, TILE, layers};
use crate::raster::{Img, Rgb, clamp255};

#[derive(Clone, Copy, Debug)]
pub struct Fit {
    pub n: usize,
    pub k: [[f64; 10]; 3],
}

#[derive(Clone, Copy, Debug)]
pub struct Sample {
    pub x: f64,
    pub y: f64,
    pub c: Rgb,
}

/// Quadratic terms first, then the cubic ones: a fit keeps its own count.
pub fn terms(x: f64, y: f64) -> [f64; 10] {
    let h = C as f64 / 2.0;
    let (u, v) = ((x - h) / h, (y - h) / h);
    [1.0, u, v, u * u, v * v, u * v, u * u * u, v * v * v, u * u * v, u * v * v]
}

pub fn at(fit: &Fit, x: f64, y: f64) -> Rgb {
    let t = terms(x, y);
    let mut out = [0.0; 3];
    for c in 0..3 {
        let acc: f64 = (0..fit.n).map(|i| fit.k[c][i] * t[i]).sum();
        out[c] = clamp255(acc);
    }
    out
}

pub fn off(a: &Rgb, b: &Rgb) -> f64 {
    (a[0] - b[0]).abs().max((a[1] - b[1]).abs()).max((a[2] - b[2]).abs())
}

/// Gauss-Jordan with partial pivoting; None on a singular system.
fn solve(mut m: [[f64; 11]; 10], n: usize) -> Option<[f64; 10]> {
    for c in 0..n {
        let p = (c..n).max_by(|&a, &b| m[a][c].abs().total_cmp(&m[b][c].abs()))?;
        m.swap(c, p);
        if m[c][c].abs() < 1e-12 {
            return None;
        }
        for r in 0..n {
            if r != c {
                let k = m[r][c] / m[c][c];
                for j in 0..=n {
                    m[r][j] -= k * m[c][j];
                }
            }
        }
    }
    let mut x = [0.0; 10];
    for i in 0..n {
        x[i] = m[i][n] / m[i][i];
    }
    Some(x)
}

pub fn surface(pts: &[Sample], n: usize) -> Option<Fit> {
    let mut a = [[0.0; 10]; 10];
    let mut b = [[0.0; 10]; 3];
    for s in pts {
        let t = terms(s.x, s.y);
        for i in 0..n {
            for j in 0..n {
                a[i][j] += t[i] * t[j];
            }
            for k in 0..3 {
                b[k][i] += t[i] * s.c[k];
            }
        }
    }
    let mut fit = Fit { n, k: [[0.0; 10]; 3] };
    for k in 0..3 {
        let mut m = [[0.0; 11]; 10];
        for i in 0..n {
            m[i][..n].copy_from_slice(&a[i][..n]);
            m[i][n] = b[k][i];
        }
        fit.k[k] = solve(m, n)?;
    }
    Some(fit)
}

/// Every second pixel of the band just inside the squircle edge that is opaque.
pub fn band_samples(layer: &Img) -> Vec<Sample> {
    let rp = &layers().rim_band_mask;
    let mut out = Vec::new();
    for y in (MARGIN..MARGIN + TILE).step_by(2) {
        for x in (MARGIN..MARGIN + TILE).step_by(2) {
            if rp.at(x, y) >= 200 && layer.alpha(x, y) >= 250 {
                out.push(Sample { x: x as f64, y: y as f64, c: layer.rgb(x, y) });
            }
        }
    }
    out
}

/// The artwork's own tile as a smooth surface, fitted to the edge band and
/// refitted without the art it caught there. None unless nearly all of the
/// band sits on the fit.
pub fn backdrop(layer: &Img) -> Option<Fit> {
    let samples = band_samples(layer);
    if samples.len() < 100 {
        return None;
    }
    let mut keep = samples.clone();
    let mut fit = None;
    for _ in 0..2 {
        let f = surface(&keep, 6)?;
        keep = samples.iter().copied().filter(|s| off(&s.c, &at(&f, s.x, s.y)) <= 10.0).collect();
        if (keep.len() as f64) < 0.85 * samples.len() as f64 {
            return None;
        }
        fit = Some(f);
    }
    fit
}

/// A fit evaluated on every canvas pixel.
pub struct Field(Vec<Rgb>);

impl Field {
    pub fn new(fit: &Fit) -> Self {
        let mut v = vec![[0.0; 3]; C * C];
        for y in MARGIN..MARGIN + TILE {
            for x in MARGIN..MARGIN + TILE {
                v[y * C + x] = at(fit, x as f64, y as f64);
            }
        }
        Self(v)
    }

    #[inline]
    pub fn get(&self, x: usize, y: usize) -> &Rgb {
        &self.0[y * C + x]
    }
}
