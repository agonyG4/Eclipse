# AstreaOS Global Context Menu Correctness Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct the existing Eclipse global Context Menu integration so Tray actions, output mapping, Dock coordinates, teardown, nested focus, animation, and behavioral tests are production-ready without changing Typhon.

**Architecture:** Keep `ContextMenuController` as the single lifecycle owner and add only a per-presentation action authorizer for live Tray nodes. Keep output mapping in `ContextMenuSurfaceBundle`, Dock coordinate conversion in a pure geometry helper, and QML limited to presentation/input forwarding. Remove the obsolete production Tray path from `BarPopupController` while reusing its DBusMenu renderer in the global Overlay.

**Tech Stack:** C++20, Qt 6 Core/Gui/Quick/Test, Qt QML, LayerShellQt, existing DBusMenu/StatusNotifier models, CMake/CTest.

## Global Constraints

- Preserve generation-based stale activation protection and target validation for every activation.
- Preserve bounded Context Menu model depth (8) and node count (256), separator normalization, provider-driven actions, exact Typhon `WindowId` operations, and atomic Dock persistence.
- Paper remains `WindowTransparentForInput`; no wallpaper pointer ownership is added.
- Do not add an Astrea-specific protocol or modify Typhon without an independently failing generic compositor regression.
- Do not leave an always-mapped Overlay, reserve workspace space for context menus, or retain duplicate production Tray lifecycle ownership.
- Use `apply_patch` for edits, `rtk` for shell inspection/build/test commands, and make no changes to the pre-existing unrelated dirty files.

### Task 1: Authorize live Tray DBusMenu actions

**Files:**
- Modify: `Shell/core/ContextMenuController.hpp`
- Modify: `Shell/core/ContextMenuController.cpp`
- Modify: `Shell/core/ContextMenuProviders.hpp`
- Modify: `Shell/core/ContextMenuProviders.cpp`
- Modify: `Shell/tests/ContextMenuTest.cpp`
- Modify: `Shell/CMakeLists.txt`

**Interfaces:**
- Add `using ActionAuthorizer = std::function<bool(const QString &token)>;`.
- Extend the presentation overload to accept an optional authorizer after `TargetValidator`.
- `activate(generation, token)` checks lifecycle, generation, target validator, authorizer/model authorization, then dispatches.
- Tray passes an authorizer that validates `tray.node.<positive integer>` against the current `DBusMenuModel` node.

- [ ] **Step 1: Write the failing tests**

Add a controller test that presents a Tray target with an empty generic model and a live authorizer, then asserts `activate(currentGeneration, "tray.node.7")` dispatches. Add rejection assertions for a stale generation, missing model, removed node, invisible/enabled-false node, separator, and submenu node. Keep a target-removal test proving item invalidation rejects activation.

```cpp
QVERIFY(controller.present(trayTarget, pointAnchor(QPoint(10, 10)), {},
    [&](const QString &token) { activatedToken = token; return true; },
    [] { return true; },
    [&] { return liveTrayNode; }));
QVERIFY(controller.activate(controller.presentationGeneration(), QStringLiteral("tray.node.7")));
QCOMPARE(activatedToken, QStringLiteral("tray.node.7"));
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R '^context-menu-test$'
```

Expected: the new Tray activation test fails because the current controller always calls `ContextMenuModel::canActivate()` and the presented Tray model is empty.

- [ ] **Step 3: Implement the minimum authorization split**

Store the optional authorizer in the controller. In `activate()`, use it when present; otherwise retain `m_model->canActivate(token)`. In `TrayContextMenuAdapter::present()`, pass the live authorizer. Resolve the node with `menuModelForItem()`, require a current model, positive numeric ID, visible/enabled non-separator leaf, and call `DBusMenuModel::activate(nodeId)` only after all checks pass. Keep `prepareMenuForPresentation()` and the existing target validator unchanged.

- [ ] **Step 4: Run the focused test to verify it passes**

Run the same `ctest` command. Expected: PASS, including all existing static-menu lifecycle tests.

- [ ] **Step 5: Commit**

```bash
rtk git add Shell/core/ContextMenuController.hpp Shell/core/ContextMenuController.cpp \
  Shell/core/ContextMenuProviders.hpp Shell/core/ContextMenuProviders.cpp \
  Shell/tests/ContextMenuTest.cpp Shell/CMakeLists.txt
rtk git commit -m "fix(shell): authorize live tray context actions"
```

### Task 2: Make Overlay mapping output-scoped and teardown synchronous

