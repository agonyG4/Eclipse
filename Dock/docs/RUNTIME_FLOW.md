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
  -> Layer Shell mapping at resting reservation
  -> configured none/lift/magnification hover effect or pinned drag preview
  -> click or one identity-based reorder request
  -> exact Typhon activation or shared launcher
  -> Typhon shell-control launch path
```

At startup, `DockRuntimePaths` resolves `ASTREA_ROOT` and the two AstreaOS
configuration paths. `DockConfigWatcher` loads validated defaults or the user
file, while `DesktopEntryCatalog` publishes an immutable XDG-priority
snapshot. The controller gives the model the configured pin order and QML
receives the model through a context property.

The application configures the Dock QQuickWindow through the required
LayerShellQt integration using `DockController::restingHeight()` as an explicit
exclusive-zone contract. The visual surface can grow taller while the pointer
is over the Dock or while a large icon needs lift/drag headroom, but the
exclusive zone remains at the resting height. The visible chrome remains at
that same resting height and the transparent surface shrinks after hover or
drag completion. `none` and `lift` clear stale scale and translate state when
switching modes. Configuration and component toggles are debounced and
re-applied without restarting the process. A Layer
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

Each QML geometry update also reports the visible chrome and the exact current
transformed interaction-target rectangles to `DockInputRegionBridge`. The bridge
clips them through the pure `DockInputRegionPolicy`, caches the last integer
`QRegion`, and calls `QQuickWindow::setMask()` only when that region changes. The
mask follows animated magnification, lift, and drag geometry; it is not the full
visual surface, and a mapped Dock with no rows still has its chrome region. The
semantic QML pointer boundary uses the same chrome/interaction union. Therefore
“the Dock ignored this click” describes Dock event arbitration; only a live
compositor test can establish that the underlying application actually received
the event.

The configured `hoverEffect` selects the pointer interaction. `none` leaves all
delegates at rest. `lift` scales only the directly hovered delegate to about
`1.1` and offsets it upward by about five pixels. In `magnification` mode, each
resting icon center receives the raised-cosine influence
`0.5 * (1 + cos(pi * distance / radius))` inside the configured radius and zero
outside it. The panel derives prefix extra widths from those scales and applies
visual translations; it does not rebuild the model or relayout a Row for each
pointer sample. Only the icon is scaled from its bottom edge, leaving its
running indicator stable. Reorder temporarily suspends hover visuals in all
modes, then restores the configured effect after drop or cancellation.

A drag handler is enabled only for the configured-pin prefix. Once the system
drag threshold is crossed, QML records the stable desktop filename, captures
the delegate's current rendered/transformed center, raises, lifts, and scales
only that delegate, and previews the target index by translating neighboring
pins on their resting vertical baseline. The drag origin, current center, and
target-slot centers use panel-center-relative coordinates, so symmetric
visual-surface-width animation cannot introduce a synthetic drag movement. The
handler passes its active centroid scene position through the drag update; the
panel also restores the last valid centroid after Qt clears it at ungrab. The
handler's `GrabExclusive`, `UngrabExclusive`, and `CancelGrabExclusive`
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
