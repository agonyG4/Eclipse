//! Raster primitives with Pillow's semantics: straight (non-premultiplied)
//! RGBA, aliased rounded rectangles, Gaussian blur, "over" compositing and a
//! Lanczos-3 resampler that works premultiplied, as Pillow's resize does.

pub type Rgb = [f64; 3];

pub fn clamp255(v: f64) -> f64 {
    v.clamp(0.0, 255.0)
}

pub fn to_byte(v: f64) -> u8 {
    clamp255(v).round() as u8
}

pub fn luma(c: &Rgb) -> f64 {
    (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]) / 255.0
}

pub fn chroma(c: &Rgb) -> f64 {
    c[0].max(c[1]).max(c[2]) - c[0].min(c[1]).min(c[2])
}

#[derive(Clone, Default)]
pub struct Img {
    pub w: usize,
    pub h: usize,
    pub p: Vec<u8>,
}

impl Img {
    pub fn new(w: usize, h: usize) -> Self {
        Self { w, h, p: vec![0; w * h * 4] }
    }

    pub fn is_empty(&self) -> bool {
        self.w == 0 || self.h == 0
    }

    #[inline]
    pub fn idx(&self, x: usize, y: usize) -> usize {
        (y * self.w + x) * 4
    }

    #[inline]
    pub fn rgb(&self, x: usize, y: usize) -> Rgb {
        let i = self.idx(x, y);
        [self.p[i] as f64, self.p[i + 1] as f64, self.p[i + 2] as f64]
    }

    #[inline]
    pub fn alpha(&self, x: usize, y: usize) -> u8 {
        self.p[self.idx(x, y) + 3]
    }

    #[inline]
    pub fn set_rgb(&mut self, x: usize, y: usize, c: &Rgb) {
        let i = self.idx(x, y);
        self.p[i] = to_byte(c[0]);
        self.p[i + 1] = to_byte(c[1]);
        self.p[i + 2] = to_byte(c[2]);
    }

    pub fn cropped(&self, x0: isize, y0: isize, x1: isize, y1: isize) -> Img {
        let w = (x1 - x0).max(0) as usize;
        let h = (y1 - y0).max(0) as usize;
        let mut out = Img::new(w, h);
        for y in 0..h {
            let sy = y as isize + y0;
            if sy < 0 || sy >= self.h as isize {
                continue;
            }
            for x in 0..w {
                let sx = x as isize + x0;
                if sx < 0 || sx >= self.w as isize {
                    continue;
                }
                let s = self.idx(sx as usize, sy as usize);
                let d = out.idx(x, y);
                out.p[d..d + 4].copy_from_slice(&self.p[s..s + 4]);
            }
        }
        out
    }

    /// Crop to the bounding box of non-zero alpha (Pillow's getbbox on RGBA).
    pub fn alpha_bbox_crop(&self) -> Img {
        let (mut x0, mut y0, mut x1, mut y1) = (self.w, self.h, 0usize, 0usize);
        let mut any = false;
        for y in 0..self.h {
            for x in 0..self.w {
                if self.alpha(x, y) != 0 {
                    any = true;
                    x0 = x0.min(x);
                    y0 = y0.min(y);
                    x1 = x1.max(x);
                    y1 = y1.max(y);
                }
            }
        }
        if !any {
            return self.clone();
        }
        self.cropped(x0 as isize, y0 as isize, x1 as isize + 1, y1 as isize + 1)
    }
}

/// One 8-bit channel (Pillow's mode 'L').
#[derive(Clone, Default)]
pub struct Plane {
    pub w: usize,
    pub h: usize,
    pub p: Vec<u8>,
}

impl Plane {
    pub fn new(w: usize, h: usize) -> Self {
        Self { w, h, p: vec![0; w * h] }
    }

    #[inline]
    pub fn at(&self, x: usize, y: usize) -> u8 {
        self.p[y * self.w + x]
    }

    pub fn subtract(&self, other: &Plane) -> Plane {
        let mut out = Plane::new(self.w, self.h);
        for (o, (a, b)) in out.p.iter_mut().zip(self.p.iter().zip(&other.p)) {
            *o = a.saturating_sub(*b);
        }
        out
    }

