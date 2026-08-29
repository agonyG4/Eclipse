# Dock Personalization v1 Correctness Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the physical-edge reveal, vertical centering, vertical drag coverage, deterministic geometry, and Settings architecture gaps without changing Dock Personalization v1 scope.

**Architecture:** Add a compositor-independent `DockSurfacePlacement` policy that combines stored configuration with the controller's runtime auto-hide state. `DockController` exposes the policy values; `AstreaShellApplication` passes the same values to Layer Shell; QML uses the same values for chrome inset, reveal target, fixed surface extent, input region, and context-menu output-local conversion. Bottom's normal geometry and exclusive-zone contract remain unchanged.

**Tech Stack:** C++20, Qt 6.6 Core/Gui/Quick/Test, Qt Quick/QML, LayerShellQt, CMake/CTest, qmllint.

## Global Constraints

- Preserve one resident Dock and one mapped Layer Shell surface.
- Keep Bottom resting geometry, magnification, drag, input-region, context-menu, icon-quality, and exclusive-zone behavior unchanged.
- Keep the existing fixed maximum surface envelope and bounded input-region guarantees.
- Keep C++ responsible for runtime state, persistence, and system access; QML remains presentation and interaction only.
- Do not add Quickshell, process execution, filesystem/JSON access from QML, IPC/DBus from QML, LayerShellQt/Hyprland/Typhon-private Settings dependencies, overlap detection, another sensor process, another resident Dock, or icon-pipeline rewrites.
- Use one derived placement policy; do not duplicate intelligent obstruction logic in QML or Layer Shell code.
- Verify every regression with a test that was observed failing before its production fix.

---

### Task 1: Add the shared runtime surface-placement policy

**Files:**
- Modify: `Dock/core/DockSurfaceGeometry.hpp`
- Modify: `Dock/core/DockSurfaceGeometry.cpp`
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Test: `Dock/tests/DockLayerShellSurfaceTest.cpp`
- Test: `Dock/tests/DockControllerTest.cpp`

**Interfaces:**
- Produces `DockSurfacePlacement { int layerShellEdgeMargin; int chromeEdgeInset; bool physicalEdgeReveal; }`.
- Produces `DockSurfaceGeometry::placementFor(const DockConfig &, bool autoHideActive)`.
- Produces `DockController::layerShellEdgeMargin()`, `chromeEdgeInset()`, and `physicalEdgeReveal()` read-only properties with `surfacePlacementChanged()`.

- [ ] **Step 1: Write failing policy tests**

Add focused cases to `DockLayerShellSurfaceTest` that call the policy for a floating configuration with `edgeMargin = 12`:

```cpp
void DockLayerShellSurfaceTest::placementPolicySeparatesPhysicalAndVisualMargins()
{
    DockConfig config = DockConfig::defaults();
    config.edgeMargin = 12;
    config.floating = true;

    const DockSurfacePlacement normal = DockSurfaceGeometry::placementFor(config, false);
    QCOMPARE(normal.layerShellEdgeMargin, 12);
    QCOMPARE(normal.chromeEdgeInset, 0);
    QVERIFY(!normal.physicalEdgeReveal);

    const DockSurfacePlacement hidden = DockSurfaceGeometry::placementFor(config, true);
    QCOMPARE(hidden.layerShellEdgeMargin, 0);
    QCOMPARE(hidden.chromeEdgeInset, 12);
    QVERIFY(hidden.physicalEdgeReveal);
}
```

Also assert attached mode stays zero/zero and non-floating auto-hide does not invent a gap. Add a controller test that applies intelligent mode, sends an active maximized Typhon snapshot, and observes exactly one `surfacePlacementChanged()` plus the existing reservation transition when obstruction appears and disappears.

- [ ] **Step 2: Run the focused tests and verify the expected failure**

Run:

```bash
cmake --build --preset debug --target dock-layer-shell-surface-test dock-controller-test
ctest --preset debug -R 'dock-layer-shell-surface-test|dock-controller-test' --output-on-failure
```

