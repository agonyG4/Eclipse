# Dock Configuration

The Dock reads `~/.config/AstreaOS/dock.json` and the shared component toggle
from `~/.config/AstreaOS/ui/components.json`:

```json
{
  "iconSize": 48,
  "bottomMargin": 12,
  "panelPadding": 14,
  "itemSpacing": 10,
  "magnificationEnabled": true,
  "magnificationScale": 1.6,
  "magnificationRadius": 2.5,
  "pins": [
    "astrea-explorer.desktop",
    "firefox.desktop",
    "kitty.desktop",
    "code.desktop",
    "spotify.desktop"
  ]
}
```

Defaults are deterministic and held in memory when `dock.json` is missing:
`iconSize` 48, `bottomMargin` 12, `panelPadding` 14, `itemSpacing` 10,
`magnificationEnabled` true, `magnificationScale` 1.6,
`magnificationRadius` 2.5, and no pins.

With no configured pins, the Dock remains unmapped and reserves no Layer Shell
exclusive zone. This is the current Stage 1 startup behavior. A future AstreaOS
installation decision must either provide image-level default configuration or
define a small built-in first-run pin set; this implementation deliberately
does neither.

Validation is field-local. `iconSize` is clamped to `32..64`,
`bottomMargin` to `0..48`, `panelPadding` to `8..32`, and `itemSpacing` to
`4..24`. `magnificationEnabled` must be boolean, `magnificationScale` is
clamped to `1.0..2.0`, and `magnificationRadius` is clamped to `1.0..4.0`.
Magnification radius is measured approximately in icon-slot radii, not as a
fixed number of neighboring icons. Numeric values must be finite. `pins` must
be an array of strings. Each pin must be a basename ending in `.desktop` with
no slash, backslash, NUL, or `..`; invalid entries are rejected while valid
entries remain. Duplicate pins collapse by first occurrence. Files larger than
1 MiB and pin arrays larger than 256 entries are rejected with defaults for that
field/file and a diagnostic error.

Hidden and `NoDisplay=true` desktop entries remain valid explicit pins, but are
not selected by normal catalog identity resolution. Unresolved explicit pins
remain visible and use their normalized filename as fallback display text.

Set the shared component key to disable or re-enable the Dock without
restarting:

```json
{
  "dock": false
}
```

The watcher monitors both files and their parent directories, so atomic
replacement and file recreation are supported. `astrea-dock --reload` forces
the same reload path.

Dock presentation QML never writes `dock.json`. A successful configured-pin
reorder, or an explicit Dock pin/unpin action, is committed by `DockController`
through `DockConfigPersistence`, which reads the latest
object, changes only `pins`, preserves known and unknown keys, validates the
complete new list, and atomically replaces the file with `QSaveFile`. A
malformed existing file is left untouched; a missing file is created as the
minimal valid object containing `pins`. Runtime-only application order remains
in-memory and is never persisted.
