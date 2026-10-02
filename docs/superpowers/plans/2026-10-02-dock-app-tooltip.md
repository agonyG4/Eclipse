# Dock App Tooltip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the generic Dock app tooltip with a themed speech bubble anchored to the visually magnified icon.

**Architecture:** Keep hover and magnification in `DockPanel`; map its current `interactionRegion` using `DockSurfaceGeometry`; present it in a full-output input-transparent Layer Shell window. The card uses shared theme tokens and a pure placement calculation for bottom, left, and right Docks.

**Tech Stack:** C++20, Qt 6.6, Qt Quick/QML, LayerShellQt, Qt Test.

**Spec:** `docs/superpowers/specs/2026-10-02-dock-app-tooltip.md`

## Global Constraints

- Preserve the 420 ms delay, Dock target identity, and scale-aware `interactionRegion` anchor.
- Keep the tooltip surface input-transparent and focusless, with `exclusive_zone = -1`. It reserves no desktop space and ignores other Layer Shell exclusive zones so its coordinates cover the complete output. Preserve the main Dock's reservation policy.
- Keep builds/tests in `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
- Work on `main`; preserve unrelated Settings/network edits.

## Review Focus

- Each Dock edge and preferred-side fallback: verify pointer edge, rectangle bounds, and output clamping in geometry tests.
- Pointer target movement during the delay and target deletion: verify the stale target is not presented.
- Magnification and drag reorder: verify the tooltip anchor follows the transformed interaction region and hides during reorder.
- Dock autohide and context menus: verify tooltip visibility is suppressed.
- Surface input/focus behavior and stacking: verify QML flags and Layer Shell configuration while preserving Overlay menus.

---

### Task 1: Output-local tooltip geometry and passive Layer Shell policy

**Files:**
- Modify: `Dock/core/DockSurfaceGeometry.hpp`
- Modify: `Dock/core/DockSurfaceGeometry.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`
- Test: `Dock/tests/DockSurfaceGeometryTest.cpp`
- Test: `Dock/tests/DockLayerShellSurfaceTest.cpp`

**Interfaces:**
- Produce `DockTooltipPlacement { QRect cardRect; QString pointerEdge; int pointerCenter; }` and `DockSurfaceGeometry::tooltipPlacementInOutput(outputSize, anchorRect, cardSize, position)`.
- Produce `DockLayerShellSurface::tooltipConfigurationFor(screen)` and `configureTooltip(window, screen, errorOut)` with Top layer, four output anchors, no keyboard focus, and `exclusive_zone = -1` for full-output coordinates.

- [ ] Write geometry tests for bottom/left/right placement, edge clamping, pointer-edge fallback, and pointer-center bounds.
- The initial Layer Shell qualification asserted an exclusive zone of 0. That did not protect the full-output coordinate invariant; the hardening follow-up below corrects the policy and regression expectation.
- [ ] Run the two tests and confirm they fail because the tooltip APIs are absent.
- [ ] Implement the pure placement helper and passive surface policy.
- [ ] Run both tests and confirm they pass.

### Task 2: Themed tooltip surface and Dock presentation wiring

**Files:**
- Create: `Dock/qml/components/DockTooltipSurface.qml`
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`
- Modify: `Shell/app/AstreaShellApplication.hpp`
- Modify: `Shell/app/AstreaShellApplication.cpp`
- Modify: `Shell/CMakeLists.txt`
- Modify: `shared/qml/ShellMaterialTheme.qml`
- Modify: `Bar/qml/components/ShellBarTheme.qml`
- Test: `Dock/tests/DockHoverQmlTest.cpp`

**Interfaces:**
- `DockTooltipSurface.showTooltip(title, anchorRect, position)` and `hideTooltip()` update only passive presentation state.
- `DockPanel` resolves the target delegate, obtains its `interactionRegion`, maps it through `windowRect` and `DockSurfaceGeometry.outputLocalDelegateRect`, and calls the surface after the existing delay.
- `AstreaShellApplication` configures and refreshes the tooltip window on the same screen as the Dock.

- [ ] Add QML regression assertions for the passive tooltip window, themed card/pointer, delayed target presentation, mapped magnified anchor, and hiding during context menus/reorder.
- [ ] Run the targeted QML test and confirm it fails before adding the presentation path.
- [ ] Add shared material text/font/shadow tokens; render a one-line rounded card and a pointer with a subtle shadow and border.
- [ ] Remove `QtQuick.Controls.Basic` and attached `ToolTip` properties from `DockAppDelegate.qml`.
- [ ] Add the separate output-sized passive window, configure it with the new Dock Layer Shell policy, and wire DockPanel eligibility/anchor state to it.
- [ ] Run the targeted QML test, geometry tests, Layer Shell test, and complete Dock hover QML test.
- [ ] Review the feature-only diff, stage only plan/spec and Dock tooltip files, and commit the logical feature change.

### Hardening Follow-up: Full-Output Geometry and Delayed Lifecycle (2026-10-02)

- [x] Use `exclusive_zone = -1`: the tooltip reserves no space and ignores other Layer Shell reservations. This is distinct from `exclusive_zone = 0`, which may move the surface around positive exclusive zones.
- [x] Correct the policy regression to assert `-1`, Top layer, keyboard interactivity None, all four anchors, and the tooltip scope. The original qualification asserted 0 and did not test `-1`.
- [x] On output dimension changes, re-present an already-visible tooltip through `presentTooltipForCurrentTarget()` after QML geometry settles. Hidden and delay-pending tooltips retain the normal 420 ms delay.
- [x] Cover bottom and left output resizing, target replacement during the delay, and target removal during the delay.
- [x] Build `dock-layer-shell-surface-test`, `dock-surface-geometry-test`, `dock-hover-qml-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; focused CTest passed 3/3 and the complete Dock CTest set passed 14/14.
- [x] `git diff --check` passed.