**Files:**
- Modify: `Shell/platform/wayland/ContextMenuSurfaceBundle.hpp`
- Modify: `Shell/platform/wayland/ContextMenuSurfaceBundle.cpp`
- Modify: `Shell/platform/wayland/ContextMenuSurfaceManager.cpp`
- Modify: `Shell/tests/ContextMenuTest.cpp`
- Create: `Shell/tests/ContextMenuSurfaceManagerTest.cpp`
- Modify: `Shell/CMakeLists.txt`

**Interfaces:**
- Add a bundle-level predicate equivalent to `shouldMapOverlay()` using bundle output key, controller active state, and controller output key.
- Add a synchronous controller settlement path used only when an output is removed or the shell shuts down.
- Keep normal user close animation asynchronous; output destruction must not depend on QML callbacks.

- [ ] **Step 1: Write failing mapping and removal tests**

Add a deterministic mapping test with two fake output records (`output-A`, `output-B`) and a controller. Assert only the matching output maps, replacement switches mapping, close clears both, and unrelated removal leaves the presentation open. Add active-output removal and remove-active-last-output/add-output-C tests asserting `Closed`, cleared model/target, and no inherited mapping.

```cpp
QVERIFY(controller.present(targetFor(QStringLiteral("output-A")), nodes, handler));
QVERIFY(bundleMapping(QStringLiteral("output-A"), true, controller));
QVERIFY(!bundleMapping(QStringLiteral("output-B"), true, controller));
controller.invalidateOutput(QStringLiteral("output-A"));
controller.completeClose();
QCOMPARE(controller.lifecycle(), ContextMenuController::Lifecycle::Closed);
```

- [ ] **Step 2: Run the new tests to verify they fail**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'context-menu-(test|surface-manager-test)'
```

Expected: the mapping test exposes that every bundle currently maps for any active presentation, and active-output removal can leave `Closing`.

- [ ] **Step 3: Implement output ownership and synchronous settlement**

Change `syncOverlayMapping()` to require `m_controller->outputKey() == outputKey()`. Add the smallest pure/static mapping predicate needed for deterministic tests. Make `invalidateOutput()` close and synchronously complete the lifecycle when the active target matches that output; retain idempotence for already closed state. Ensure manager removal calls invalidation before erasing/deleting the bundle. Ensure shutdown settles before destroying bundles.

- [ ] **Step 4: Run the focused tests to verify they pass**

Run the same focused command. Expected: PASS, including existing surface policy and controller tests.

- [ ] **Step 5: Commit**

```bash
rtk git add Shell/platform/wayland/ContextMenuSurfaceBundle.hpp \
  Shell/platform/wayland/ContextMenuSurfaceBundle.cpp \
  Shell/platform/wayland/ContextMenuSurfaceManager.cpp \
  Shell/tests/ContextMenuTest.cpp Shell/tests/ContextMenuSurfaceManagerTest.cpp \
  Shell/CMakeLists.txt
rtk git commit -m "fix(shell): scope context overlays to their output"
```

### Task 3: Add tested Dock output-local geometry

**Files:**
- Create: `Dock/core/DockSurfaceGeometry.hpp`
- Create: `Dock/core/DockSurfaceGeometry.cpp`
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`
- Create: `Dock/tests/DockSurfaceGeometryTest.cpp`
- Modify: `Dock/CMakeLists.txt`
- Modify: `Shell/CMakeLists.txt`

**Interfaces:**
- Add `DockSurfaceGeometry::delegateRectInOutput(const QSize &outputSize, const QSize &surfaceSize, int bottomMargin, const QRectF &delegateRect)` returning a `QRect` in output-local coordinates.
- The helper uses centered horizontal placement and bottom anchoring; it never consumes virtual desktop origin values.
- QML passes output size, current Dock surface size, bottom margin, and the delegate’s Dock-surface-local rectangle.

- [ ] **Step 1: Write failing pure geometry tests**

Cover centered 1920×1080, ultrawide, non-zero virtual origin ignored, magnified surface width, first/middle/last delegates, and bottom margins 0/12/48. Assert the returned rectangle remains inside output bounds where the delegate is valid and changes only with surface/layout geometry.

```cpp
QCOMPARE(DockSurfaceGeometry::delegateRectInOutput(
    QSize(1920, 1080), QSize(600, 84), 12, QRectF(14, 35, 48, 48)),
    QRect(674, 984, 48, 48));
```

