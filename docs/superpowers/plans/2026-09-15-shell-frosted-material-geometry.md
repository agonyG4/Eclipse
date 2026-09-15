# Eclipse Shell Frosted Material Geometry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Eclipse Topbar radius ownership and Dock material coupling while preserving the stable Layer Shell envelope and dynamic icon interaction.

**Architecture:** Add a small `Astrea.Shared.ShellMaterialTheme` QML object containing the current Shell material state/tokens. `ShellBarTheme` forwards its existing public API from that object, and Dock consumes the same background/border/radius policy. Keep Dock chrome at resting geometry; magnification remains in delegates and their panel-owned interaction targets.

**Tech Stack:** Qt 6.8 QML, Qt Quick, CMake, Qt Test, existing Astrea backdrop-effect region classes.

## Global Constraints

- Eclipse-only; do not modify Typhon.
- Preserve unrelated screenshot, shortcut-routing, Shell runtime, and existing dirty-tree work exactly.
- Keep `surfaceWidth`, `surfaceHeight`, `surfaceHeadroom`, `maximumMagnificationExtraPrimary`, and `maximumMagnificationExtraWidth` unchanged.
- Do not add clipping to magnified Dock content or change Typhon effect planning.
- Use the existing `build/release` directory for compilation and testing.

---

### Task 1: Add failing regression coverage

**Files:**
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `Dock/tests/DockHoverQmlTest.cpp`
- Modify: `shared/tests/BackdropEffectRegionsTest.cpp`

**Interfaces:**
- Consumes: current `BarSegment`, Launcher/Status QML object tree, Dock panel properties, and `AstreaBackdropEffectRegions::resolvedRegion()`.
- Produces: red tests that specify the new radius, stable-material, interaction, and rounded-region contracts.

- [ ] **Step 1: Add Bar radius and backdrop assertions.**

Declare and implement tests that instantiate `BarSegment`, Launcher, and Status. Assert that `barSegmentSurface.radius` equals a positive `surfaceRadius`, and locate each `AstreaBackdropRegion` by its `item` property to assert its radius equals the corresponding pill's visible radius. The current implementation must fail because `BarSegment` lacks `surfaceRadius` and Launcher/Status bind to the nonexistent `Item.radius`.

- [ ] **Step 2: Add Dock stable-chrome assertions.**

Change the existing hover expectations so horizontal and vertical cases require `dockChrome.width == restingWidth` and `dockChrome.height == restingHeight` before, during, and after hover. Keep the existing surface stability assertions. Add checks that a scaled icon overhangs the chrome while remaining inside the panel and that the input mask contains its interaction bounds but not transparent panel corners.

- [ ] **Step 3: Add the rounded resolved-region regression.**

Add a 36px-high item with a positive radius to `BackdropEffectRegionsTest`. Assert the resolved region has multiple rectangles, covers the pill center, and does not cover its top-left corner.

- [ ] **Step 4: Run focused tests and verify the expected red failures.**

Run:

```bash
rtk ctest --test-dir build/release -R 'dock-hover-qml-test|bar-qml-smoke-test|backdrop-effect-regions-test' --output-on-failure
```

Expected: the new radius and stable-chrome assertions fail against the current implementation; existing unrelated tests remain identifiable separately.

- [ ] **Step 5: Commit the failing tests.**

```bash
rtk git add Bar/tests/BarQmlSmokeTest.cpp Dock/tests/DockHoverQmlTest.cpp shared/tests/BackdropEffectRegionsTest.cpp
rtk git commit -m "test: cover shell material geometry regressions"
```

### Task 2: Add shared Shell material tokens and radius authority

**Files:**
- Create: `shared/qml/ShellMaterialTheme.qml`
- Modify: `shared/CMakeLists.txt`
- Modify: `Bar/cmake/BarResources.cmake`
- Modify: `Bar/qml/components/ShellBarTheme.qml`
- Modify: `Bar/qml/components/BarSegment.qml`
- Modify: `Bar/qml/LauncherSurface.qml`
- Modify: `Bar/qml/StatusSurface.qml`
- Modify: `Shell/CMakeLists.txt` only if the Bar QML test needs the shared QML plugin explicitly.

**Interfaces:**
- Consumes: `ThemeController.themeMode`, `ThemeController.shellStyle`, and the existing six Shell palette combinations.
- Produces: `Shared.ShellMaterialTheme` with `isLight`, `isTransparent`, `isDefault`, `isFrosted`, `background`, `surface`, `border`, `borderHover`, `hover`, `pressed`, `active`, `radiusLarge`, and `radiusMedium`; `BarSegment.surfaceRadius`.

- [ ] **Step 1: Register `ShellMaterialTheme.qml` in `Astrea.Shared`.**

