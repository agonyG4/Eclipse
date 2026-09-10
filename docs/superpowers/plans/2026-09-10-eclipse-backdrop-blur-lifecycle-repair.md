# Eclipse Backdrop Blur Lifecycle Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repair existing Shell backdrop blur synchronization across mapping, native surface recreation, capability changes, ancestor animation, and dynamic item destruction.

**Architecture:** Keep `AstreaBackgroundEffectBinding` as the sole protocol owner. Extend `AstreaBackdropEffectRegions` with window lifecycle/event-filter observation and an animation synchronization boundary, and make each `AstreaBackdropRegion` source lifetime-safe with `QPointer`. Preserve all existing QML exact-region integrations and use queued synchronization plus binding deduplication to avoid update loops.

**Tech Stack:** C++20, Qt 6.8 Qt Quick/QML, Qt Test, Wayland client protocol, existing Ninja build directory `/home/agony/GitHub/Eclipse/build/debug` or the configured existing build selected by CMake.

## Global Constraints

- Preserve Dock → `dockChrome`, Bar → `launcherPill`/`statusPill`/`activePopup`, Tray → `tooltipCard`, Spotlight → `panel`, AltTab → visible panel, and ContextMenu → visible card(s).
- Keep fullscreen transparent envelopes unblurred, Bar reserve and Desktop interaction without blur.
- Do not issue raw `wl_surface_commit()`; request a Qt frame/update after effective protocol state changes.
- Do not create alternate build directories; reuse the existing configured build and run CTest from that directory.
- Do not modify or stage unrelated existing Eclipse working-tree changes.

---

### Task 1: Make region sources lifetime-safe and add lifecycle test seams

**Files:**
- Modify: `shared/platform/wayland/effects/AstreaBackdropRegion.hpp`
- Modify: `shared/platform/wayland/effects/AstreaBackdropRegion.cpp`
- Modify: `shared/platform/wayland/effects/AstreaBackgroundEffectBinding.hpp`
- Modify: `shared/platform/wayland/effects/AstreaBackgroundEffectBinding.cpp`
- Test: `shared/tests/WaylandEffectsStateTest.cpp`

**Interfaces:**
- `AstreaBackdropRegion::item()` returns a null-safe `QQuickItem *` backed by `QPointer<QQuickItem>`.
- Existing `AstreaBackgroundEffectLifecycleState` remains the deterministic generation/effect ownership seam.

- [ ] **Step 1: Add a failing lifetime regression around a destroyed source item.** Construct a descriptor and a `QQuickItem`, assign it, delete the item, and assert the descriptor returns `nullptr`.

- [ ] **Step 2: Run the focused test and confirm it fails against the raw pointer implementation.**

```bash
cmake --build /home/agony/GitHub/Eclipse/build/debug --target backdrop-effect-regions-test wayland-effects-state-test
QT_QPA_PLATFORM=offscreen /home/agony/GitHub/Eclipse/build/debug/shared/backdrop-effect-regions-test
```

- [ ] **Step 3: Replace the raw item member with `QPointer<QQuickItem>`.** Return `.data()` and keep property notifications unchanged.

- [ ] **Step 4: Extend the lifecycle seam test to assert generation A is destroyed before generation B is bound and only one effect is owned for each generation.**

- [ ] **Step 5: Run the focused tests and verify they pass.**

- [ ] **Step 6: Commit the lifetime-safe source change.**

```bash
rtk git add shared/platform/wayland/effects/AstreaBackdropRegion.hpp shared/platform/wayland/effects/AstreaBackdropRegion.cpp shared/tests/WaylandEffectsStateTest.cpp
rtk git commit -m "fix(effects): make backdrop region sources lifetime safe"
```

### Task 2: Observe mapped-window lifecycle and request Qt commits

**Files:**
- Modify: `shared/platform/wayland/effects/AstreaBackdropEffectRegions.hpp`
- Modify: `shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp`
- Modify: `shared/platform/wayland/effects/AstreaBackgroundEffectBinding.cpp`
- Test: `shared/tests/BackdropEffectRegionsTest.cpp`

**Interfaces:**
- `AstreaBackdropEffectRegions` installs/removes an event filter on the current `QQuickWindow` and handles `QPlatformSurfaceEvent::SurfaceCreated` and `SurfaceAboutToBeDestroyed`.
- Window `visibleChanged`, `widthChanged`, `heightChanged`, `afterAnimating`, and `AstreaWaylandEffects::availableChanged` schedule synchronization.
- The existing-window call to `AstreaBackgroundEffectBinding::sync()` passes `requestFrame=true`.

- [ ] **Step 1: Add failing hidden-to-visible and ancestor-transform tests.** Use an offscreen `QQuickWindow`, a parent/item tree, and a descriptor. Assert hidden resolution is empty, visible resolution is non-empty, then change only parent scale/x/y and invoke the normal `afterAnimating` callback; assert the mapped region changes.