- [ ] **Step 2: Run the geometry test to verify it fails**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R '^dock-surface-geometry-test$'
```

Expected: the target is absent or the new expected helper result is unavailable.

- [ ] **Step 3: Implement the helper and wire QML**

Compute the centered surface origin as `(outputWidth - surfaceWidth) / 2` horizontally and `outputHeight - bottomMargin - surfaceHeight` vertically. Map the delegate rectangle through that origin and round only at the C++ boundary. Expose the current `window.width/height`, `DockController.bottomMargin`, and `root.mapToItem(root, ...)` delegate-local rectangle; remove `mapToItem(null)` and both `outputOrigin` subtraction operations. Keep normal left click, drag, hover, and magnification behavior unchanged.

- [ ] **Step 4: Run the geometry and Dock interaction tests**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'dock-(surface-geometry|controller|app-model|config-persistence)-test|context-menu-qml-interaction-test'
```

Expected: PASS, with no right-click launch or left-click activation regression.

- [ ] **Step 5: Commit**

```bash
rtk git add Dock/core/DockSurfaceGeometry.hpp Dock/core/DockSurfaceGeometry.cpp \
  Dock/qml/Main.qml Dock/qml/components/DockPanel.qml Dock/qml/components/DockAppDelegate.qml \
  Dock/tests/DockSurfaceGeometryTest.cpp Dock/CMakeLists.txt Shell/CMakeLists.txt
rtk git commit -m "fix(dock): use output-local context menu geometry"
```

### Task 4: Complete generic and Tray submenu focus/placement behavior

**Files:**
- Modify: `Shell/qml/contextmenu/ContextMenuView.qml`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Modify: `Shell/tests/ContextMenuQmlSmokeTest.cpp`
- Create or modify: `Shell/tests/ContextMenuQmlInteractionTest.cpp`
- Modify: `Shell/CMakeLists.txt`

**Interfaces:**
- Generic child views call their parent’s focus restoration callback after closing.
- Each view exposes the selected delegate’s mapped rectangle in overlay-local coordinates.
- Keyboard selection remains one active-item state per view; Left closes one child, Escape closes the controller.

- [ ] **Step 1: Write failing QML interaction tests**

Instantiate the real QML components with a controller and model. Send key events for first selection, Up/Down, Home/End, Enter, Space, disabled/separator skipping, Right submenu open, child Up/Down, Left focus restoration, and Escape. Add outside left/right mouse presses and an inside menu press; assert the controller closes only for outside/Escape and activation occurs only for enabled leaf items. Add a tall root menu, keyboard-scroll it, open a submenu, and assert its anchor follows the visible delegate rectangle.

- [ ] **Step 2: Run the interaction test to verify it fails**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R '^context-menu-qml-interaction-test$'
```

Expected: the child Left path does not return focus to the parent and the scrolled submenu anchor differs from the selected row’s actual position.

- [ ] **Step 3: Implement focus and mapped-row geometry**

Give each menu view a `restoreFocus()` function that calls `forceActiveFocus()` and reselects the parent submenu row. On child close, invoke it before/after deactivating the loader as appropriate. Replace `root.y + root.activeRowY` with `list.currentItem.mapToItem(root.parent, 0, 0)` and use the resulting rectangle for `submenuPosition()`. Guard null/unmapped delegates and keep recursive parent links for nested menus. Add `enabled: false` during controller Closing and ensure shield/card event ordering remains unchanged.

- [ ] **Step 4: Run the interaction and existing QML tests**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'context-menu-qml-(smoke|interaction)-test|bar-qml-smoke-test'
```

Expected: PASS with existing Tray DBusMenu rendering tests intact.

- [ ] **Step 5: Commit**

```bash
rtk git add Shell/qml/contextmenu/ContextMenuView.qml Bar/qml/TrayContextMenu.qml \
  Bar/qml/TrayMenuCard.qml Shell/tests/ContextMenuQmlSmokeTest.cpp \
  Shell/tests/ContextMenuQmlInteractionTest.cpp Shell/CMakeLists.txt
rtk git commit -m "fix(shell): restore nested context menu focus"
```

### Task 5: Make Context Menu close transitions authoritative

**Files:**
- Modify: `Shell/qml/contextmenu/ContextMenuOverlaySurface.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuView.qml`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Modify: `Shell/tests/ContextMenuQmlInteractionTest.cpp`

**Interfaces:**
- The Overlay remains mapped during a normal exit only while its visual exit animation runs.
- `completeClose()` is called from animation completion, not a fixed timer.
- Closing disables menu delegates and rejects all activations through the controller.