Expected: compilation fails because the placement type/API does not exist, or the new assertions fail against the current config-only Layer Shell behavior.

- [ ] **Step 3: Implement the minimal policy and controller properties**

Define the small value type beside `DockSurfaceGeometry`, derive it only from `DockConfig` plus the explicit runtime boolean, and compute:

```cpp
const int configuredMargin = config.effectiveEdgeMargin();
return autoHideActive
    ? DockSurfacePlacement{0, configuredMargin, true}
    : DockSurfacePlacement{configuredMargin, 0, false};
```

For attached mode `configuredMargin` is already zero. `DockController` must compare the old and new derived policy around `applyConfig()` and `updateAutoHidePolicy()`, emit `surfacePlacementChanged()` when any field changes, and retain `reservationChanged()` for exclusive-zone changes. Runtime Typhon transitions must therefore notify both policy and reservation consumers. Do not add an obstruction calculation outside `DockController::updateAutoHidePolicy()`.

- [ ] **Step 4: Run the focused tests and verify green**

Run the same focused build and CTest command. Expected: all new policy and controller transition assertions pass with existing Dock tests.

- [ ] **Step 5: Commit the policy boundary**

```bash
git add Dock/core/DockSurfaceGeometry.hpp Dock/core/DockSurfaceGeometry.cpp \
  Dock/core/DockController.hpp Dock/core/DockController.cpp \
  Dock/tests/DockLayerShellSurfaceTest.cpp Dock/tests/DockControllerTest.cpp
git commit -m "fix(dock): centralize runtime surface placement"
```

### Task 2: Apply the policy to Layer Shell, QML geometry, input, and context menus

**Files:**
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`
- Modify: `Shell/app/AstreaShellApplication.cpp`
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`
- Test: `Dock/tests/DockLayerShellSurfaceTest.cpp`
- Test: `Dock/tests/DockHoverQmlTest.cpp`
- Test: `Dock/tests/DockSurfaceGeometryTest.cpp`

**Interfaces:**
- `DockLayerShellSurface::configurationFor` and `configure` consume `DockSurfacePlacement` while preserving the existing overload for normal-mode callers/tests.
- QML reads `DockController.layerShellEdgeMargin`, `DockController.chromeEdgeInset`, and `DockController.physicalEdgeReveal`.
- Output-local context-menu conversion receives the actual Layer Shell margin, not the stored/effective margin when the surface is at the physical edge.

- [ ] **Step 1: Write failing deterministic placement and QML geometry tests**

Extend `DockLayerShellSurfaceTest` for Bottom/Left/Right with `edgeMargin = 12` and the same resting zone. Assert normal floating Layer Shell margin is 12, active auto-hide margin is 0, attached is 0, and `exclusiveZoneForMapping(false, resting)` plus temporary reveal stays zero through controller policy. Add QML assertions that an active auto-hide panel has its edge reveal target at x/y zero on the selected physical edge, chrome inset 12, and surface cross-axis size equal to the normal envelope plus exactly 12. Add context-menu geometry assertions for a right/left/bottom delegate using the actual runtime margin so no second 12px offset is applied.

- [ ] **Step 2: Run the new focused tests and verify the expected failure**

Run:

```bash
cmake --build --preset debug --target dock-layer-shell-surface-test dock-surface-geometry-test dock-hover-qml-test
ctest --preset debug -R 'dock-layer-shell-surface-test|dock-surface-geometry-test|dock-hover-qml-test' --output-on-failure
```

Expected: the new policy-to-surface and QML edge-coordinate assertions fail against the current config-only margin and edge-anchored chrome.

- [ ] **Step 3: Implement the one-surface physical-edge placement**

Pass the controller's policy into the Layer Shell configuration. Use `layerShellEdgeMargin` for the selected Layer Shell anchor only. Keep the exclusive zone from the controller and never derive it from the expanded surface extent.

In `DockPanel.qml`, add the policy values as derived read-only properties. Add `chromeEdgeInset` only to the cross-axis surface extent:

