# Dock App Tooltip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Present the Dock app tooltip as a compact themed pill bubble anchored to the visually magnified icon.

**Architecture:** Keep hover and magnification in `DockPanel`; map its current `interactionRegion` using `DockSurfaceGeometry`; present it in a full-output input-transparent Layer Shell window. One closed Qt Quick Shapes path renders the pill body and integrated tail. The text uses the body rectangle, while Frosted blur registers a rounded body-only region.

**Tech Stack:** C++20, Qt 6.6, Qt Quick/QML, LayerShellQt, Qt Test.

**Spec:** `docs/superpowers/specs/2026-10-02-dock-app-tooltip.md`

## Global Constraints

- Preserve the 420 ms delay, Dock target identity, and scale-aware `interactionRegion` anchor.
- Keep the tooltip surface input-transparent and focusless, with `exclusive_zone = -1`. It reserves no desktop space and ignores other Layer Shell exclusive zones so its coordinates cover the complete output. Preserve the main Dock's reservation policy.
- Keep builds/tests in `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
- Work on `main`; preserve unrelated Settings/network edits.

## Review Focus

- Each Dock edge and preferred-side fallback: verify tail edge, target gap, pointer-center safe inset, body bounds, and output clamping in geometry tests.
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

- [ ] Add QML regression assertions for the passive tooltip window, unified vector bubble, delayed target presentation, mapped magnified anchor, and hiding during context menus/reorder.
- [ ] Run the targeted QML test and confirm it fails before adding the presentation path.
- [ ] Add shared material text/font/shadow tokens; render one one-line vector bubble with a subtle continuous outline and shadow.
- [ ] Remove `QtQuick.Controls.Basic` and attached `ToolTip` properties from `DockAppDelegate.qml`.
- [ ] Add the separate output-sized passive window, configure it with the new Dock Layer Shell policy, and wire DockPanel eligibility/anchor state to it.
- [ ] Run the targeted QML test, geometry tests, Layer Shell test, and complete Dock hover QML test.
- [ ] Review the feature-only diff, stage only plan/spec and Dock tooltip files, and commit the logical feature change.

### Hardening Follow-up: Full-Output Geometry and Delayed Lifecycle (2026-10-02)

- [x] Use `exclusive_zone = -1`: the tooltip reserves no space and ignores other Layer Shell reservations. This is distinct from `exclusive_zone = 0`, which may move the surface around positive exclusive zones.
- [x] Correct the policy regression to assert `-1`, Top layer, keyboard interactivity None, all four anchors, and the tooltip scope. The original qualification asserted 0 and did not test `-1`.
- [x] On output dimension changes, re-present an already-visible tooltip through `presentTooltipForCurrentTarget()` after QML geometry settles. This records the geometry behavior; lifecycle revalidation is qualified separately below. Hidden and delay-pending tooltips retain the normal 420 ms delay.
- [x] Cover bottom and left output resizing, target replacement during the delay, and target removal during the delay.
- [x] Build `dock-layer-shell-surface-test`, `dock-surface-geometry-test`, `dock-hover-qml-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; focused CTest passed 3/3 and the complete Dock CTest set passed 14/14.
- [x] `git diff --check` passed.

### Tooltip Lifecycle Race Hardening (2026-10-02)

- [x] Keep the 420 ms `tooltipDelay` as the sole hidden-to-visible presentation authority. Deferred visible refreshes update a tooltip only while its original presentation remains visible and eligible.
- [x] Capture the tooltip lifecycle generation and `pointerTargetTaskKey` when queuing a refresh. Discard the callback at execution if either identity changed, the surface is missing or hidden, or the current presentation is ineligible.
- [x] Coalesce refresh requests for the same generation and task so output width/height changes resolve once using final geometry. Route output geometry, magnification geometry, and `DockController.modelChanged` refreshes through this guarded path.
- [x] Qualify replacement-target races for geometry and model changes, preserve pending-target/removal and interaction regressions, and assert one presentation for settled output geometry changes.