- [ ] **Step 1: Add failing transition assertions**

Assert that opening starts at hidden opacity/scale and reaches visible state without blocking input initialization, that `close()` immediately disables menu interaction while retaining the card for the exit, and that animation completion calls `completeClose()`. Assert output removal still closes synchronously without QML animation.

- [ ] **Step 2: Run the transition test to verify it fails**

Run the focused QML interaction target. Expected: the current fixed timer controls completion and the generic card remains visually unchanged during Closing.

- [ ] **Step 3: Implement enter/exit animations**

Wrap generic and Tray content in an animated presentation item. On Open, set opacity/scale to `0/0.97` and start a short `ParallelAnimation`; on Closing, set `enabled` false, animate to `0/0.97`, and invoke `completeClose()` in `onFinished` only if the same closing presentation is still active. Remove `finishCloseTimer`. Keep the overlay hidden after Closed and ensure stale callbacks are harmless through controller generation/lifecycle checks. Use existing theme animation durations; do not add a reduced-motion subsystem.

- [ ] **Step 4: Run QML lifecycle tests**

Run the focused Context Menu QML targets and existing Bar QML smoke target. Expected: PASS and no invisible Overlay remains mapped.

- [ ] **Step 5: Commit**

```bash
rtk git add Shell/qml/contextmenu/ContextMenuOverlaySurface.qml \
  Shell/qml/contextmenu/ContextMenuView.qml Bar/qml/TrayContextMenu.qml \
  Bar/qml/TrayMenuCard.qml Shell/tests/ContextMenuQmlInteractionTest.cpp
rtk git commit -m "fix(shell): animate and settle context menu closing"
```

### Task 6: Remove obsolete production Tray ownership and share only generic theme tokens

**Files:**
- Modify: `Bar/core/BarPopupController.hpp`
- Modify: `Bar/core/BarPopupController.cpp`
- Modify: `Bar/platform/wayland/BarSurfaceBundle.cpp`
- Modify: `Bar/platform/wayland/BarSurfaceManager.cpp`
- Modify: `Bar/qml/Tray.qml`
- Modify: `Bar/qml/PopupOverlaySurface.qml`
- Modify: `Bar/qml/StatusSurface.qml`
- Modify: `Bar/tests/BarCoreTest.cpp`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Create: `Shell/qml/components/ShellMenuTheme.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuCard.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuItem.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuSeparator.qml`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Modify relevant QML resource lists in `Shell/CMakeLists.txt`

**Interfaces:**
- `BarPopupController::PopupKind` contains only `None`, `AstreaMenu`, `Network`, `Bluetooth`, and `Volume`.
- `toggleTrayMenu()`, Tray popup branches, and compatibility-only production fallback are removed.
- `ShellMenuTheme.qml` exposes only the generic colors, typography, spacing, radius, opacity, and animation tokens used by the shared menu primitives; it reads the existing `ThemeController` state and does not duplicate the Bar-specific surface structure.

- [ ] **Step 1: Update failing ownership tests**

Change Bar tests to assert Tray is not a Bar popup and add a global Tray presentation test that finds the DBusMenu renderer under the Context Menu Overlay. Assert Tray left/middle semantics remain unchanged and remote fallback still calls the existing service path when the global controller is unavailable only in isolated compatibility test setup.

- [ ] **Step 2: Run the ownership tests to verify they fail**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'bar-(core|qml-smoke)-test|context-menu-qml-interaction-test'
```

Expected: the current Bar controller still exposes `TrayMenu` and the Bar overlay still constructs the Tray lifecycle.

- [ ] **Step 3: Remove duplicate production lifecycle and update theme imports**

Delete the Tray enum/method/support branch and remove Tray from `PopupOverlaySurface.qml`, its `popupForKind()` switch, click shield decisions, and Bar lifecycle assertions. Route both Tray right-click and menu-capable left-click through `ContextMenuController::presentTray()`; retain service fallback only for the explicitly unsupported controller case. Point generic Shell and retained Tray renderer primitives at `ShellMenuTheme.qml`, preserving all existing token values and `ShellBarTheme` behavior for unrelated Bar components.

- [ ] **Step 4: Run the ownership and DBusMenu regression tests**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'bar-(core|qml-smoke)-test|statusnotifier(-dbus-integration)?-test|context-menu-(test|qml-interaction)-test'
```

Expected: PASS with live revisions, `aboutToShow`, nesting, check/radio state, icons, separators, visibility/enabled state, and fallback preserved.

