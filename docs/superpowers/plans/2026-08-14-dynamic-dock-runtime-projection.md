# Dynamic Dock Runtime Projection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Project authoritative Typhon applications into a stable Dock union of configured pins and currently running resolved non-pinned applications, while giving Explorer its canonical first-party app identity.

**Architecture:** The Typhon projector will emit only live resolved application state plus deterministic first-observed order. `DockAppModel` will own configured pins, runtime authority, dynamic membership, and structural row updates; `DockController` will retain the same exact-window activation state and truthful pin counters. Explorer will publish `astrea-explorer` from its root QML so the existing matcher resolves it without heuristics.

**Tech Stack:** C++20, Qt 6 `QAbstractListModel`/QTest, Typhon v2 Wayland protocol, QML/Quickshell 0.3.0, CMake/CTest.

## Global Constraints

- Typhon production source remains unchanged unless a concrete protocol contract defect is proven.
- Application identity comes from client `app_id`; title, PID, process inspection, and launcher heuristics remain forbidden.
- Configured pins precede runtime-only rows; runtime-only order is first-observed and does not follow focus changes.
- Runtime authority loss keeps pins as neutral unknown rows and removes runtime-only rows.
- Existing Layer Shell integration and Dock visuals remain unchanged.
- All new documentation is English.

---

### Task 1: Refactor the pure runtime projector

**Files:**
- Modify: `shared/platform/typhon/DockApplicationStateProjector.hpp`
- Modify: `shared/platform/typhon/DockApplicationStateProjector.cpp`
- Modify: `shared/tests/DockApplicationStateProjectorTest.cpp`

- [ ] Replace the pin-seeded hash return with a `DockApplicationRuntimeProjection` containing `states` and unique `encounterOrder`.
- [ ] Preserve matcher resolution, minimized/active aggregation, duplicate-PID handling, window counts, and descending focus-serial window IDs.
- [ ] Add RED tests for empty projections, non-pinned apps, encounter order, duplicate encounters, and focus-only changes.
- [ ] Run `dock-application-state-projector-test` to observe the pre-fix failure.
- [ ] Implement the smallest projector change and run the focused test to PASS.

### Task 2: Make the model own the pins/runtime union

**Files:**
- Modify: `Dock/core/DockAppInfo.hpp`
- Modify: `Dock/core/DockAppModel.hpp`
- Modify: `Dock/core/DockAppModel.cpp`
- Modify: `Dock/tests/DockAppModelTest.cpp`

- [ ] Add model-owned pin configuration, runtime state, dynamic order, and authority state.
- [ ] Add an explicit runtime projection/clear API and derive `pinned`, runtime roles, and membership from current inputs.
- [ ] Preserve launch state and errors while using insert/remove/move/dataChanged updates rather than reset-per-snapshot.
- [ ] Add RED tests for dynamic rows, close removal, grouping, minimized state, stable ordering, new-app append, pin/unpin transitions, stopped-pin retention, and authority loss.
- [ ] Implement and run `dock-app-model-test` to PASS.

### Task 3: Integrate controller projection and truthful counters

**Files:**
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockTyphonRuntimeIntegrationTest.cpp`
- Inspect: `Shell/app/AstreaShellApplication.cpp`

- [ ] Pass the complete authoritative projection to the model and clear both controller/model runtime state on authority loss.
- [ ] Make `pinCount` configured-pin count and `resolvedPinCount` count only configured resolved pins.
- [ ] Preserve exact WindowId activation and stale-target no-fallback behavior for dynamic rows.
- [ ] Add RED controller/integration coverage for dynamic appearance, activation, minimized activation, close removal, multi-window grouping, and disconnect semantics.
- [ ] Implement and run focused controller/integration tests to PASS.

### Task 4: Correct Explorer’s first-party identity

**Files:**
- Modify: `/home/agony/.local/share/Astrea/src/Apps/Explorer/Main.qml`
- Test/inspect: `/home/agony/.local/share/Astrea/src/Apps/Explorer/astrea-explorer.desktop`
- Test/inspect: `/home/agony/.local/share/Astrea/src/System/tests/test_bin_launchers.py`

- [ ] Add the supported root `//@ pragma AppId astrea-explorer` contract to the current Explorer source.
- [ ] Preserve the existing desktop entry and launcher route; audit first-party Quickshell desktop entries without mass refactoring.
- [ ] Add or extend the smallest source contract test available for the AppId declaration.
- [ ] Run the Explorer-focused test and verify Eclipse’s matcher resolves `astrea-explorer` to `astrea-explorer.desktop`.

### Task 5: Update documentation and verify the complete product path

**Files:**
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TYPHON_RUNTIME_STATE.md`
- Modify: `Dock/docs/TESTING.md`

- [ ] Document the projector/model/controller boundaries, stable union ordering, authority semantics, exact activation, and first-party AppId contract.
- [ ] Run focused tests, all relevant Debug and Release CTest suites, QML lint, LayerShell-enabled builds, and `git diff --check`.
- [ ] Inspect the final diff for forbidden identity/process heuristics and unintended visual changes.
- [ ] Perform safe live checks and report each acceptance case as PASS, FAIL, or NOT EXECUTED.
- [ ] Commit Eclipse as `feat(dock): project running applications dynamically` and commit only the Explorer identity correction separately if required by its repository state.
