# Eclipse Shell Frosted Material Geometry Design

## Goal

Fix the Eclipse-only frosted Shell visual regression without changing Typhon,
the Dock Layer Shell envelope, or the existing icon interaction model.

## Approved design

Topbar segments expose one authoritative `surfaceRadius` property. The visible
`BarSegment` rectangle and each Launcher/Status `BackdropRegion` bind to that
property, so the visual and blur geometries cannot diverge or pass an undefined
radius to QML.

The Dock keeps its existing `surfaceWidth`, `surfaceHeight`, headroom, placement,
and Layer Shell behavior. Its `dockChrome` becomes a stable resting-sized
material rectangle in both orientations. Magnification continues to affect
only delegate scale and visual offsets; the existing interaction target remains
reparented to the panel and continues to publish dynamic scaled icon bounds.

The shared `Astrea.Shared/ShellMaterialTheme.qml` object owns only cross-Shell
material state, palette tokens, and shared radii. It copies the current
`ShellBarTheme` six-combination Borealis policy exactly. `ShellBarTheme` keeps
its public API and forwards material state/tokens from this object. Dock uses
the shared background and border directly and removes its hard-coded dark plate
and unconditional black overlay.

## Testing

Bar QML tests assert the radius authority, Launcher/Status backdrop radius
parity, and shared palette behavior. Dock hover tests assert that the outer
surface remains preallocated and stable, the chrome remains resting-sized,
magnified icons overhang the chrome inside the envelope, and the input mask
continues to cover those icons without activating transparent corners. A shared
backdrop geometry test asserts that a positive-radius 36px pill decomposes into
multiple rectangles while excluding the top-left rounded corner.

No Typhon source, blur policy, compositor planning, or unrelated dirty files
are in scope.
