# Dock Personalization v1 Design

## Goal

Expose the Dock's supported personalization controls in native Astrea Settings and
apply them to the resident Dock through the existing `dock.json` watcher path,
without adding a Settings-to-Shell protocol or weakening the current Dock
geometry, input-region, reorder, activation, icon-quality, or Layer Shell
reservation invariants.

## Shared contract

`shared/dock/` becomes the compositor-independent boundary for the Dock contract:

- `DockConfig` owns defaults, the canonical field names, and the legacy
  `bottomMargin` compatibility read.
- `DockConfigCodec` reads bounded JSON objects, validates/clamps each field,
  preserves the existing pin filtering rules, and patches canonical known fields
  into an existing object while leaving unknown keys intact.
- `DockConfigStore` performs the read/patch/serialize/`QSaveFile` commit used by
  both resident Dock pin persistence and Settings personalization writes.

The existing Dock service headers remain compatibility-facing wrappers where
needed by current production and tests, but parsing and atomic writing have one
implementation. Settings links only to this shared boundary and Qt Core; it does
not link Dock runtime, LayerShellQt, Typhon, or Shell targets.

Canonical persisted fields are `iconSize`, `panelPadding`, `itemSpacing`,
`hoverEffect`, `magnificationScale`, `magnificationRadius`, `edgeMargin`,
`position`, `floating`, `cornerRadius`, `autoHide`, `indicatorStyle`,
`indicatorSize`, and `animationsEnabled`/`animationSpeed`, plus `pins`.
`bottomMargin` is read only when `edgeMargin` is absent. A Settings write emits
`edgeMargin` and removes only the obsolete `bottomMargin` key; all other unknown
keys and the exact pin order are preserved. Syntax errors, oversized files, and
write failures leave the previous file unchanged.

## Runtime flow

`DockConfigWatcher` consumes the shared codec and continues to publish config
changes to `DockController`. The controller owns effective floating margin,
orientation, indicator/animation properties, and auto-hide state. It retains the
manual `show`/`hide` mapping override separately from `revealed` state. Auto-hide
never fully unmaps a configured Dock: an auto-hidden Dock remains mapped with a
small bounded edge reveal target, while the Layer Shell exclusive zone is zero.

The current Typhon snapshot is sufficient for the v1 intelligent policy: an
active toplevel with Maximized or Fullscreen state is considered obstructing. No
arbitrary overlap geometry is inferred. Intelligent mode behaves as Never when
there is no such active window and as Always when there is one. Pointer enter,
leave, reveal, and the bounded leave-delay timer are routed through the
controller; temporary reveal never changes the exclusive zone.

`DockLayerShellSurface` derives anchors and margins from the selected edge:
Bottom/Bottom, Left/Left, or Right/Right. Only the selected edge receives the
effective margin. The exclusive zone is the resting cross-axis thickness and is
never based on magnified or drag headroom.

`DockSurfaceGeometry` accepts a position and effective edge margin and maps
output-local context-menu anchors for all three edges. The old bottom-only helper
remains as a compatibility overload for existing bottom tests.

## Position-aware QML geometry

`DockPanel` keeps one magnification/reorder pass over a logical primary axis.
Bottom uses X as primary and grows upward; Left and Right use Y as primary and
grow respectively rightward and leftward in the cross-axis visual placement.
The same prefix displacement, closest delegate, drag origin, reorder target,
interaction-region, and fixed-envelope calculations are reused for every edge.
The visual chrome is placed at the selected output edge, while a vertical Dock's
primary strip remains centered on the output's vertical axis. Delegate icons use
the corresponding edge transform origin. Running indicators stay outside icon
transforms and are placed below/left/right for Bottom/Left/Right.

The fixed surface envelope is computed from structural row/configuration state,
never the current pointer frame. Input regions contain only the mapped chrome,
current transformed delegate targets, or the bounded reveal target when
collapsed. No transparent headroom becomes clickable and no surface resize or
exclusive-zone update occurs per pointer frame.

Corner radius is supplied by validated configuration and applied consistently to
the outer and inner chrome. Indicator presentation is configuration-driven:
line preserves the current active/inactive widths with configured thickness,
dot uses a compact circle, and none omits it. Known-running semantics remain
unchanged.

Dock animation durations are centralized in QML through a controller-provided
speed/enabled policy. Disabled animations use zero-duration transitions,
`1.0` keeps current timing, and larger values shorten all Dock-specific
transitions without touching global Settings or Shell animation policy.

## Settings controller and page

`SettingsDockController` is a focused native QObject exposed as
`SettingsController.dock`. It injects the config path for tests and uses the
production `~/.config/AstreaOS/dock.json` path by default. It exposes one
strongly typed Q_PROPERTY/setter per supported setting, a bounded `lastError`,
and `flush()`. Setters validate before changing local state, suppress redundant
signals, debounce continuous writes, and flush the last pending value. An
external replacement/recreation refreshes the effective properties through the
same shared codec. Failed commits restore the previous effective state and
report a bounded error.

The `dock` navigation descriptor is placed under the existing Appearance group
and routes to `pages/appearance/Dock.qml`. The page uses existing form
components, translated labels/options, and a small presentation-only preview
driven directly by `SettingsController.dock`. It contains Layout, Behavior, and
Indicators sections; dependent controls are disabled for non-magnification,
disabled animations, or no indicator as specified. It never imports resident
Dock QML and never accesses JSON, the filesystem, processes, IPC, DBus, or a
compositor.

## Testing and documentation

Shared contract/store tests cover defaults, parsing, enums, finite values,
clamping, migration, unknown-key/pin preservation, malformed-file refusal, and
atomic replacement. Existing Dock watcher/controller tests are extended for all
new properties, no-op changes, effective floating margin, and external reload.
Pure Layer Shell and output geometry tests cover all positions and auto-hide
reservation. Existing bottom QML tests remain intact; focused vertical,
indicator, animation, and auto-hide cases are added. Settings unit/integration/
static tests cover the controller, route, translated controls, preview, and
forbidden API boundaries.

The Dock architecture, configuration, runtime flow, testing, and Settings
architecture documents will describe canonical migration, intelligent-mode
limitations, fixed-envelope behavior, and the fact that offscreen tests do not
prove live Wayland/Typhon behavior.