Add it to the `QML_FILES` list for `astrea-shared-core`. Ensure Bar resources can import the `Astrea.Shared` module through the existing shared-core plugin linkage.

- [ ] **Step 2: Move only material policy into the shared object.**

Copy the exact current background, surface, border, hover, pressed, active, light/dark, default/transparent/frosted, and radius values. Do not move text or unrelated Bar-only typography tokens.

- [ ] **Step 3: Forward `ShellBarTheme` public material API.**

Instantiate `ShellMaterialTheme` and forward all existing state/material aliases and radius properties while leaving the public property names intact. Keep the existing Bar palette test expectations unchanged.

- [ ] **Step 4: Make `BarSegment.surfaceRadius` authoritative.**

Define `readonly property real surfaceRadius: theme.shellRadiusLarge - 2`, set the visual rectangle radius from it, and bind Launcher/Status backdrop regions to `launcherPill.surfaceRadius` and `statusPill.surfaceRadius`.

- [ ] **Step 5: Run Bar and shared tests.**

```bash
rtk cmake --build build/release --target bar-qml-smoke-test backdrop-effect-regions-test
rtk ctest --test-dir build/release -R 'bar-qml-smoke-test|backdrop-effect-regions-test' --output-on-failure
```

Expected: the new Bar radius/palette tests and the rounded-region test pass.

- [ ] **Step 6: Commit the shared material and Topbar changes.**

```bash
rtk git add shared/qml/ShellMaterialTheme.qml shared/CMakeLists.txt Bar/cmake/BarResources.cmake Bar/qml/components/ShellBarTheme.qml Bar/qml/components/BarSegment.qml Bar/qml/LauncherSurface.qml Bar/qml/StatusSurface.qml Shell/CMakeLists.txt
rtk git commit -m "fix: share shell material geometry policy"
```

### Task 3: Stabilize Dock material chrome

**Files:**
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/tests/DockHoverQmlTest.cpp`

**Interfaces:**
- Consumes: `Shared.ShellMaterialTheme`, existing resting geometry, delegate visual transforms, and `inputInteractionRects`.
- Produces: stable resting-sized `dockChrome`, semantic Shell colors, and unchanged dynamic delegate/input behavior.

- [ ] **Step 1: Use shared material tokens in Dock.**

Instantiate `Shared.ShellMaterialTheme`, use its `background` and `border` for the single material rectangle, use `isFrosted` for backdrop enablement, and remove the hard-coded dark background and nested black overlay.

- [ ] **Step 2: Stop hover from changing material geometry.**

Bind both chrome dimensions directly to `restingWidth`/`restingHeight`, retain placement, radius, width/height change notifications, and any geometry-change animation needed for real model/config changes. Remove `magnificationWidth`/`magnificationHeight` and their assignments because no material consumer remains.

- [ ] **Step 3: Preserve content and interaction geometry.**

Leave `appRow`, delegate scale/offset transforms, `interactionTarget`, `updateInputRegion()`, and `isInteractivePoint()` behavior intact. Do not add clipping or a giant input rectangle.

- [ ] **Step 4: Run Dock hover tests.**

```bash
rtk cmake --build build/release --target dock-hover-qml-test
rtk ctest --test-dir build/release -R 'dock-hover-qml-test' --output-on-failure
```

Expected: stable surface/chrome, overhang, dynamic input, vertical orientation, drag/reorder, and auto-hide tests pass.

- [ ] **Step 5: Commit the Dock change.**

```bash
rtk git add Dock/qml/components/DockPanel.qml Dock/tests/DockHoverQmlTest.cpp
rtk git commit -m "fix: keep dock material stable during magnification"
```

### Task 4: Full focused verification and handoff

**Files:**
- Verify only the task files and pre-existing unrelated dirty files.

**Interfaces:**
- Consumes: the focused commits and existing `build/release` configuration.
- Produces: reproducible build/test evidence and a clean task-scoped diff report.

- [ ] **Step 1: Build the requested Eclipse targets.**

```bash
rtk cmake --build build/release --target dock-hover-qml-test bar-qml-smoke-test astrea-shell
```

- [ ] **Step 2: Run focused tests and shared geometry tests.**

```bash
rtk ctest --test-dir build/release -R 'dock-hover-qml-test|bar-qml-smoke-test|backdrop-effect-regions-test' --output-on-failure
```

- [ ] **Step 3: Run diff validation.**

```bash
git diff --check
```

- [ ] **Step 4: Inspect the final task-scoped diff and status.**

```bash
rtk git diff --stat
rtk git status --short
```

Report the pre-existing screenshot changes separately; do not claim a workspace-wide clean tree.
