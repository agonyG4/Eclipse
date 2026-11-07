//! Dev tool: `render_raw <in.rgba> <out-prefix> [keep-dark]`, where in.rgba is
//! u32 LE width, u32 LE height, then straight RGBA8. Writes
//! <out-prefix>-light.rgba and <out-prefix>-dark.rgba (512², RGBA8).
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let data = std::fs::read(&args[1]).expect("read input");
    let w = u32::from_le_bytes(data[0..4].try_into().unwrap());
    let h = u32::from_le_bytes(data[4..8].try_into().unwrap());
    let n = 512 * 512 * 4;
    let (mut light, mut dark) = (vec![0u8; n], vec![0u8; n]);
    let keep = args.get(3).is_some() as i32;
    if std::env::var_os("TILES_WARM").is_some() {
        let (mut a, mut b) = (vec![0u8; n], vec![0u8; n]);
        unsafe { astrea_app_tiles::astrea_app_tiles_render(data[8..].as_ptr(), w, h, w * 4, keep, a.as_mut_ptr(), b.as_mut_ptr()) };
    }
    let t = std::time::Instant::now();
    let ok = unsafe {
        astrea_app_tiles::astrea_app_tiles_render(
            data[8..].as_ptr(), w, h, w * 4, keep, light.as_mut_ptr(), dark.as_mut_ptr())
    };
    eprintln!("{} ms", t.elapsed().as_millis());
    assert_eq!(ok, 1, "render failed");
    std::fs::write(format!("{}-light.rgba", args[2]), light).unwrap();
    std::fs::write(format!("{}-dark.rgba", args[2]), dark).unwrap();
}