- [ ] **Step 5: Commit**

```bash
rtk git add Bar/core/BarPopupController.hpp Bar/core/BarPopupController.cpp \
  Bar/platform/wayland/BarSurfaceBundle.cpp Bar/platform/wayland/BarSurfaceManager.cpp \
  Bar/qml/Tray.qml Bar/qml/PopupOverlaySurface.qml Bar/qml/StatusSurface.qml \
  Bar/tests/BarCoreTest.cpp Bar/tests/BarQmlSmokeTest.cpp \
  Shell/qml/components/ShellMenuTheme.qml Shell/qml/contextmenu \
  Bar/qml/TrayContextMenu.qml Bar/qml/TrayMenuCard.qml Shell/CMakeLists.txt
rtk git commit -m "refactor(shell): make global controller own tray menus"
```

### Task 7: Expand behavioral regression coverage and documentation

**Files:**
- Modify: `Shell/tests/ContextMenuTest.cpp`
- Modify: `Shell/tests/ContextMenuQmlInteractionTest.cpp`
- Modify: `Shell/tests/ContextMenuSurfaceManagerTest.cpp`
- Modify: `Dock/tests/DockSurfaceGeometryTest.cpp`
- Modify: `Dock/docs/TESTING.md`
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Shell/CMakeLists.txt`
- Modify: `Dock/CMakeLists.txt`

- [ ] **Step 1: Add remaining behavioral assertions**

Cover controller replacement, stale generation after replacement, disabled/hidden static action rejection, target invalidation, shutdown cleanup, Dock right-click not launching, stable `desktopFileName`, exact output-local geometry, Tray real leaf activation, disabled/submenu rejection, item removal invalidation, only-target-output mapping, unrelated output removal, and stale Closing output recreation.

- [ ] **Step 2: Run all new focused tests**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure -R 'context-menu|dock-surface-geometry|dock-controller|dock-config-persistence|desktop-entry-catalog|shell-runtime|statusnotifier|bar-(core|qml-smoke)'
```

Expected: all relevant targets pass.

- [ ] **Step 3: Update documentation contracts**

Document that Dock pin/unpin persistence is controller-owned and atomic, that Dock context-menu coordinates are output-local, and that Tray context-menu ownership belongs to `ContextMenuController`. Do not claim Typhon runtime qualification.

- [ ] **Step 4: Commit**

```bash
rtk git add Shell/tests Dock/tests Dock/docs Shell/CMakeLists.txt Dock/CMakeLists.txt
rtk git commit -m "test(shell): close context menu integration gaps"
```

### Task 8: Full verification and fresh source review

**Files:**
- No production edits planned; review all closure files and the final diff.

- [ ] **Step 1: Run the configured build**

```bash
CMAKE_PREFIX_PATH=/home/agony/.local/opt/astrea-layer-shell-qt-6.7.4-1.1/usr \
LD_LIBRARY_PATH=/home/agony/.local/opt/astrea-layer-shell-qt-6.7.4-1.1/usr/lib:${LD_LIBRARY_PATH:-} \
rtk cmake --build --preset debug
```

- [ ] **Step 2: Run the full test suite**

```bash
TMPDIR=/tmp \
LD_LIBRARY_PATH=/home/agony/.local/opt/astrea-layer-shell-qt-6.7.4-1.1/usr/lib:${LD_LIBRARY_PATH:-} \
rtk ctest --preset debug --output-on-failure
```

- [ ] **Step 3: Run the QML gate and whitespace check**

```bash
CMAKE_PREFIX_PATH=/home/agony/.local/opt/astrea-layer-shell-qt-6.7.4-1.1/usr \
LD_LIBRARY_PATH=/home/agony/.local/opt/astrea-layer-shell-qt-6.7.4-1.1/usr/lib:${LD_LIBRARY_PATH:-} \
rtk tools/ci/run-qml-gate.sh
rtk git diff --check
```

- [ ] **Step 4: Perform the fresh source review**

Check specifically that one output alone maps the Overlay, no lifecycle completion depends on a destroyed QML object, Dock never reconstructs global coordinates, Tray dynamic actions bypass only static-model authorization while retaining target/generation checks, Bar has no Tray production lifecycle, nested Left restores focus, and no Closing/stale Overlay remains mapped.

- [ ] **Step 5: Commit only if review finds a required correction**

Use a focused fix commit naming the corrected invariant. Do not stage or alter pre-existing unrelated working-tree files.
