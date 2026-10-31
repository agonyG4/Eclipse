# Eclipse Settings icon rendering

## Default pipeline for monochrome interface SVGs

Use `qml/components/controls/ThemedIcon.qml` (`Controls.ThemedIcon`) for
navigation icons, device category symbols, audio glyphs, and other monochrome
interface symbols. The component:

- Resolves Lucide, optional user-installed AppleOS SF Symbols, and configured
  Settings icon overrides through `SettingsController`.
- Tints the original vector source with `tintedSymbolSource()` (or
  `tintedSymbolUrl()` when an icon key is supplied) **before rasterization**.
- Preserves path geometry, SVG paint opacity, and aspect ratio. A small optical
  margin is applied to the presentation copy of SVG view boxes; source files
  are not rewritten.
- Requests physical image pixels based on the item's logical size and display
  DPR. It does not add GPU `ColorOverlay` layers or mipmap downsampling.
- Keeps color alpha via `Image.opacity`, without adding an offscreen pass.
- Caches tinted SVG URL results by resolved path, RGB color, file size, and
  modification time, with a bounded cache.

Example:

```qml
Controls.ThemedIcon {
    width: 24
    height: 24
    iconKey: "bluetooth"
    tint: Components.Theme.textSecondary
}
```

For a pre-resolved Settings icon URL, use `iconSource` instead of `iconKey`.
Use `ready` for fallback visibility and `imageSource` when testing the final
vector payload. `DeviceSymbol.qml` remains a compatibility facade over
`ThemedIcon`, with a fallback glyph.

## Scope and exceptions

Do **not** use this monochrome pipeline for application icons, avatars,
wallpaper thumbnails, multicolor assets, or material/background effects.
Application icons retain the existing shared `AstreaAppIcon` path used by Dock,
which handles variable icon themes, alpha masks, magnification, and its own
DPI-aware texture requests.

Avoid introducing `Image + Legacy.ColorOverlay` for Settings monochrome
SVGs. It incurs an extra offscreen texture path and can blur or clip
small/high-DPI symbols. Do not bundle Apple's reference symbols into the
open-source repository; AppleOS remains a user-local theme.