### Visual Hardening Pass (2026-10-03)

**Goal:** Replace the separate card and rotated-square pointer with one pill-shaped vector bubble while preserving tooltip placement, ownership, lifecycle, and Layer Shell behavior.

**Architecture:** `DockSurfaceGeometry` is the explicit owner of the body height, pointer width/depth, and target gap consumed by both C++ placement and QML rendering. `DockTooltipBubble.qml` draws one closed `ShapePath` for the rounded body and integrated pointer. The tooltip text remains centered in the body rectangle. Frosted mode registers a rounded body-only backdrop proxy, then draws the single translucent silhouette above it with one outline and one shadow.

**Files:**
- Modify: `Dock/core/DockSurfaceGeometry.hpp` and `.cpp` for shared tooltip metrics, pill-safe pointer inset, and 3 px target gap.
- Modify: `Dock/tests/DockSurfaceGeometryTest.cpp` for shared metrics, all edge orientations, rounded-corner exclusion, target gap, and clamped-pointer behavior.
- Create: `Dock/qml/components/DockTooltipBubble.qml` for a single Qt Quick Shapes silhouette and one shadow.
- Modify: `Dock/qml/components/DockTooltipSurface.qml` for the body-sized Frosted backdrop proxy, text layout, and bubble wiring; preserve surface flags and lifecycle calls.
- Modify: `Shell/CMakeLists.txt` to add the new QML resource and alias.
- Modify: `shared/qml/ShellMaterialTheme.qml` to add tooltip-specific surface, border, and shadow semantics without changing existing tokens.
- Modify: `Dock/tests/DockHoverQmlTest.cpp` for unified-shape, pill radius, body text/backdrop bounds, translucency, single shadow, and orientation regressions.
- Modify: this spec, this plan, and `.superpowers/sdd/2026-10-02-dock-app-tooltip/progress.md` after verification.

**Execution steps:**

- [x] Update the geometry and QML assertions first, build and run them from `/mnt/Aether/Desktop/GitHub/Eclipse-build`, and confirm the expected new-architecture or metric assertions fail.
- [x] Add constant `DockSurfaceGeometry` properties for `tooltipBodyHeight`, `tooltipPointerWidth`, `tooltipPointerDepth`, and `tooltipTargetGap`; compute placement spacing from depth plus target gap and clamp pointer centers using `min(edgeLength / 2, bodyHeight / 2 + pointerWidth / 2)`.
- [x] Implement `DockTooltipBubble.qml` with `Shape.CurveRenderer` and one closed path for top, bottom, left, and right tails; use the geometry object's pointer metrics and a single `MultiEffect` shadow on the vector shape.
- [x] Rewire `DockTooltipSurface.qml` to use a 72–280 px body, 34 px height, 16 px horizontal text padding, 13 px text, and a non-rendering pill-body backdrop proxy; keep the body and text bounds separate from total bubble bounds.
- [x] Add tooltip-specific shared material colors: opaque elevated default, approximately 0.44-alpha Frosted tint, more transparent existing-style tint for Transparent, an attenuated border, and one subtle shadow. Keep existing shared tokens intact.
- [x] Add the new component to `Shell/CMakeLists.txt`, update the exact geometry/QML regressions, and revise the visual architecture wording in the spec without rewriting prior lifecycle history.
- [x] Build `dock-hover-qml-test`, `dock-surface-geometry-test`, `astrea-shell`, and `bar-qml-smoke-test`; run the three requested focused tests, the complete Dock CTest set, and `git diff --check` from the verified Aether build tree.
- [x] Inspect the available native Wayland session. No running Astrea Shell or Typhon process was available, and the desktop automation surface was unavailable, so native visual qualification could not be performed.
- [x] Append a separate visual-hardening entry to progress after verification, review the final changed-file list and forbidden legacy tooltip constructions, then commit the logical change on `main`.
