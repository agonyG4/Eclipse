//! App artwork SVGs at master resolution, with resvg: complete SVG 1.1
//! (clip paths, masks, filters), where Qt's SVG Tiny reader drops parts.

use resvg::{tiny_skia, usvg};

/// `data` (SVG or SVGZ) scaled to fit an `extent` square, centred on it, as
/// straight RGBA. None when it does not parse or has no size.
pub fn rasterize(data: &[u8], extent: usize) -> Option<Vec<u8>> {
    let tree = usvg::Tree::from_data(data, &usvg::Options::default()).ok()?;
    let size = tree.size();
    let (w, h) = (size.width() as f64, size.height() as f64);
    if !(w > 0.0 && h > 0.0) {
        return None;
    }
    let k = (extent as f64 / w).min(extent as f64 / h);
    let (dx, dy) = ((extent as f64 - w * k) / 2.0, (extent as f64 - h * k) / 2.0);
    let mut pixmap = tiny_skia::Pixmap::new(extent as u32, extent as u32)?;
    let transform = tiny_skia::Transform::from_row(k as f32, 0.0, 0.0, k as f32, dx as f32, dy as f32);
    resvg::render(&tree, transform, &mut pixmap.as_mut());
    // tiny-skia is premultiplied; the tile pass works on straight alpha
    Some(
        pixmap
            .pixels()
            .iter()
            .flat_map(|p| {
                let c = p.demultiply();
                [c.red(), c.green(), c.blue(), c.alpha()]
            })
            .collect(),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clip_path_is_honoured() {
        // a full square clipped to its left half
        let svg = br#"<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">
            <defs><clipPath id="c"><rect width="5" height="10"/></clipPath></defs>
            <rect width="10" height="10" fill="red" clip-path="url(#c)"/></svg>"#;
        let px = rasterize(svg, 100).unwrap();
        assert_eq!(px[(50 * 100 + 20) * 4 + 3], 255);
        assert_eq!(px[(50 * 100 + 80) * 4 + 3], 0);
    }
}
