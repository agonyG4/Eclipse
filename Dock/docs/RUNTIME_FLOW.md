# Dock Runtime Flow

```text
startup
  -> runtime paths
  -> config
  -> desktop-entry catalog
  -> model (configured pins)
  -> one Typhon toplevel connection
  -> authoritative runtime projection (pins + live resolved apps)
  -> QML
  -> QWindow input region (chrome + transformed interaction targets)
  -> position-aware Layer Shell mapping at resting cross-axis reservation
  -> configured none/lift/magnification hover effect or pinned drag preview
  -> click or one identity-based reorder request
  -> exact Typhon activation or shared launcher
  -> Typhon shell-control launch path
```

Icon source quality is resolved independently of the hover frame. The shared
`AstreaAppIcon` computes a maximum logical presentation extent, reads the
effective screen DPR, and requests `ceil(logicalExtent * DPR)` physical pixels
from `AstreaIconProvider`. Dock magnification changes only the delegate
transform; a configuration change can change the maximum source target once,
but a hover animation does not reload the source texture.

The provider passes named icons directly to `QIcon::pixmap()` so Qt retains
Freedesktop `Scale`, fixed/threshold/scalable directory, and theme-inheritance
semantics. It normalizes only the returned pixmap DPR metadata at the QML
boundary; it does not resample or reselect a representation using
`availableSizes()`. Theme mutation and named-icon lookup share one narrow
mutex. The Dock DPR boundary test records `Screen.devicePixelRatio`,
`QQuickWindow::devicePixelRatio()`, and
`QQuickWindow::effectiveDevicePixelRatio()` for 1x, 1.5x, and 2x runs.

At startup, `DockRuntimePaths` resolves `ASTREA_ROOT` and the two AstreaOS
configuration paths. `DockConfigWatcher` loads validated defaults or the user
file, while `DesktopEntryCatalog` publishes an immutable XDG-priority
snapshot. The controller gives the model the configured pin order and QML
receives the model through a context property.

The application configures the Dock QQuickWindow through the required
LayerShellQt integration using `DockController::exclusiveZone()` as an explicit
resting cross-axis contract. Bottom, Left, and Right select the corresponding
Layer Shell edge; only that edge receives the effective margin. QML reserves a
fixed maximum transparent envelope for the configured primary-axis rows and
possible magnification/lift/drag bounds before pointer interaction begins. The
requested surface dimensions therefore stay constant during hover and reorder;
only the resting chrome's primary extent and icon transforms animate. The
exclusive zone never includes temporary magnification headroom. `none` and
`lift` clear stale scale and translate state when switching modes.
Configuration and component toggles are debounced and re-applied without
restarting the process. A Layer
Shell setup or mapping failure is reported as a shell failure; the Dock is
never shown as an ordinary Qt window. A click calls
`DockController::launchByDesktopFileName`. If authoritative Typhon
runtime state identifies a live window, the controller submits one exact
`activate` action for the most recent focus-serial candidate, including
minimized windows. The shared Typhon connection owns authentication, pending
state, completion, and generation cleanup. If no live window is known, the
existing controller and shared supervised launcher path is used. An
unavailable or failed action is reconciled without launching on that same
click.

Each QML geometry update also reports the actual mapped `dockChrome` rectangle
and the exact current transformed interaction-target rectangles to
`DockInputRegionBridge`. The bridge clips them through the pure
`DockInputRegionPolicy`, caches the last integer `QRegion`, and calls
`QQuickWindow::setMask()` only when that region changes. The mask follows
animated magnification, lift, and drag geometry; it is not the full fixed
visual envelope, and a mapped Dock with no rows still has its chrome region.
The semantic QML pointer boundary uses the same chrome/interaction union.
Therefore “the Dock ignored this click” describes Dock event arbitration; only a
live compositor test can establish that the underlying application actually
received the event.

The configured `hoverEffect` selects the pointer interaction. `none` leaves all
delegates at rest. `lift` scales only the directly hovered delegate to about
`1.1` and offsets it upward by about five pixels. In `magnification` mode, each
resting icon center receives the raised-cosine influence
`0.5 * (1 + cos(pi * distance / radius))` inside the configured radius and zero
outside it. The panel derives prefix extra widths from those scales and applies
visual translations; it does not rebuild the model or relayout a Row for each
pointer sample. Only the icon is scaled from its edge-facing primary-axis
origin (Bottom: bottom, Left: left, Right: right), leaving its running indicator
stable. Reorder temporarily suspends hover visuals in all
modes, then restores the configured effect after drop or cancellation.

When `floating` is false, the selected Layer Shell edge margin is zero while
the configured `edgeMargin` remains persisted for re-enabling floating. Auto-hide
never unmaps an enabled Dock with rows: Always retains a bounded edge reveal
target and zero exclusive zone, while Intelligent enters that behavior only for
an active maximized or fullscreen Typhon toplevel. The heuristic intentionally
does not claim overlap detection because v1 Typhon data has no output-local
geometry. Revealing a collapsed Dock does not change its exclusive zone.

A drag handler is enabled only for the configured-pin prefix. Once the system
drag threshold is crossed, QML records the stable desktop filename, captures
the delegate's current rendered/transformed center, raises, lifts, and scales
only that delegate, and previews the target index by translating neighboring
pins on their resting vertical baseline. The drag origin, current center, and
target-slot centers use panel-center-relative coordinates, so symmetric
visual-surface-width animation cannot introduce a synthetic drag movement. The
handler maps its active centroid into panel coordinates through the drag update;
the panel restores the last valid panel point after Qt clears the centroid at
ungrab. The handler's `GrabExclusive`, `UngrabExclusive`, and
`CancelGrabExclusive`
transitions make release and cancellation mutually exclusive; passive
transitions do not finalize a reorder. A click below threshold remains
activation-only. A precise panel-level interaction target tracks the transformed
icon rectangle, including its bottom-origin headroom, without making unrelated
transparent space actionable. The authoritative model and on-disk
configuration remain unchanged during the preview. On a moved drop, QML emits
one reorder request; the controller reads the current pin list, atomically
persists it, then calls the normal model reconciliation path. The existing
`rowsMoved` signal schedules a deferred hover-geometry refresh so current
delegate identity and magnification arrays remain aligned after a move.
Runtime-only rows remain after the pin section and retain their deterministic
first-observed order.

The Dock owns one Typhon toplevel connection. After the initial snapshot
commits, `DockApplicationStateProjector` emits only resolved live applications
and a deterministic encounter order. `DockAppModel` unions that projection
with configured pins: pins remain first, and new runtime-only applications
append without focus-driven reordering. A missing runtime entry removes a
runtime-only row but leaves a configured pin as a known stopped row.

If the connection is unavailable, each pinned item exposes
`runtimeKnown=false` and neutral runtime booleans; runtime-only rows are
removed. A resolved running item uses exact Typhon activation rather than a
duplicate launch; stale or unavailable targets reconcile state and never fall
through to a same-click launch.
