# Eclipse Dock Magnification and Persistent Reordering Design

## Scope

Add continuous macOS-style Dock magnification and persistent reordering of
configured pins while preserving the unified `astrea-shell` ownership, the
desktop filename identity contract, Typhon runtime authority, and the
model's structural row reconciliation.

## Architecture

`DockConfigWatcher` will expose validated magnification settings alongside the
existing field-local configuration. Shared validation helpers will be reused
by a new `DockConfigPersistence` boundary. The persistence boundary reads the
current JSON object, validates the complete replacement pin list, changes only
`pins`, and atomically replaces the file with `QSaveFile`; malformed or
oversized existing files are never replaced.

`DockController::movePinned(desktopFileName, targetPinIndex)` will be the only
owner of reorder policy. It verifies the stable identity is currently pinned,
clamps the final pin index, persists once, and updates `DockAppModel` only
after a successful write. `DockAppModel::setPins()` remains the existing
structural reconciliation path, so delegate and launch/runtime state follow
desktop filename identity through `beginMoveRows()`.

`DockPanel` will retain a fixed resting `Row` geometry and a fixed-height
bottom-anchored chrome rectangle. Its selectable hover modes are `none`, the
lightweight Eclipse `lift` effect, and continuous `magnification`; reorder
temporarily suspends either hover effect. A panel-level hover tracker computes
each slot's magnification from a symmetric raised-cosine distance curve in
O(n), then assigns icon scale and cumulative visual translations to the
existing delegates. Icons scale from their bottom edge; indicators stay
outside that transform. The transparent visual surface can add transient
headroom above the resting chrome so the centered baseline, lift, magnification,
and drag bounds remain inside the surface. It returns to resting dimensions
after exit. The controller's resting Dock height is the sole exclusive-zone
contract.

Pinned delegates will use `DragHandler` with the existing Qt Quick pointer
stack and a movement threshold, while `TapHandler` retains click activation.
The panel records the dragged desktop filename and original pin index, moves
neighbors through an ephemeral preview, and emits one identity-based reorder
request on drop. Drag origin and target calculations use coordinates relative to
the panel center, which remains invariant under symmetric surface-width
animation. Only the dragged delegate receives the vertical reorder lift. A
grab-transition state machine treats `UngrabExclusive` as successful completion
and `CancelGrabExclusive` as cancellation, so finish and cancel cannot both be
emitted and passive transitions do not finalize a drag. A precise panel-level
interaction target follows the transformed icon bounds, including bottom-origin
vertical growth, without making unrelated headroom clickable. Runtime-only rows
have no drag handler and are never pinned implicitly.

During an active drag, the panel captures the source delegate's current
rendered center before suspending magnification and uses the current
`desktopFileName` on each delegate as identity. The drag handler forwards its
active scene centroid with translation updates; because Qt may reset the
centroid before an exclusive ungrab callback, the delegate retains the last
valid active point and restores it to the panel before hover is resumed. A
deferred `rowsMoved` callback refreshes hover geometry after the model's
`beginMoveRows()` reconciliation, and no separate QML identity cache is kept.

The Dock surface has three intentionally separate geometry contracts. The visual
QQuickWindow may grow above the resting chrome for magnification and drag
headroom. QML reports the exact transformed delegate interaction targets and the
chrome rectangle to a C++ `DockInputRegionBridge`; its pure
`DockInputRegionPolicy` validates, clips, unions, and bounds those rectangles,
and the bridge caches and applies the resulting `QRegion` with
`QQuickWindow::setMask()`. The Layer Shell exclusive zone remains the explicit
resting height. The QML semantic pointer boundary mirrors the same chrome/target
union, but a Dock-side ignored event must not be described as proof that the
compositor delivered it to the underlying application.

## Error and lifecycle behavior

Persistence errors are bounded and exposed through `DockController::lastError`;
the in-memory config and model remain unchanged on failure. A successful write
updates the model immediately, while the existing watcher may subsequently
reload the same atomic result without a feedback loop. Layer Shell mapping
uses zero for an unmapped Dock and the normal resting height for a mapped Dock,
regardless of the current visual `QQuickWindow` height.

## Verification

Focused Qt tests will cover magnification parsing, atomic pin persistence,
watcher recovery, controller/model reorder semantics, identity/state
preservation, failure atomicity, and the explicit resting reservation policy.
Existing Typhon and Shell integration tests will be rerun unchanged. QML will
be checked with `qmllint`; visual acceptance cases that require a live Wayland
session will be reported separately from deterministic tests.

Runtime-only drag-to-pin and drag-out-to-unpin remain future work.
