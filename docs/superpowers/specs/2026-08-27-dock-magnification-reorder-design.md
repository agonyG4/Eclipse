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

`DockPanel` will retain a fixed resting `Row` geometry. A panel-level hover
tracker will compute each slot's scale from a symmetric raised-cosine distance
curve in O(n), then assign icon scale and cumulative visual translations to
the existing delegates. Icons scale from their bottom edge; indicators stay
outside that transform. The panel's visual width/height expand around the
stable centered strip and return to the resting metrics after exit. The
controller's resting Dock height is the sole exclusive-zone contract.

Pinned delegates will use `DragHandler` with the existing Qt Quick pointer
stack and a movement threshold, while `TapHandler` retains click activation.
The panel records the dragged desktop filename and original pin index, moves
neighbors through an ephemeral preview, and emits one identity-based reorder
request on drop. Runtime-only rows have no drag handler and are never pinned
implicitly.

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
