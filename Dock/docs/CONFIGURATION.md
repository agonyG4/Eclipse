# Dock Configuration

The Dock reads `~/.config/AstreaOS/dock.json` and the shared component toggle
from `~/.config/AstreaOS/ui/components.json`:

```json
{
  "iconSize": 48,
  "edgeMargin": 12,
  "panelPadding": 14,
  "itemSpacing": 10,
  "hoverEffect": "magnification",
  "magnificationScale": 1.6,
  "magnificationRadius": 2.5,
  "position": "bottom",
  "floating": true,
  "cornerRadius": 23,
  "autoHide": "never",
  "indicatorStyle": "line",
  "indicatorSize": 3,
  "animationsEnabled": true,
  "animationSpeed": 1.0,
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
`iconSize` 48, `edgeMargin` 12, `panelPadding` 14, `itemSpacing` 10,
`hoverEffect` `magnification`, `magnificationScale` 1.6,
`magnificationRadius` 2.5, `position` `bottom`, `floating` true,
`cornerRadius` 23, `autoHide` `never`, `indicatorStyle` `line`,
`indicatorSize` 3, `animationsEnabled` true, `animationSpeed` 1.0, and no
pins.

With no configured pins, the Dock remains unmapped and reserves no Layer Shell
exclusive zone. This is the current Stage 1 startup behavior. A future AstreaOS
installation decision must either provide image-level default configuration or
define a small built-in first-run pin set; this implementation deliberately
does neither.

Validation is field-local. `iconSize` is clamped to `32..64`,
`edgeMargin` to `0..48`, `panelPadding` to `8..32`, and `itemSpacing` to
`4..24`. `hoverEffect` must be one of `none`, `lift`, or `magnification`; an
invalid value falls back to `magnification`. `none` disables hover animation,
`lift` is the lightweight Eclipse effect (the directly hovered icon scales to
about `1.1` and moves upward), and `magnification` enables the continuous
neighborhood effect. `magnificationScale` is clamped to `1.0..2.0`, and
`magnificationRadius` is clamped to `1.0..4.0`. `position` accepts only
`bottom`, `left`, or `right`; top is intentionally not supported in v1.
`floating` is boolean, `cornerRadius` is clamped to `0..48`, `autoHide` accepts
`never`, `intelligent`, or `always`, `indicatorStyle` accepts `line`, `dot`, or
`none`, `indicatorSize` is clamped to `1..12`, and `animationSpeed` is clamped
to `0.25..4.0`. Magnification radius is measured approximately in icon-slot
radii, not as a fixed number of neighboring icons. Numeric values must be
finite. For backward compatibility, a legacy
`magnificationEnabled` value is read only when `hoverEffect` is absent: `false`
maps to `none` and `true` maps to `magnification`. `pins` must be an array of
strings. Each pin must be a basename ending in `.desktop` with no slash,
backslash, NUL, or `..`; invalid entries are rejected while valid entries
remain. Duplicate pins collapse by first occurrence. Files larger than 1 MiB
and pin arrays larger than 256 entries are rejected with defaults for that
field/file and a diagnostic error.

Hidden and `NoDisplay=true` desktop entries remain valid explicit pins, but are
not selected by normal catalog identity resolution. Unresolved explicit pins
remain visible and use their normalized filename as fallback display text.

Magnification is a continuous neighborhood effect, not a discrete hovered-item
switch. The configured radius is converted from slot radii to pixels; icons
inside it receive a raised-cosine influence and icons outside it remain at
scale `1.0`. The Dock computes translations from cumulative extra widths while
leaving the resting Row unchanged. The transparent Layer Shell surface reserves
a conservative maximum horizontal neighborhood and maximum vertical
magnification/lift/drag headroom when structural state changes; it does not
resize per pointer frame. The centered visual chrome remains at resting height
and animates only its explicit width. Its Qt input region includes the actual
chrome rectangle and current transformed icon interaction rectangles, so unused
transparent envelope space does not remain an input surface.

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

`bottomMargin` is a legacy read compatibility key. If `edgeMargin` is absent,
the parser reads `bottomMargin`; if both are present, `edgeMargin` wins. Settings
writes emit canonical `edgeMargin` and remove only the legacy alias. A Settings
personalization write preserves the raw `pins` array and all unknown keys; pin
reorder/pin actions remain the only operations that intentionally replace
`pins`.

Dock presentation QML never writes `dock.json`. A successful configured-pin
reorder, or an explicit Dock pin/unpin action, is committed by `DockController`
through `DockConfigPersistence`, which reads the latest object, changes only
`pins`, preserves known and unknown keys, validates the complete new list, and
atomically replaces the file with `QSaveFile`. A malformed existing file is
left untouched; a missing file is created as the minimal valid object
containing `pins`. Runtime-only application order remains in-memory and is
never persisted. Reordering is pinned-only, uses `desktopFileName` identity,
and applies to the model only after persistence succeeds. Settings writes are
debounced and flush through the same atomic shared store. Changing any
personalization value through the watcher updates the existing Dock surface
without restarting the unified Shell.

`floating=true` applies `edgeMargin` to the selected edge. `floating=false`
uses an effective margin of zero while retaining the configured `edgeMargin` in
memory and on disk, so re-enabling floating restores the previous distance.

`autoHide=never` keeps the Dock revealed and reserves its resting cross-axis
thickness. `always` keeps the mapped surface alive, reserves no exclusive zone,
collapses after a 180 ms leave delay, and retains a bounded edge reveal target.
`intelligent` behaves as Never unless the published Typhon snapshot contains
an active maximized or fullscreen toplevel, in which case it behaves as Always.
This v1 obstruction heuristic does not inspect arbitrary overlap geometry;
future Typhon output/geometry publication can replace it.

`indicatorStyle=line` retains the current line indicator, `dot` uses a compact
circle, and `none` hides the indicator. `indicatorSize` is applied to the
configured indicator presentation; vertical Docks place it on the screen-edge
side of the icon. `animationsEnabled=false` makes configured Dock transitions
immediate. With animations enabled, `animationSpeed=1.0` retains current
timing, higher values are faster, and lower values are slower.
