# Dock App Tooltip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Present the Dock app tooltip as a compact themed pill bubble anchored to the visually magnified icon.

**Architecture:** Keep hover and magnification in `DockPanel`; map its current `interactionRegion` using `DockSurfaceGeometry`; present it in a full-output input-transparent Layer Shell window. One closed Qt Quick Shapes path renders the pill body and integrated tail with exact circular arcs. `FontMetrics` measures the application `displayName` and drives a 72–420 px output-bounded body width. `DockTooltipBubble` owns both the vector path and the combined pixel-center binary body/tail `backdropRectangles` used by the single Frosted effect descriptor. The visual Shape is rendered through a local DPR-aware supersampled layer; the backdrop remains bounded by Wayland's integer region representation.

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

### Circular Side Tail and Frosted Region Integration (2026-10-03)

**Goal:** Remove the vertical endcap stroke seam and blur the complete vector bubble in Frosted mode using the existing bounded rectangle protocol.

**Architecture:** Keep one closed `ShapePath` for the pill and tail. Use exact circular SVG arcs and splice side tails between circle intersections derived from `radius - sqrt(radius² - halfPointer²)`. Extend `BackdropRegion` with optional item-local rectangles, map every rectangle through `QQuickItem::mapRectToItem`, and merge body and tail regions through the existing 96-rectangle budget.

**Files:**
- Modify: `Dock/qml/components/DockTooltipBubble.qml` for circular endcaps, side-tail intersections, and generated tail scanlines.
- Modify: `Dock/qml/components/DockTooltipSurface.qml` for the shared Frosted body and tail descriptors.
- Modify: `Dock/tests/DockHoverQmlTest.cpp` for semantic path, descriptor, union, clip, and rectangle-budget regressions.
- Modify: `shared/platform/wayland/effects/AstreaBackdropRegion.hpp` and `.cpp` for `QVariantList localRectangles` and change notification.
- Modify: `shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp` for transformed local rectangles in the existing aggregate budget.
- Modify: `shared/tests/BackdropEffectRegionsTest.cpp` for exact local unions, transforms, empty-list compatibility, and budget behavior.
- Modify: this spec and plan to record the final geometry and Frosted region model.
- Modify: `.superpowers/sdd/2026-10-02-dock-app-tooltip/progress.md` only after verification.

**Execution steps:**

- [x] Add side-tail path and explicit-local-region regression assertions; run each focused test against the current implementation and confirm the expected failures.
- [x] Add the backward-compatible `localRectangles` property and resolve each local rectangle through the owning item's coordinate transform and surface clip.
- [x] Keep explicit rectangles in the existing 96-rectangle aggregation and conservative overflow path.
- [x] Replace quadratic capsule corners with exact circular arcs; splice the left/right tails at the derived circle intersections.
- [x] Generate no more than eight one-pixel tail scanlines from the same pointer geometry and register them beside the rounded body only while Frosted and visible.
- [x] Build the requested targets in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; run focused backdrop, geometry, and QML tests, the complete Dock set, and discovered shared-effects CTest cases.
- [x] Update the spec/plan, inspect every changed file and forbidden legacy pattern, run `git diff --check`, and inspect for an available native Wayland session. No live Shell or Typhon process was available and the CUA native pipe could not attach, so native visual qualification remains outstanding.
- [x] Append the verified result to progress and commit the logical change on `main`.

### Visual Hardening Pass (2026-10-03)

**Goal:** Replace the separate card and rotated-square pointer with one pill-shaped vector bubble while preserving tooltip placement, ownership, lifecycle, and Layer Shell behavior.

**Architecture:** `DockSurfaceGeometry` is the explicit owner of the body height, pointer width/depth, and target gap consumed by both C++ placement and QML rendering. `DockTooltipBubble.qml` draws one closed `ShapePath` for the rounded body and integrated pointer. The tooltip text remains centered in the body rectangle. Frosted mode registers a rounded body proxy unioned with bounded tail scanlines, then draws the single translucent silhouette above it with one outline and one shadow.

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

The initial visual-hardening implementation registered only the rounded body; the Circular Side Tail and Frosted Region Integration follow-up above supersedes that interim region description.