    pub fn blurred(&self, sigma: f64) -> Plane {
        let mut v: Vec<f64> = self.p.iter().map(|&b| b as f64).collect();
        blur_plane(&mut v, self.w, self.h, sigma);
        Plane { w: self.w, h: self.h, p: v.into_iter().map(to_byte).collect() }
    }
}

/// Pillow's filled rounded_rectangle: inclusive pixel bounds, aliased.
pub fn rounded_rect(w: usize, h: usize, x0: f64, y0: f64, x1: f64, y1: f64, r: f64) -> Plane {
    let mut out = Plane::new(w, h);
    let rr = (r + 0.25) * (r + 0.25);
    for y in 0..h {
        let fy = y as f64;
        if fy < y0 || fy > y1 {
            continue;
        }
        for x in 0..w {
            let fx = x as f64;
            if fx < x0 || fx > x1 {
                continue;
            }
            let dx = if fx < x0 + r { x0 + r - fx } else if fx > x1 - r { fx - (x1 - r) } else { 0.0 };
            let dy = if fy < y0 + r { y0 + r - fy } else if fy > y1 - r { fy - (y1 - r) } else { 0.0 };
            if dx * dx + dy * dy <= rr {
                out.p[y * w + x] = 255;
            }
        }
    }
    out
}

pub fn blur_plane(v: &mut [f64], w: usize, h: usize, sigma: f64) {
    if sigma <= 0.0 {
        return;
    }
    let radius = (sigma * 3.0).ceil() as isize;
    let mut kernel: Vec<f64> = (-radius..=radius)
        .map(|i| (-((i * i) as f64) / (2.0 * sigma * sigma)).exp())
        .collect();
    let sum: f64 = kernel.iter().sum();
    kernel.iter_mut().for_each(|k| *k /= sum);
    let mut tmp = vec![0.0; v.len()];
    for y in 0..h {
        for x in 0..w {
            let mut acc = 0.0;
            for (k, i) in kernel.iter().zip(-radius..=radius) {
                let sx = (x as isize + i).clamp(0, w as isize - 1) as usize;
                acc += v[y * w + sx] * k;
            }
            tmp[y * w + x] = acc;
        }
    }
    for y in 0..h {
        for x in 0..w {
            let mut acc = 0.0;
            for (k, i) in kernel.iter().zip(-radius..=radius) {
                let sy = (y as isize + i).clamp(0, h as isize - 1) as usize;
                acc += tmp[sy * w + x] * k;
            }
            v[y * w + x] = acc;
        }
    }
}

pub fn blur_rgb(img: &mut Img, sigma: f64) {
    for c in 0..3 {
        let mut v: Vec<f64> = (0..img.w * img.h).map(|i| img.p[i * 4 + c] as f64).collect();
        blur_plane(&mut v, img.w, img.h, sigma);
        for (i, val) in v.into_iter().enumerate() {
            img.p[i * 4 + c] = to_byte(val);
        }
    }
}

/// Porter-Duff "over" of `src` placed at (ox, oy) onto `dst`, straight alpha.
pub fn alpha_composite(dst: &mut Img, src: &Img, ox: isize, oy: isize) {
    for y in 0..src.h {
        let dy = y as isize + oy;
        if dy < 0 || dy >= dst.h as isize {
            continue;
        }
        for x in 0..src.w {
            let dx = x as isize + ox;
            if dx < 0 || dx >= dst.w as isize {
                continue;
            }
            let si = src.idx(x, y);
            let sa8 = src.p[si + 3];
            if sa8 == 0 {
                continue;
            }
            let di = dst.idx(dx as usize, dy as usize);
            let sa = sa8 as f64 / 255.0;
            let da = dst.p[di + 3] as f64 / 255.0;
            let oa = sa + da * (1.0 - sa);
            for c in 0..3 {
                dst.p[di + c] =
                    to_byte((src.p[si + c] as f64 * sa + dst.p[di + c] as f64 * da * (1.0 - sa)) / oa);
            }
            dst.p[di + 3] = to_byte(oa * 255.0);
        }
    }
}

