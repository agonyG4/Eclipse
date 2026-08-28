# Eclipse Dock Magnification and Persistent Reordering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add continuous Dock magnification and atomic persistent reordering of configured pins without changing Typhon or runtime-only ordering semantics.

**Architecture:** Extend the validated Dock configuration, add a narrow C++ JSON persistence boundary, and route stable-identity reorders through `DockController` into the existing structural `DockAppModel` reconciliation. Keep all magnification and drag preview geometry in QML transforms over a stable resting Row, and pass an explicit resting reservation height to Layer Shell.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/Test, QSaveFile, QAbstractListModel, QML Pointer Handlers, CMake/CTest.

## Global Constraints

- `desktopFileName` remains the stable Dock application identity.
- Configured pins precede runtime-only applications.
- Runtime-only applications append in deterministic first-observed order and do not follow focus changes.
- `DockAppModel::reconcileRows()` continues to use structural insert/remove/move operations and `beginMoveRows()`.
- QML remains presentation/interaction UI only and never parses or writes JSON.
- Typhon remains authoritative for runtime application/window state and is not modified.
- The Layer Shell exclusive zone remains the normal resting Dock height while a transparent visual surface may grow with transient headroom.
- `none`, `lift`, and `magnification` remain distinct selectable interaction modes; lift preserves the lightweight Eclipse hover, while magnification is suspended during reorder.
- The visible bottom-anchored chrome remains exactly the resting height in every mode; only transparent headroom grows.
- Runtime-only rows are not draggable; drag-out-to-unpin and drag-to-pin are out of scope.
- Preserve existing defaults, field-local fallback semantics, activation, launch suppression, and authority-loss behavior.

---

### Task 1: Add failing configuration and persistence tests

**Files:**
- Create: `Dock/services/DockConfigValidation.hpp`
- Create: `Dock/services/DockConfigPersistence.hpp`
- Create: `Dock/services/DockConfigPersistence.cpp`
- Modify: `Dock/tests/DockConfigWatcherTest.cpp`
- Create: `Dock/tests/DockConfigPersistenceTest.cpp`

**Interfaces:** `DockConfigWatcher` reuses shared desktop filename/JSON size validation; `DockConfigPersistence::writePins(const QStringList &, QString *)` is the C++ mutation boundary.

- [x] Add tests for magnification defaults, valid values, wrong types, non-finite values, and lower/upper clamps.
- [x] Add tests for preserving known/unknown keys, creating a missing file, refusing malformed/oversized files, rejecting duplicate/invalid replacement pins, atomic replacement, bounded write failure, and watcher reload after replacement.
- [x] Build and run the new focused tests before implementing production behavior; confirm failures are due to missing fields/boundary.

### Task 2: Implement shared Dock validation and configuration fields

**Files:**
- Modify: `Dock/services/DockConfigValidation.hpp`
- Modify: `Dock/services/DockConfigPersistence.*`
- Modify: `Dock/services/DockConfigWatcher.hpp`
- Modify: `Dock/services/DockConfigWatcher.cpp`
- Modify: `Dock/tests/DockConfigWatcherTest.cpp`
- Modify: `Dock/CMakeLists.txt`

- [x] Extract the existing maximum-size and desktop filename validation contract for reuse.
- [x] Add `magnificationEnabled`, `magnificationScale`, and `magnificationRadius` defaults and field-local numeric/bool validation with finite-number checks and bounded ranges.
- [x] Implement latest-object read, minimal missing-file creation, pins-only mutation, output-size validation, QSaveFile commit, bounded errors, and no replacement on malformed existing JSON.
- [x] Run the focused config/persistence tests to green and preserve the existing watcher debounce/re-add recovery.

### Task 3: Add failing controller/model reorder and reservation tests

**Files:**
- Create: `Dock/tests/DockLayerShellSurfaceTest.cpp`
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockAppModelTest.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/CMakeLists.txt`

- [x] Add controller tests for first-to-end, last-to-first, same-index no-op, invalid/non-pinned rejection, persistence failure atomicity, and launch/runtime state preservation.
- [x] Add runtime-only ordering and identity/state assertions around pinned reorder, including grouped/minimized rows through existing model/integration coverage.
- [x] Add a pure reservation-policy test proving visual height changes do not alter mapped reservation and unmapped reservation is zero.
- [x] Run the focused tests before implementation and confirm the expected missing-operation failures.

### Task 4: Implement controller persistence and explicit Layer Shell reservation

**Files:**
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`
- Modify: `Shell/runtime/ShellRuntime.cpp`
- Modify: `Shell/runtime/ShellRuntime.hpp`
- Modify: `Shell/app/AstreaShellApplication.cpp`
- Modify: `Dock/CMakeLists.txt`
- Modify: `Shell/CMakeLists.txt`

