//! Dev tool: `style_raw <in.rgba> <out-prefix> [tint-hex]`; in.rgba is u32 LE
//! width, u32 LE height (both 512), then straight RGBA8. Writes
//! <out-prefix>-{dark,clear-light,clear-dark,tinted-light,tinted-dark}.rgba.
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let data = std::fs::read(&args[1]).expect("read input");
    let tint = args.get(3).map(|t| u32::from_str_radix(t.trim_start_matches('#'), 16).unwrap()).unwrap_or(0x0a84ff);
    let n = 512 * 512 * 4;
    for (name, style, dark) in [("dark", 1, 1), ("clear-light", 2, 0), ("clear-dark", 2, 1),
                                ("tinted-light", 3, 0), ("tinted-dark", 3, 1)] {
        let mut out = vec![0u8; n];
        let t = std::time::Instant::now();
        let ok = unsafe {
            astrea_app_tiles::astrea_app_tiles_style(data[8..].as_ptr(), 512, 512, 2048, style, dark, tint, out.as_mut_ptr())
        };
        assert_eq!(ok, 1);
        eprintln!("{name}: {} ms", t.elapsed().as_millis());
        std::fs::write(format!("{}-{name}.rgba", args[2]), out).unwrap();
    }
}