- [ ] **Step 2: Run the new tests before implementation.**

```bash
cmake --build /home/agony/GitHub/Eclipse/build/debug --target backdrop-effect-regions-test
QT_QPA_PLATFORM=offscreen /home/agony/GitHub/Eclipse/build/debug/shared/backdrop-effect-regions-test
```

Expected: hidden-to-visible and ancestor synchronization assertions fail because the controller has no window lifecycle/animation observer.

- [ ] **Step 3: Add observed-window bookkeeping and lifecycle connections.** Disconnect/remove the old filter on window replacement; connect visibility and size signals; connect `afterAnimating`; connect manager availability; install the filter on the new window; schedule one queued sync for each boundary event.

- [ ] **Step 4: Handle native surface events.** On `SurfaceAboutToBeDestroyed`, call the binding’s destroy path before native teardown and clear resolved/effective state. On `SurfaceCreated`, notify the binding, re-resolve the current region, and schedule reapplication for the new native surface.

- [ ] **Step 5: Make hidden sync release active state and change existing-window sync to `requestFrame=true`.** Keep `QQuickWindow::update()` inside the existing effects manager; do not call raw Wayland commit.

- [ ] **Step 6: Add item `destroyed` connections in `refreshConnections()`.** A destroyed QML source must schedule a clear without dereferencing it.

- [ ] **Step 7: Run the focused tests and verify hidden-to-visible, surface-generation, transform, and destruction behavior.**

```bash
cmake --build /home/agony/GitHub/Eclipse/build/debug --target backdrop-effect-regions-test wayland-effects-state-test
QT_QPA_PLATFORM=offscreen /home/agony/GitHub/Eclipse/build/debug/shared/backdrop-effect-regions-test
QT_QPA_PLATFORM=offscreen /home/agony/GitHub/Eclipse/build/debug/shared/wayland-effects-state-test
```

- [ ] **Step 8: Commit the lifecycle/runtime repair.**

```bash
rtk git add shared/platform/wayland/effects/AstreaBackdropEffectRegions.hpp shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp shared/platform/wayland/effects/AstreaBackgroundEffectBinding.cpp shared/tests/BackdropEffectRegionsTest.cpp
rtk git commit -m "fix(effects): resync backdrop blur across window lifecycle"
```

### Task 3: Reapply on capability changes and preserve exact Shell mappings

**Files:**
- Modify: `shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp`
- Test: `shared/tests/BackdropEffectRegionsTest.cpp`
- Test: existing Dock/Bar/Spotlight/AltTab/ContextMenu/Settings tests only if a focused assertion exposes a regression

- [ ] **Step 1: Add a failing capability-transition test seam.** Simulate manager availability loss and return around a requested Frosted region; assert active state clears on loss and a fresh sync is scheduled/reapplied on return.

- [ ] **Step 2: Run the focused test and capture the missing-observer failure.**

- [ ] **Step 3: Connect `AstreaWaylandEffects::availableChanged` for the observed target window.** On loss, call the binding sync/release path; on return, schedule the current requested region without requiring a style toggle or resize.

- [ ] **Step 4: Verify existing QML mapping literals are unchanged.** Use `rtk rg` to compare the exact current region choices and ensure no fullscreen envelope gains a descriptor.

- [ ] **Step 5: Run focused Shell component tests.**

```bash
ctest --test-dir /home/agony/GitHub/Eclipse/build/debug --output-on-failure -R 'wayland-effects-state-test|backdrop-effect-regions-test|dock|bar|spotlight|alt.?tab|context.?menu|settings.*effect'
```

- [ ] **Step 6: Commit only if this task adds a separate capability change.**

### Task 4: Full Eclipse verification and real Shell observations

**Files:**
- Modify: only files required by verified failures.

- [ ] **Step 1: Build with the existing configured build directory.**

```bash
cmake --build /home/agony/GitHub/Eclipse/build/debug --parallel 2
```

- [ ] **Step 2: Run the complete CTest suite.**

```bash
ctest --test-dir /home/agony/GitHub/Eclipse/build/debug --output-on-failure
```

- [ ] **Step 3: Run focused lifecycle/geometry and Shell integration tests.** Record fresh results for Wayland effects state, BackdropEffectRegions lifecycle/geometry, Dock, Bar, Spotlight, AltTab, ContextMenu, and Settings effect surface.

- [ ] **Step 4: Launch the real Shell using its existing configured runtime and record each mandatory visual observation as `[PASS]` or `[FAIL]`.** Check mapped Dock, exact dockChrome, Bar launcher/status, exact popup, Spotlight, AltTab scale, ContextMenu placement/cascades, Default/Transparent clears, and Frosted reapply.

- [ ] **Step 5: Run `rtk git diff --check`, inspect `rtk git status --short`, and commit only the lifecycle files; leave all unrelated pre-existing Eclipse edits unstaged.**