```qml
readonly property int surfaceCrossInset: DockController.chromeEdgeInset
readonly property int surfaceWidth: vertical
    ? Math.max(restingWidth + surfaceCrossInset,
               Math.ceil(restingWidth + surfaceHeadroom + surfaceCrossInset))
    : Math.max(restingWidth, Math.ceil(restingWidth + maximumMagnificationExtraPrimary))
readonly property int surfaceHeight: vertical
    ? Math.max(restingHeight, Math.ceil(restingHeight + maximumMagnificationExtraPrimary))
    : Math.max(restingHeight + surfaceCrossInset,
               Math.ceil(restingHeight + surfaceHeadroom + surfaceCrossInset))
```

Place vertical chrome at `x = chromeEdgeInset` for Left and `parent.width - width - chromeEdgeInset` for Right; place Bottom chrome at `parent.height - height - chromeEdgeInset`. Keep normal mode inset zero. Place the reveal target at the root edge (`x = 0`/`root.width - width`, `y = root.height - height`) only while `physicalEdgeReveal` is true; retain current bounded dimensions.

Update the input region from the same reveal/chrome rectangles. Keep all interaction rectangles finite and bounded. Pass `DockController.layerShellEdgeMargin` into `outputLocalDelegateRect`, so normal surfaces use their Layer Shell origin and auto-hide surfaces use zero. Do not change icon source properties, transforms, magnification radius, drag scaling, or the exclusive-zone value.

In `AstreaShellApplication`, connect `surfacePlacementChanged` to full Dock Layer Shell reconfiguration and preserve the reservation connection for the explicit exclusive-zone update. Ensure configuration uses the controller policy after controller state has settled, including intelligent Typhon transitions.

- [ ] **Step 4: Run the focused tests and verify green**

Run the focused build and CTest command again, then run:

```bash
qmllint Dock/qml/Main.qml Dock/qml/components/DockPanel.qml Dock/qml/components/DockAppDelegate.qml
```

Expected: all Bottom compatibility assertions, the three-edge policy geometry cases, context-menu conversion, input-region cases, and QML lint pass.

- [ ] **Step 5: Commit the placement integration**

```bash
git add Dock/platform/wayland/DockLayerShellSurface.hpp \
  Dock/platform/wayland/DockLayerShellSurface.cpp Shell/app/AstreaShellApplication.cpp \
  Dock/qml/Main.qml Dock/qml/components/DockPanel.qml \
  Dock/qml/components/DockAppDelegate.qml Dock/tests/DockLayerShellSurfaceTest.cpp \
  Dock/tests/DockHoverQmlTest.cpp Dock/tests/DockSurfaceGeometryTest.cpp
git commit -m "fix(dock): reveal floating auto-hide at physical edge"
```

### Task 3: Center vertical content through the axis model

**Files:**
- Modify: `Dock/qml/components/DockPanel.qml`
- Test: `Dock/tests/DockHoverQmlTest.cpp`
- Modify: `Dock/docs/ARCHITECTURE.md` if the geometry description needs its wording aligned

**Interfaces:**
- The resting grid remains driven by `restingCross`, `restingPrimary`, and delegate slot geometry.
- Bottom positioning is unchanged.

- [ ] **Step 1: Write the failing vertical-centering assertion**

In the existing vertical QML test, map the first/middle/last delegate icon centers and the chrome rectangle into panel coordinates. Assert the icon centers share the chrome cross-axis midpoint for both Left and Right, and assert their cross-axis distances from the corresponding chrome edge mirror across positions. Use the live mapped rectangles; do not assert arbitrary constants.

- [ ] **Step 2: Run the test and verify it fails against the current layout**

```bash
cmake --build --preset debug --target dock-hover-qml-test
ctest --preset debug -R dock-hover-qml-test --output-on-failure
```

Expected: the new vertical cross-axis midpoint/mirror assertion fails before the layout change.

- [ ] **Step 3: Implement axis-model centering**

