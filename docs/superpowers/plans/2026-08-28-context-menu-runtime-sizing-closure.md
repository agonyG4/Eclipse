# AstreaOS Context Menu Runtime Sizing Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the Eclipse global Context Menu runtime geometry defects by making generic sizing model-driven, bounded, output-local, and observable without changing Typhon or layer-shell policy.

**Architecture:** Keep `ContextMenuController` and `ContextMenuSurfaceBundle` as the lifecycle and output owners. Add model-side exact content metrics and a single `ShellMenuTheme.qml` metric contract; `ContextMenuView` resolves one natural-to-bounded size, while Overlay and submenu placement consume that final size. Desktop diagnostics are added at the existing output-local input boundary and the existing Overlay boundary.

**Tech Stack:** C++20, Qt 6 Core/Gui/Quick/Test, QML, CMake/Ninja, LayerShellQt, `rtk` command proxy.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse`; do not modify `/home/agony/GitHub/Typhon`.
- Preserve `ContextMenuController`, `ContextMenuModel`, `ContextMenuPlacement`, `ContextMenuSurfaceBundle`, `DesktopInteractionSurface`, global Overlay mapping, generation protection, output-scoped mapping, Dock/Tray functionality, keyboard navigation, submenu lifecycle, Paper transparency, and Typhon layer-shell configuration.
- Keep generic Context Menu minimum width at 200 logical px and maximum width at 260 logical px; use actual Astrea font metrics for natural width.
- Make exact visible model height the intrinsic height authority: normal rows 36 px, separator rows 10 px, hidden rows zero, plus top/bottom card padding.
- Resolve final size before `ContextMenuPlacement`; cap with output edge margin and make tall menus scrollable.
- Do not add arbitrary offsets, guessed scale conversions, dummy Desktop actions, or unrelated Dock/Bar/hover/reorder/shell changes.
- Preserve pre-existing unrelated dirty worktree files and stage only files changed by this closure.
- Use `apply_patch` for edits and `rtk` for repository inspection, build, test, and git commands.

---

## File map

- `Shell/core/ContextMenuModel.hpp/.cpp`: bounded model presentation revision, exact visible content height, and Qt-font-metric natural width calculation.
- `Shell/core/ContextMenuController.hpp/.cpp`: pass the shared resolved edge margin into pure placement and retain the existing Desktop point contract.
- `Shell/core/ContextMenuPlacement.hpp/.cpp`: apply the supplied edge margin to flip/clamp bounds in output-local coordinates.
- `Shell/qml/contextmenu/ShellMenuTheme.qml`: single generic Context Menu metric authority using the current Astrea typography and spacing.
- `Shell/qml/contextmenu/ContextMenuCard.qml`: consume shared card metrics and remove fixed generic width.
- `Shell/qml/contextmenu/ContextMenuItem.qml`: make row slots and text elision match the metric calculation.
- `Shell/qml/contextmenu/ContextMenuView.qml`: calculate natural/resolved width and exact desired/resolved height, enable scrolling only when capped, and pass resolved submenu dimensions.
- `Shell/qml/contextmenu/ContextMenuOverlaySurface.qml`: place only from final bound dimensions, coalesce geometry settlement, and log explicit placement fields.
- `Shell/qml/contextmenu/DesktopInteractionSurface.qml`: log output-local surface and right-click values before calling `presentDesktop()`.
- `Shell/tests/ContextMenuTest.cpp`: exact model metric and edge-margin placement tests.
- `Shell/tests/ContextMenuQmlSmokeTest.cpp`: production-QML width/height, one-item Desktop, tall-menu, four-corner, Dock-row, and model-replacement tests.
- `Shell/CMakeLists.txt`: change only if the selected test coverage requires a newly registered source; no new QML file is planned.

### Task 1: Add failing model metric contracts

**Files:**

- Modify: `Shell/tests/ContextMenuTest.cpp`
- Modify: `Shell/core/ContextMenuModel.hpp`
- Modify: `Shell/core/ContextMenuModel.cpp`

**Interfaces:**

- Add `Q_PROPERTY(quint64 presentationRevision READ presentationRevision NOTIFY presentationChanged)`.
- Add `Q_INVOKABLE int presentationContentHeight(int normalRowHeight, int separatorHeight) const`.
- Add `Q_INVOKABLE int presentationNaturalWidth(const QString &fontFamily, int bodyFontSize, int smallFontSize, int rowHorizontalMargin, int iconSlotWidth, int spacing, int cardPadding, int borderWidth) const`.
- `presentationContentHeight()` sums only visible root rows, using `separatorHeight` for separators and `normalRowHeight` for actions/submenus.
- `presentationNaturalWidth()` uses `QFontMetrics` for the supplied font family/sizes and accounts for label, shortcut, submenu arrow, icon/check slot, row margins, spacing, card padding, and border chrome; hidden/separator rows contribute no width.

- [ ] **Step 1: Write failing C++ tests**

Extend `ContextMenuTest` with one test that sets the following model roots and asserts exact heights using `(36, 10)`:

```cpp
QCOMPARE(one.presentationContentHeight(36, 10), 36);
QCOMPARE(three.presentationContentHeight(36, 10), 108);
QCOMPARE(mixed.presentationContentHeight(36, 10), 118); // 2 rows + separator + row
QCOMPARE(hidden.presentationContentHeight(36, 10), 36); // one hidden row omitted
QCOMPARE(submenu.presentationContentHeight(36, 10), 36); // submenu is one normal row
```

Add a width test with short, medium, long, shortcut, and submenu rows. Assert the measured width is content-sensitive, that adding a shortcut/arrow increases the row width, and that hidden/separator-only content does not increase it.

- [ ] **Step 2: Run the model test and verify the expected failure**

Run:

```bash
rtk run -c 'ctest --test-dir build/debug --output-on-failure -R "^context-menu-test$"'
```

Expected: compilation fails because the two model metric methods and revision property do not exist.

- [ ] **Step 3: Implement the minimal model metrics**

Include `QFont` and `QFontMetrics`. Add the revision property/signal, increment the revision after successful `setRootNodes()` reset and after a non-empty `clear()`, and implement the two invokables by iterating `m_root->children`. Set body font weight to `QFont::Medium` to match `ContextMenuItem.qml`; use `horizontalAdvance()` for labels, shortcuts, and `QStringLiteral("›")`. Do not change node validation, separator normalization, or activation behavior.

- [ ] **Step 4: Run the model test and verify it passes**

Run the same command. Expected: all `context-menu-test` cases pass, including the new exact height and natural width assertions.

- [ ] **Step 5: Commit the model contract**

```bash
rtk git add Shell/core/ContextMenuModel.hpp Shell/core/ContextMenuModel.cpp Shell/tests/ContextMenuTest.cpp
rtk git commit -m "fix(shell): add exact context menu model metrics"
```

### Task 2: Add the single QML sizing contract and failing geometry tests

**Files:**

- Modify: `Shell/qml/contextmenu/ShellMenuTheme.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuCard.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuItem.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuView.qml`
- Modify: `Shell/tests/ContextMenuQmlSmokeTest.cpp`

**Interfaces:**

- `ShellMenuTheme.qml` owns `contextMenuMinimumWidth: 200`, `contextMenuMaximumWidth: 260`, `contextMenuNormalRowHeight: 36`, `contextMenuSeparatorHeight: 10`, `contextMenuCardPadding: 10`, `contextMenuRowHorizontalMargin: 10`, `contextMenuIconSlotWidth: 20`, `contextMenuRowSpacing: 12`, `contextMenuBorderWidth: 1`, and `contextMenuEdgeMargin: 8`.
- `ContextMenuView` exposes `naturalWidth`, `resolvedWidth`, `exactContentHeight`, `desiredHeight`, `resolvedHeight`, `scrollable`, and `modelRowCount` as read-only diagnostic properties.
- `ContextMenuView` binds its own width to `resolvedWidth` and height to `resolvedHeight`; its card/list uses the same theme metrics.

- [ ] **Step 1: Write failing production-QML geometry tests**

Replace fixed-width smoke expectations with tests using real `ContextMenuView`/`ContextMenuOverlaySurface` objects:

```cpp
QTRY_COMPARE_WITH_TIMEOUT(view->property("modelRowCount").toInt(), 1, 1000);
QTRY_VERIFY_WITH_TIMEOUT(view->property("resolvedWidth").toReal() >= 200.0, 1000);
QTRY_VERIFY_WITH_TIMEOUT(view->property("resolvedWidth").toReal() <= 260.0, 1000);
QTRY_COMPARE_WITH_TIMEOUT(view->property("exactContentHeight").toInt(), 36, 1000);
QTRY_COMPARE_WITH_TIMEOUT(view->property("desiredHeight").toInt(), 56, 1000);
QTRY_COMPARE_WITH_TIMEOUT(view->property("resolvedHeight").toInt(), 56, 1000);
```

Add short/medium/long/shortcut/submenu instances and assert short content is below maximum while long content clamps to maximum. Add a realistic Dock model (`New Window`, `Open Windows`, separator, `Close Window`, separator, `Unpin from Dock`) and assert the final delegate bottom is at or above the card bottom padding boundary. Add a large model on a short output and assert `scrollable` is true, the resolved card height is output height minus two edge margins, and the last row can be positioned with keyboard selection.

- [ ] **Step 2: Run the QML smoke test and verify the expected failure**

Run:

```bash
rtk run -c 'ctest --test-dir build/debug --output-on-failure -R "^context-menu-qml-smoke-test$"'
```

Expected: the new properties do not exist or the old fixed 280 px/`ListView.contentHeight` expectations fail.

- [ ] **Step 3: Implement the theme-backed sizing contract**

Remove every generic `280` from Card, View, and submenu code. Make `ContextMenuCard` derive its default implicit dimensions from content and shared padding. In `ContextMenuView`, call the model invokables with `ShellMenuTheme` values, clamp natural width to `[minimum, maximum]` and available output width minus two edge margins, and compute desired/resolved height from the exact model result plus top/bottom padding. Reference `menuModel.presentationRevision` through a binding dependency so reset/replacement updates metrics.

Make `ContextMenuItem` use explicit icon/check, label, shortcut, and arrow slots whose widths match the model formula; set the label width to the remaining row width and retain `Text.ElideRight`. Set `ListView.interactive` only when `desiredHeight > resolvedHeight`; keep `positionViewAtIndex(row, ListView.Contain)` for keyboard reachability. Keep hidden delegates at height zero.

- [ ] **Step 4: Run the QML smoke test and verify it passes**

Run the same command. Expected: the one-item Desktop menu resolves to at least 200 px wide and 56 px tall, short menus do not resolve to 260 px, long rows elide at 260 px, Dock rows fit inside card padding, and tall menus scroll.

- [ ] **Step 5: Commit the shared QML sizing contract**

```bash
rtk git add Shell/qml/contextmenu/ShellMenuTheme.qml Shell/qml/contextmenu/ContextMenuCard.qml Shell/qml/contextmenu/ContextMenuItem.qml Shell/qml/contextmenu/ContextMenuView.qml Shell/tests/ContextMenuQmlSmokeTest.cpp
rtk git commit -m "fix(shell): resolve context menu size from content"
```

### Task 3: Apply final-size placement and Desktop boundary diagnostics

**Files:**

- Modify: `Shell/core/ContextMenuController.hpp`
- Modify: `Shell/core/ContextMenuController.cpp`
- Modify: `Shell/core/ContextMenuPlacement.hpp`
- Modify: `Shell/core/ContextMenuPlacement.cpp`
- Modify: `Shell/qml/contextmenu/ContextMenuOverlaySurface.qml`
- Modify: `Shell/qml/contextmenu/ContextMenuView.qml`
- Modify: `Shell/qml/contextmenu/DesktopInteractionSurface.qml`
- Modify: `Shell/tests/ContextMenuTest.cpp`
- Modify: `Shell/tests/ContextMenuQmlSmokeTest.cpp`

**Interfaces:**

- Extend `ContextMenuPlacement::Request` with `int edgeMargin = 0`.
- Extend `ContextMenuController::menuPosition()` and `submenuPosition()` with a trailing `int edgeMargin = 8`; QML passes `ShellMenuTheme.contextMenuEdgeMargin` explicitly.
- `DesktopInteractionSurface.qml` logs `desktop-input` with `windowWidth`, `windowHeight`, `mouseX`, `mouseY`, `outputKey`, `outputWidth`, `outputHeight`, `outputOriginX`, and `outputOriginY` under `ASTREA_CONTEXT_MENU_DEBUG=1`.
- Overlay diagnostics add `placementInputX/Y`, `placementFinalX/Y`, `naturalWidth`, `naturalHeight`, `desiredHeight`, `resolvedWidth`, and `resolvedHeight`.

- [ ] **Step 1: Write failing edge-margin and final-size tests**

Update `ContextMenuTest` point placement to assert an 8 px margin when the request supplies `edgeMargin = 8`, including all four corners. Preserve the existing zero-margin oversized-output expectation. Extend QML smoke coverage to assert Desktop one-item placement at top-left/top-right/bottom-left/bottom-right from the final resolved size and that final positions remain within `[edgeMargin, output - edgeMargin - size]`.

- [ ] **Step 2: Run focused tests and verify the expected failure**

Run:

```bash
rtk run -c 'ctest --test-dir build/debug --output-on-failure -R "context-menu-(test|qml-smoke-test|qml-interaction-test)$"'
```

Expected: placement requests have no edge-margin field and the new final-size assertions fail against the current zero-margin/fixed-size behavior.

- [ ] **Step 3: Implement final-size placement and diagnostics**

Make placement derive its clamp bounds from `output` plus the non-negative request edge margin, reducing the margin to zero only when a menu is larger than its output. Flip point placement before clamping; retain Dock centering/above-first behavior and submenu direction. Pass the QML metric edge margin through both controller invokables.

Replace Overlay's fixed-width and implicit-height assignments with bindings to `menuView.resolvedWidth`/`resolvedHeight`. Coalesce `syncMenuGeometry()` behind a zero-interval Timer triggered by presentation, model-size, and output-size changes so placement is evaluated after the final bindings settle. Do not set position from `ListView.contentHeight`. Pass the same resolved width/height from each generic submenu view into `submenuPosition()`.

Add the Desktop input debug record immediately before `presentDesktop()` and add explicit final placement fields to the existing Overlay debug record. Do not add coordinate compensation or scale conversion.

- [ ] **Step 4: Run focused tests and verify they pass**

Run the same focused command. Expected: exact point flips/clamps, final-size placement, four corners, and reusable-renderer model replacement pass.

- [ ] **Step 5: Commit placement and diagnostics**

```bash
rtk git add Shell/core/ContextMenuController.hpp Shell/core/ContextMenuController.cpp Shell/core/ContextMenuPlacement.hpp Shell/core/ContextMenuPlacement.cpp Shell/qml/contextmenu/ContextMenuOverlaySurface.qml Shell/qml/contextmenu/ContextMenuView.qml Shell/qml/contextmenu/DesktopInteractionSurface.qml Shell/tests/ContextMenuTest.cpp Shell/tests/ContextMenuQmlSmokeTest.cpp
rtk git commit -m "fix(shell): place context menus from final geometry"
```

### Task 4: Full verification and runtime qualification

**Files:**

- No additional production files planned; review the focused diff and existing dirty worktree.

- [ ] **Step 1: Rebuild the debug preset**

```bash
rtk run -c 'cmake --build --preset debug'
```

Use the existing LayerShellQt prefix and library environment if the configured build requires it.

- [ ] **Step 2: Run the complete configured test suite**

```bash
rtk run -c 'TMPDIR=/tmp ctest --preset debug --output-on-failure'
```

Record the exit status and any unrelated baseline failures without altering unrelated files.

- [ ] **Step 3: Run QML gate and whitespace checks**

```bash
rtk run -c 'tools/ci/run-qml-gate.sh'
rtk git diff --check
```

- [ ] **Step 4: Run a fresh source review**

Use `rtk rg` to verify no generic `280` remains in Context Menu sizing, no `ListView.contentHeight` drives intrinsic size, no unconditional `interactive: false` remains for generic menus, no Typhon path changed, and no layer-shell policy changed. Confirm only focused files are staged in each implementation commit.

- [ ] **Step 5: Qualify the real session with both diagnostics enabled**

Run the built `astrea-shell` with `ASTREA_CONTEXT_MENU_DEBUG=1` and `OBLIVION_ONE_LAYER_SHELL_DEBUG=1`. Exercise Desktop center and four corners, Dock few/many actions, Desktop Actions, multiple windows, and rapid Dock→Desktop→Dock replacement. Capture the `desktop-input`, controller `presentation`, Overlay `settled`, and placement fields. Compare click coordinates, output-local anchor, final size, and final position. If the session cannot expose a background click because regular windows cover the Bottom-layer interaction surface, report that exact limitation and do not call the Desktop runtime path fully qualified.

- [ ] **Step 6: Commit any required verification correction only**

If verification exposes a required focused correction, stage only its Context Menu files and commit it with a message naming the corrected invariant. Otherwise leave the three focused commits intact.