### Tooltip Text Sizing and Exact Frosted Pill Follow-up (2026-10-04)

**Goal:** Show each application's full display name at natural width when possible, and make the Frosted effect follow the exact vector pill silhouette while preserving placement and lifecycle behavior.

**Final architecture:** The Dock label is `displayName` alone. `FontMetrics` with the visual text's exact font measures the natural width; the card adds 32 px padding and clamps between 72 px and the smaller of a 420 px design maximum or the logical output width less two 8 px insets. `DockTooltipBubble` remains the vector authority and now derives `backdropBodyRectangles`, `backdropTailRectangles`, and their union from its own local geometry. Its 1 px render padding protects the centered outline and effect texture; `cardRect` and pointer placement do not include that padding.

**Verification:**

- [x] The explicit `Zen Browser` regression uses the Shell tooltip font metrics: advance width 78.5469 px, expected and actual card width 111 px, title and text match, and `truncated` is false.
- [x] A measured medium-long name grows past the previous 280 px cap without truncation. A 1000-character name caps at 314 px on a 330 px output, stays inside the 8 px edge inset, and remains one-line `ElideRight` text.
- [x] A running Zen Browser delegate with four windows still presents only `Zen Browser`; model `windowCount` remains four.
- [x] For all pointer edges, the padded bubble body maps exactly to `cardRect`, the outline has room inside the Shape item, and the tail tip retains its 3 px target gap.
- [x] Frosted uses one BackdropRegion on `DockTooltipBubble`. Resolved scanline counts: bottom 34 body + 6 tail = 40; top 34 + 6 = 40; left 34 + 7 = 41; right 34 + 7 = 41. Mapped tests include pill center/edge centers and tail points, and exclude all four radius-17 corners and points outside the tail.
- [x] Built `dock-hover-qml-test`, `dock-surface-geometry-test`, `backdrop-effect-regions-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; focused tests passed 3/3, all Dock tests passed 14/14, and the shared-effects subset passed 3/3.
- [x] `git diff --check` passed.
- Native Frosted visual qualification remains unobserved: the Wayland session has no running Eclipse/Typhon window and the CUA native pipe is unavailable, so no live comparison to the macOS reference is claimed.

### Short Dock Tooltip Edge Aliasing (2026-10-05)

**Goal:** Improve the small-width vector/stroke raster and replace excessive under-coverage in the Frosted body's and integrated tail's bounded binary mask without changing logical geometry, placement, lifecycle, or effect ownership.

**Architecture:** Keep `Shape.CurveRenderer`, the existing single closed `ShapePath`, and the existing `MultiEffect`. Set the Shape layer texture to the ceiling of its logical dimensions multiplied by `max(2, validated Screen.devicePixelRatio)` and enable smooth linear sampling. Compute body rows and all four triangle-tail orientations by pixel-center inclusion against the existing analytic capsule/triangle geometry. Keep one `BackdropRegion` for `backdropRectangles`; do not change the generic Wayland rectangle protocol or its 96-rectangle budget.

**Files:**
- Modify: `Dock/qml/components/DockTooltipBubble.qml` for DPR-aware layer dimensions and pixel-center body/tail slices. `buildPathData()`, pill metrics, render padding, and placement inputs stay unchanged.
- Modify: `Dock/tests/DockHoverQmlTest.cpp` for the compact Sober case, independent C++ capsule and triangle raster references, adjacent-pixel checks, source-level old/new comparison, and DPR texture assertions. Preserve and rerun Zen Browser sizing.
- Modify: this spec and plan to record both raster domains and the `wl_region` coverage limit.
- Append: `.superpowers/sdd/2026-10-02-dock-app-tooltip/progress.md` after the final verification pass.

**Verification:**

- [x] Add the regression assertions before the implementation; the focused test initially failed because the required `layer.textureSize` was absent.
- [x] Compare the 72×34 analytic capsule against a C++ pixel-center reference. The first four top rows change from old/new widths 38/46, 48/52, 54/56, and 56/58; every included center is inside and the immediate left/right excluded centers are outside.
- [x] Compare the QML tail slices against the independent analytic reference for bottom, top, left, and right. Final body/tail/resolved counts: top 34+7=41, bottom 34+7=41, left 34+8=42, right 34+8=42; maximum 42 remains below 96.
- [x] Qualify Sober at its 72 px minimum width, 34 px height, radius 17, pointer width 14/depth 8, and 3 px gap. Its bottom bubble layer is 74×44 logical pixels and 148×88 at 2×. Zen Browser remains fully visible at its 111 px measured card width.
- [x] Run the Sober DPR case at 1×, 1.5×, 2×, and 3×: raster scales are 2, 2, 2, and 3 respectively; texture sizes are 148×88, 148×88, 148×88, and 222×132.
- [x] Build `dock-hover-qml-test`, `dock-surface-geometry-test`, `backdrop-effect-regions-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`. Focused CTest passed 3/3; complete Dock CTest passed 14/14; relevant shared-effects CTest passed 3/3.
- [x] Native Default/Frosted comparison was unavailable: the native app inventory was empty and CUA reported the native pipe unavailable. This pass does not attribute an unobserved live artifact or claim that `wl_region` has fractional antialiasing.
- [x] Run `git diff --check`, complete the adversarial diff review, append progress, and commit the logical change on `main`.

### Typhon Analytic Background Coverage Follow-up (2026-10-09)

**Architecture:** Preserve the current `resolvedRegion()` scanline decomposition as the standards-compatible `ext-background-effect-v1` fallback. Add one optional surface-local analytic rounded rectangle plus triangle descriptor to `BackdropEffectRegions`. When the Qt Wayland display exposes the private manager and same-display shell authentication succeeds, submit a single outward-rounded public work rectangle with a 2 logical px antialias guard and the private shape in one state batch. The work rectangle is an effect/capture/damage bound; it is not the visual coverage. The public `wl_region` remains binary, while Typhon's private final-composite coverage supplies fractional edges. The exact private XML comes from Typhon commit `a0a45fc411b3bf18920ffdff5457d05a72c345f0`.

**Geometry:** Keep the analytic QML values item-local and resolve them in the shared effects layer. Accept translation, axis-aligned reflection, and uniform scale for the rounded rect; reject rotation, shear, and non-uniform scale to the public fallback. Use the Dock bubble's shared `bodyRect`/tail points for the vector path, fallback slices, and analytic triangle. The 1 px render-padding tangential offset is corrected. Keep Sober at 72×34, radius 17, with the existing pointer, gap, material, placement, lifecycle, and 2× raster minimum.

**Lifecycle and authorization:** Discover public and private managers on the Qt Wayland connection. Reuse `TyphonShellAuthenticator` for that exact display; private auth failure must not affect public `available()`. Own no more than one public effect proxy and one lazy private coverage proxy per native surface generation, retain/clear the private proxy while inactive, and release both on surface replacement. Queue private state, public state, then one Qt frame request and one flush. Qt keeps ownership of `wl_surface.commit`.

**Verification:**

- [x] Copy and hash-check the exact Typhon protocol XML; generate optional production client code only and test-only fake-server headers.
- [x] Add deterministic geometry/work-region, transform rejection, 96-rectangle fallback, state deduplication, transition, authentication, and same-client protocol tests.
- [x] Build the required Wayland effects, Dock, Shell, Bar smoke, and private protocol test targets in `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
- [x] Focused tests passed 5/5; all Dock tests passed 14/14; relevant shared-effects/Typhon protocol tests passed 11/11; Bar/Shell smoke tests passed 2/2.
- [x] Confirm fallback counts of 41 rectangles for top/bottom and 42 for left/right; analytic mode submits one guarded work rectangle. The private manager's absence and auth rejection both retain fallback behavior in the fake compositor.
- [x] Native visual qualification was unavailable: the active Wayland registry advertises the public manager but no private coverage manager, and the native app/window inventory was empty. No Typhon Sober or Zen visual comparison is claimed.
- [ ] Run final `git diff --check`, adversarial source/diff review, append the post-verification progress entry, and commit the logical Eclipse change on `main`.