- [x] Inject one `DockConfigPersistence` into the unified Shell runtime and expose validated magnification/resting-height controller properties.
- [x] Implement stable-identity `movePinned`, clamping final target indices and updating model/config only after persistence succeeds.
- [x] Change `configure`/`updateExclusiveZone`/`setMapped` to receive explicit resting reservation height; never derive mapped exclusive zone from `QQuickWindow::height()`.
- [x] Keep empty/disabled mapping at zero and remapping at the resting height.
- [x] Run focused controller, Layer Shell, Shell runtime, and existing Typhon tests.

### Task 5: Implement QML magnification and pinned drag preview

**Files:**
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`

- [x] Add panel-level pointer tracking and one-pass raised-cosine scale/extra-width/prefix calculation.
- [x] Keep the Row's resting slots stable, translate delegates by cumulative extra widths, center the complete strip, and expand/shrink only the transparent visual surface above fixed resting chrome.
- [x] Scale only the icon from its bottom edge, keep running indicators unscaled, retain adequate source sampling, and keep tooltip targeting identity-based.
- [x] Add TapHandler/DragHandler separation, pinned-only drag enablement, thresholded preview, neighbor animations, center-relative drag geometry, a precise transformed-icon interaction target, grab-transition release/cancel finalization, one drop request, and no drag activation.
- [x] Run `qmllint` against all modified Dock QML and inspect for binding loops, index-based identity, and pointer feedback risks.

### Task 6: Update documentation

**Files:**
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/CONFIGURATION.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TESTING.md`

- [x] Document magnification settings/formula/layout, visual versus reserved height, pinned-only reorder lifecycle, atomic pins persistence, watcher behavior, controller/model/persistence ownership, and unchanged runtime-only ordering.
- [x] Correct stale claims that `astrea-dock` is the resident process; identify unified `astrea-shell` as the host and `astrea-dock` as compatibility IPC client.

### Task 7: Complete verification and branch assessment

- [x] Run the available Debug builds, focused Dock tests, QML lint, QML cache compilation, and `git diff --check`.
- [x] Run existing Dock Typhon runtime integration without changing Typhon; the unified Shell test remains blocked by unrelated in-progress context-menu edits.
- [x] Inspect the entire feature diff and worktree, confirming unrelated pre-existing changes remain untouched.
- [x] Unify the sole divergent local branch into `main` with a merge commit while preserving unrelated working-tree changes in a recoverable stash.
- [x] Report exact commands/results, manual live checks, unverified cases, and future runtime-only drag features.

### Post-review correction coverage

- [x] Restore the centered delegate baseline and add transient lift/drag headroom, with bounds checked across icon sizes `32..64`.
- [x] Keep the bottom chrome at the resting height while visual surface height changes.
- [x] Make pointer release and cancellation terminal paths mutually exclusive, and cover click/drag/reset sequencing in the offscreen QML test.
- [x] Make drag coordinates invariant under centered surface-width animation, lift only the dragged delegate, align pointer bounds to transformed icon geometry, and test exclusive grab transitions directly.

### Final reorder/model-move closure

- [x] Add failing QML coverage for rendered-center drag origins on first, middle, and last pins, post-move geometry refresh, and release-pointer hover restoration.
- [x] Capture the source's transformed center before suspending magnification, refresh hover arrays after `rowsMoved`, and remove the parallel `delegateKeys` cache.
- [x] Feed `DragHandler.centroid.scenePosition` through the active drag signal, update pointer state during exclusive drags, and use `activeTranslation.x` for thresholded movement.
- [x] Update Dock documentation/spec notes and run the focused Debug/Release Dock and unified Shell verification.

### Wayland input-region closure

- [x] Add the pure Qt input-region policy and cached QQuickWindow bridge.
- [x] Report exact transformed QML interaction targets and use the semantic
      chrome/target pointer boundary.
- [x] Cover invalid/clipped geometry, transformed headroom, and exclusive
      release behavior in focused tests.
- [x] Document and verify visual surface, QWindow input region, and Layer Shell
      exclusive-zone separation; live compositor pass-through remains separately
      unverified in the offscreen test environment.
