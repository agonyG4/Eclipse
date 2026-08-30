# Dock Placement Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make runtime Dock position changes part of the derived Layer Shell placement identity so every edge transition reconfigures the resident surface exactly once.

**Architecture:** Extend `DockSurfacePlacement` with the canonical Dock position string. `DockSurfaceGeometry::placementFor()` derives position and physical/visual margin policy together; `DockController` compares the complete snapshot and keeps `reservationChanged()` separate. `DockLayerShellSurface` consumes that same snapshot for both anchors and margins.

**Tech Stack:** C++20, Qt 6, Qt Test, CMake, LayerShellQt integration.

## Global Constraints

- Preserve the one resident Dock surface and existing physical-edge reveal architecture.
- Preserve Bottom geometry, exclusive-zone behavior, QML interaction behavior, context menus, and icon resolution.
- Do not restore generic `configChanged()` to Layer Shell reconfiguration.
- Keep canonical configuration strings `bottom`, `left`, and `right` at the JSON boundary.
- Do not modify unrelated Shell components or add new IPC, processes, themes, blur, shaders, or opacity controls.

### Task 1: Add runtime placement identity regression coverage

**Files:**
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockLayerShellSurfaceTest.cpp`

**Interfaces:**
- Consumes: `DockController::applyConfig()`, `surfacePlacementChanged()`, `DockSurfaceGeometry::placementFor()`, and `DockLayerShellSurface::configurationFor()`.
- Produces: deterministic runtime transition tests covering normal, Always auto-hide, Intelligent-obstructed, and visual-only changes.

- [ ] **Step 1: Write the failing controller test**

Add a test that applies Bottom, spies on `surfacePlacementChanged`, then applies otherwise identical Left, Right, and Bottom configurations. Assert one signal per transition and assert the snapshot position. Repeat the edge transitions with Always auto-hide and an Intelligent snapshot containing an active maximized toplevel. Add a visual-only config mutation and assert no placement signal.

- [ ] **Step 2: Run the controller test to verify the expected failure**

Run:

```bash
cmake --build --preset debug --target dock-controller-test
ctest --preset debug -R '^dock-controller-test$' --output-on-failure
```

Expected: the new position assertions fail because Bottom/Left/Right currently produce equal placement snapshots.

- [ ] **Step 3: Add the Layer Shell snapshot regression**

Add a focused test that derives placements for Bottom, Left, and Right and passes each placement snapshot to `DockLayerShellSurface::configurationFor()`. Assert only the selected edge is anchored and receives the physical margin, including zero margin for active auto-hide.

- [ ] **Step 4: Run the Layer Shell test to establish its pre-fix result**

Run:

```bash
cmake --build --preset debug --target dock-layer-shell-surface-test
ctest --preset debug -R '^dock-layer-shell-surface-test$' --output-on-failure
```

Expected: the new snapshot-anchor assertions fail to compile or fail until the consumer accepts the placement identity.

### Task 2: Extend and consume the complete placement snapshot

**Files:**
- Modify: `Dock/core/DockSurfaceGeometry.hpp`
- Modify: `Dock/core/DockSurfaceGeometry.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`

**Interfaces:**
- Consumes: canonical `DockConfig::position` values and existing placement margin policy.
- Produces: `DockSurfacePlacement::position` and a `configurationFor()` overload that uses one coherent placement snapshot for anchors and margins.

- [ ] **Step 1: Add the position field and derive it in `placementFor()`**

Preserve the existing aggregate equality and return the normalized position string alongside the existing margin/inset/reveal values.

- [ ] **Step 2: Make Layer Shell configuration use placement position**

Have the placement-aware overload choose Bottom/Left/Right anchors from `placement.position` and apply `placement.layerShellEdgeMargin` only on that selected edge. Keep the legacy config-only overload as a compatibility wrapper that derives the normal placement snapshot.

- [ ] **Step 3: Run focused tests to verify the implementation**

Run:

```bash
cmake --build --preset debug --target dock-controller-test dock-layer-shell-surface-test
ctest --preset debug -R '^dock-controller-test$|^dock-layer-shell-surface-test$' --output-on-failure
```

Expected: all new transition, negative, and anchor tests pass.

### Task 3: Verify the shell signal contract and complete project verification

**Files:**
- Inspect only: `Shell/app/AstreaShellApplication.cpp`
- Modify only if required: no generic-signal fallback is permitted.

**Interfaces:**
- Consumes: existing `surfacePlacementChanged()` connection and `DockController::surfacePlacement()` snapshot.
- Produces: evidence that visual settings do not trigger Layer Shell reconfiguration while placement changes do.

- [ ] **Step 1: Confirm the dedicated shell connection remains intact**

Verify the application still connects `surfacePlacementChanged()` directly to `configureDockSurface()` and that `reservationChanged()` remains the exclusive-zone-only path.

- [ ] **Step 2: Run the requested focused and full verification**

Run the focused Dock targets, then:

```bash
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset release
ctest --preset release --output-on-failure
git diff --check
git status --short
```

Record any unrelated pre-existing failures without modifying them.

- [ ] **Step 3: Commit only the bounded fix and its tests/plan**

```bash
git add Dock/core/DockSurfaceGeometry.hpp Dock/core/DockSurfaceGeometry.cpp \
  Dock/platform/wayland/DockLayerShellSurface.hpp \
  Dock/platform/wayland/DockLayerShellSurface.cpp \
  Dock/tests/DockControllerTest.cpp Dock/tests/DockLayerShellSurfaceTest.cpp \
  docs/superpowers/plans/2026-08-29-dock-placement-identity.md
git commit -m "fix(dock): include position in placement identity"
```

## Self-Review

- Position transitions are covered through the runtime `applyConfig()` notification path, not only independent static configurations.
- Always and Intelligent-obstructed placement snapshots retain zero physical margin, configured chrome inset, and physical-edge reveal.
- Purely visual configuration changes remain excluded from `surfacePlacementChanged()`.
- Layer Shell anchors and margins come from one placement snapshot.
- Bottom compatibility and the existing dedicated shell signal are preserved.