Use a cross-axis centering expression for the vertical grid/content placement based on the chrome width and the grid's implicit cross extent. Keep the existing primary-axis `y` centering and Bottom `x`/`y` placement intact. Do not add Left/Right pixel constants. Re-run the existing indicator, transformed interaction-target, headroom, and fixed-envelope assertions to ensure indicator edge-side placement and magnification remain unchanged.

- [ ] **Step 4: Run the focused QML test and verify green**

```bash
cmake --build --preset debug --target dock-hover-qml-test
ctest --preset debug -R dock-hover-qml-test --output-on-failure
```

- [ ] **Step 5: Commit vertical centering**

```bash
git add Dock/qml/components/DockPanel.qml Dock/tests/DockHoverQmlTest.cpp Dock/docs/ARCHITECTURE.md
git commit -m "fix(dock): center vertical resting content"
```

### Task 4: Add real Left/Right drag and reorder regression coverage

**Files:**
- Modify: `Dock/tests/DockHoverQmlTest.cpp`
- Modify: `Dock/docs/TESTING.md`

**Interfaces:**
- Exercise the existing `beginReorder`, `updateReorder`, `finishReorder`, and `cancelReorder` QML behavior through the real `DockPanel` object and live delegates.
- Observe `reorderRequested(QString,int)`, `activated(QString)`, delegate transform properties, panel dimensions, and `DockInputRegionBridge` updates.

- [ ] **Step 1: Write focused failing Left/Right behavior coverage**

Add a parameterized test helper for both positions with these real assertions:

```cpp
const QPointF sourceCenter = source->mapToItem(panel,
                                               source->width() / 2.0,
                                               source->height() / 2.0);
QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                  Q_ARG(QVariant, QVariant(source->objectName()))));
QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                  Q_ARG(QVariant, QVariant(source->objectName())),
                                  Q_ARG(QVariant, QVariant(primaryDelta)),
                                  Q_ARG(QVariant, QVariant(sourceCenter.x())),
                                  Q_ARG(QVariant, QVariant(sourceCenter.y()))));
```

Cover first-to-last and last-to-first positive/negative Y deltas, `dragOriginCenterRelativeX` from the source Y center, neighbor `visualOffsetY` preview displacement, inward dragged `visualOffsetX` for Left/Right, magnification scale equal to 1 during drag, one identity-based finish request, no request on cancel, zero activation signals, unchanged panel width/height, unchanged captured origin after collapse, and an input region containing only bounded interaction/chrome rectangles. Use the real QML object properties and bridge update calls, not helper-only arithmetic.

- [ ] **Step 2: Run the new tests and verify the expected failure**

```bash
cmake --build --preset debug --target dock-hover-qml-test
ctest --preset debug -R dock-hover-qml-test --output-on-failure
```

Expected: at least one vertical Y-primary/neighbor/inward-lift assertion fails or is absent under the current incomplete coverage.

- [ ] **Step 3: Correct only the QML behavior exposed by the tests**

If a failure is behavioral rather than coverage-only, keep the existing primary/cross-axis model: `translationY` drives reorder targets for vertical positions, `crossOffset()` drives inward lift, drag state forces magnification to 1, and finish/cancel retain the current identity and activation guards. Preserve the captured center relative to the panel center and the fixed surface envelope. Avoid changing Bottom paths.

- [ ] **Step 4: Run all Dock QML regressions and update documentation**

```bash
cmake --build --preset debug --target dock-hover-qml-test
ctest --preset debug -R dock-hover-qml-test --output-on-failure
```

Update `Dock/docs/TESTING.md` to state explicitly that deterministic offscreen tests prove QML arbitration, geometry, and input-mask calculations, while real Wayland is still required for compositor pointer delivery at physical edges, floating-gap traversal, underlying-application click-through, and visual feel.

- [ ] **Step 5: Commit the vertical drag coverage**

```bash
git add Dock/tests/DockHoverQmlTest.cpp Dock/docs/TESTING.md
git commit -m "test(dock): cover vertical reorder behavior"
```