pub fn composited(bottom: &Img, top: &Img) -> Img {
    let mut out = bottom.clone();
    alpha_composite(&mut out, top, 0, 0);
    out
}

fn sinc(x: f64) -> f64 {
    if x == 0.0 {
        1.0
    } else {
        let px = std::f64::consts::PI * x;
        px.sin() / px
    }
}

fn lanczos3(x: f64) -> f64 {
    if x.abs() < 3.0 { sinc(x) * sinc(x / 3.0) } else { 0.0 }
}

/// Per output sample: the first source index and its normalised weights.
fn coefficients(src: usize, dst: usize) -> Vec<(usize, Vec<f64>)> {
    let scale = src as f64 / dst as f64;
    let filter_scale = scale.max(1.0);
    let support = 3.0 * filter_scale;
    (0..dst)
        .map(|i| {
            let center = (i as f64 + 0.5) * scale;
            let lo = ((center - support).floor().max(0.0)) as usize;
            let hi = ((center + support).ceil() as usize).min(src);
            let mut w: Vec<f64> =
                (lo..hi).map(|j| lanczos3((j as f64 + 0.5 - center) / filter_scale)).collect();
            let sum: f64 = w.iter().sum();
            if sum != 0.0 {
                w.iter_mut().for_each(|v| *v /= sum);
            }
            (lo, w)
        })
        .collect()
}

/// Lanczos-3 resample through premultiplied alpha (Pillow's LANCZOS).
pub fn resized(src: &Img, w: usize, h: usize) -> Img {
    if src.w == w && src.h == h {
        return src.clone();
    }
    let pre: Vec<[f64; 4]> = (0..src.w * src.h)
        .map(|i| {
            let a = src.p[i * 4 + 3] as f64 / 255.0;
            [src.p[i * 4] as f64 * a, src.p[i * 4 + 1] as f64 * a, src.p[i * 4 + 2] as f64 * a,
             src.p[i * 4 + 3] as f64]
        })
        .collect();
    let cx = coefficients(src.w, w);
    let mut horiz = vec![[0.0f64; 4]; w * src.h];
    for y in 0..src.h {
        for (x, (lo, ws)) in cx.iter().enumerate() {
            let mut acc = [0.0; 4];
            for (k, wt) in ws.iter().enumerate() {
                let s = &pre[y * src.w + lo + k];
                for c in 0..4 {
                    acc[c] += s[c] * wt;
                }
            }
            horiz[y * w + x] = acc;
        }
    }
    let cy = coefficients(src.h, h);
    let mut out = Img::new(w, h);
    for (y, (lo, ws)) in cy.iter().enumerate() {
        for x in 0..w {
            let mut acc = [0.0; 4];
            for (k, wt) in ws.iter().enumerate() {
                let s = &horiz[(lo + k) * w + x];
                for c in 0..4 {
                    acc[c] += s[c] * wt;
                }
            }
            let a = clamp255(acc[3]);
            let i = out.idx(x, y);
            if a > 0.0 {
                let f = 255.0 / a;
                for c in 0..3 {
                    out.p[i + c] = to_byte(acc[c] * f);
                }
            }
            out.p[i + 3] = to_byte(a);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resize_keeps_flat_colour() {
        let mut img = Img::new(64, 64);
        for px in img.p.chunks_mut(4) {
            px.copy_from_slice(&[200, 100, 50, 255]);
        }
        let out = resized(&img, 40, 40);
        assert_eq!(out.rgb(20, 20), [200.0, 100.0, 50.0]);
        assert_eq!(out.alpha(0, 0), 255);
    }

    #[test]
    fn composite_over_transparent_keeps_source() {
        let mut dst = Img::new(1, 1);
        let mut src = Img::new(1, 1);
        src.p.copy_from_slice(&[10, 20, 30, 128]);
        alpha_composite(&mut dst, &src, 0, 0);
        assert_eq!(dst.p, vec![10, 20, 30, 128]);
    }
}
