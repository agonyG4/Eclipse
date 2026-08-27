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
- The Layer Shell exclusive zone remains the normal resting Dock height while the visual surface may grow.
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

- [ ] Add tests for magnification defaults, valid values, wrong types, non-finite values, and lower/upper clamps.
- [ ] Add tests for preserving known/unknown keys, creating a missing file, refusing malformed/oversized files, rejecting duplicate/invalid replacement pins, atomic replacement, bounded write failure, and watcher reload after replacement.
- [ ] Build and run the new focused tests before implementing production behavior; confirm failures are due to missing fields/boundary.

### Task 2: Implement shared Dock validation and configuration fields

**Files:**
- Modify: `Dock/services/DockConfigValidation.hpp`
- Modify: `Dock/services/DockConfigPersistence.*`
- Modify: `Dock/services/DockConfigWatcher.hpp`
- Modify: `Dock/services/DockConfigWatcher.cpp`
- Modify: `Dock/tests/DockConfigWatcherTest.cpp`
- Modify: `Dock/CMakeLists.txt`

- [ ] Extract the existing maximum-size and desktop filename validation contract for reuse.
- [ ] Add `magnificationEnabled`, `magnificationScale`, and `magnificationRadius` defaults and field-local numeric/bool validation with finite-number checks and bounded ranges.
- [ ] Implement latest-object read, minimal missing-file creation, pins-only mutation, output-size validation, QSaveFile commit, bounded errors, and no replacement on malformed existing JSON.
- [ ] Run the focused config/persistence tests to green and preserve the existing watcher debounce/re-add recovery.

### Task 3: Add failing controller/model reorder and reservation tests

**Files:**
- Create: `Dock/tests/DockLayerShellSurfaceTest.cpp`
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockAppModelTest.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/CMakeLists.txt`

- [ ] Add controller tests for first-to-end, last-to-first, same-index no-op, invalid/non-pinned rejection, persistence failure atomicity, and launch/runtime state preservation.
- [ ] Add runtime-only ordering and identity/state assertions around pinned reorder, including grouped/minimized rows through existing model/integration coverage.
- [ ] Add a pure reservation-policy test proving visual height changes do not alter mapped reservation and unmapped reservation is zero.
- [ ] Run the focused tests before implementation and confirm the expected missing-operation failures.

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

- [ ] Inject one `DockConfigPersistence` into the unified Shell runtime and expose validated magnification/resting-height controller properties.
- [ ] Implement stable-identity `movePinned`, clamping final target indices and updating model/config only after persistence succeeds.
- [ ] Change `configure`/`updateExclusiveZone`/`setMapped` to receive explicit resting reservation height; never derive mapped exclusive zone from `QQuickWindow::height()`.
- [ ] Keep empty/disabled mapping at zero and remapping at the resting height.
- [ ] Run focused controller, Layer Shell, Shell runtime, and existing Typhon tests.

### Task 5: Implement QML magnification and pinned drag preview

**Files:**
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`

- [ ] Add panel-level pointer tracking and one-pass raised-cosine scale/extra-width/prefix calculation.
- [ ] Keep the Row's resting slots stable, translate delegates by cumulative extra widths, center the complete strip, and expand/shrink only the visual surface.
- [ ] Scale only the icon from its bottom edge, keep running indicators unscaled, retain adequate source sampling, and keep tooltip targeting identity-based.
- [ ] Add TapHandler/DragHandler separation, pinned-only drag enablement, thresholded preview, neighbor animations, one drop request, and no drag activation.
- [ ] Run `qmllint` against all modified Dock QML and inspect for binding loops, index-based identity, and pointer feedback risks.

### Task 6: Update documentation

**Files:**
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/CONFIGURATION.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TESTING.md`

- [ ] Document magnification settings/formula/layout, visual versus reserved height, pinned-only reorder lifecycle, atomic pins persistence, watcher behavior, controller/model/persistence ownership, and unchanged runtime-only ordering.
- [ ] Correct stale claims that `astrea-dock` is the resident process; identify unified `astrea-shell` as the host and `astrea-dock` as compatibility IPC client.

### Task 7: Complete verification and branch assessment

- [ ] Run `cmake` configure/build for available Debug and Release configurations, focused Dock/Shell tests, affected CTest, QML lint, and `git diff --check`.
- [ ] Run existing Dock Typhon runtime integration and Shell unified runtime integration tests without changing Typhon.
- [ ] Inspect the entire diff and worktree, confirming unrelated pre-existing changes remain untouched.
- [ ] Check local branch divergence and integrate only branches that can be safely unified into `main` without overwriting unrelated user work; report any branch not safely merged.
- [ ] Report exact commands/results, manual live checks, unverified cases, and future runtime-only drag features.