### Task 5: Extend Settings architecture guards and reuse invariants

**Files:**
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`
- Test: `Settings/tests/CMakeLists.txt` only if the static test needs target wiring (otherwise leave unchanged)

**Interfaces:**
- `production_source_files` includes `services/dock/SettingsDockController.cpp`, `services/dock/SettingsDockController.hpp`, and `qml/pages/appearance/Dock.qml` through the existing source-reading guard.
- `core_production_cpp_files` includes `services/dock/SettingsDockController.cpp` so tests cannot compile it directly instead of linking `astrea-settings-core`.

- [ ] **Step 1: Write failing structural assertions**

Extend the CMake test's expected production list and reuse list, then run the static test against the current source. The test must fail before the change if the new boundary is not covered by the guard. Include forbidden tokens for Quickshell, process execution, filesystem/QML access, IPC, DBus, LayerShellQt, LayerShell helpers, Hyprland, and Typhon-private APIs in the same existing loop; use the existing source read path for both C++ and QML.

- [ ] **Step 2: Run the static test and verify the expected failure**

```bash
cmake --build --preset debug --target settings-structure-test
ctest --preset debug -R settings-structure-test --output-on-failure
```

Expected: the command reports the newly required source boundary/reuse invariant is not present until the guard is updated.

- [ ] **Step 3: Implement the guard extension**

Add the two Dock controller files and `qml/pages/appearance/Dock.qml` to `production_source_files`; add the controller `.cpp` to `core_production_cpp_files`; expand forbidden tokens only with exact boundary tokens needed to catch QML filesystem access, IPC/DBus, LayerShell, Quickshell, process execution, Hyprland, and Typhon-private references. Do not weaken existing guards or add a Settings-to-Shell route.

- [ ] **Step 4: Run Settings focused tests and verify green**

```bash
ctest --preset debug -R 'settings-structure-test|settings-dock-controller-test|settings-qml-smoke-test' --output-on-failure
```

- [ ] **Step 5: Commit the Settings guard**

```bash
git add Settings/tests/static/SettingsStructureTest.cmake
git commit -m "test(settings): guard Dock personalization boundary"
```

### Task 6: Full verification, live qualification, and final commit

**Files:**
- Modify only if verification reveals an in-scope defect: files from Tasks 1–5
- Verify: `Dock/docs/TESTING.md`, final CMake/CTest configuration, and worktree

- [ ] **Step 1: Run focused deterministic verification in Debug**

```bash
cmake --build --preset debug
ctest --preset debug --output-on-failure
qmllint Dock/qml/Main.qml \
  Dock/qml/components/DockPanel.qml \
  Dock/qml/components/DockAppDelegate.qml
```

- [ ] **Step 2: Run the complete Release verification**

```bash
cmake --build --preset release
ctest --preset release --output-on-failure
```

- [ ] **Step 3: Run repository hygiene checks**

```bash
git diff --check
git status --short
```

Confirm only the intended Dock/Settings/docs files are in the worktree; preserve unrelated pre-existing user changes and do not stage them.

- [ ] **Step 4: Perform live Typhon/Wayland qualification**

With the production LayerShellQt-enabled shell running, manually record pass/fail for Bottom/Left/Right Floating+Always physical-edge reveal, retained 12px visual gap, intelligent obstruction hide/restore, pointer traversal through the gap, vertical centering symmetry, both vertical reorder directions, context-menu anchoring, and Bottom magnification/icon sharpness. Record any environment blocker explicitly; never infer these results from direct controller calls or offscreen `setPointerInside()`.

- [ ] **Step 5: Commit the verified closure**

```bash
git add docs/superpowers/plans/2026-08-29-dock-personalization-v1-correctness-closure.md \
  Dock Settings Shell docs
git commit -m "fix(dock): close personalization v1 correctness gaps"
```

Report root causes, the shared placement policy, exact geometry/state changes, tests added, Debug/Release results, live Wayland results per case, and final `git status --short`.
