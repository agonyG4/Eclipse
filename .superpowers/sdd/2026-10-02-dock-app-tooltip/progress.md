# Progress: Dock App Tooltip

Ruling: stay on `main` and do not create a worktree or branch — the repo instructions require `main` and prohibit branches — cost if wrong: changes share the current checkout.

Ruling: execute inline and self-review the final diff — the repo instructions prohibit subagents — cost if wrong: no independent fresh-context reviewer.

Ruling: use the approved investigation design as the spec authority — the user approved the architecture and implementation plan — cost if wrong: the implementation may need a small design correction.

## Tasks

- [x] Task 1: output-local tooltip geometry and passive Layer Shell policy.
- [x] Task 2: themed tooltip surface and Dock presentation wiring.

Verification: `astrea-shell` built from `/mnt/Aether/Desktop/GitHub/Eclipse-build`; all 14 Dock CTest cases passed, including magnification tracking, Dock interaction suppression, placement fallback, and Layer Shell policy.

## Tooltip lifecycle race hardening (2026-10-02)

- [x] Confirmed the stale callback race before implementation: the geometry-refresh and `modelChanged` regressions both presented replacement target B before its configured delay.
- [x] Split ownership between the 420 ms initial-presentation timer and lifecycle-qualified visible refreshes. Each refresh captures lifecycle generation and task key, coalesces matching requests, and revalidates generation, key, surface existence/visibility, and eligibility before resolving the current target.
- [x] Added deterministic regressions for geometry-triggered and model-triggered stale refreshes; both fail against the old implementation and pass with the lifecycle guard.
- [x] Verified width and height changes produce one settled re-presentation, retained the pending replacement/removal and magnification/interaction regressions, and passed the focused Dock hover QML test.

## Unified tooltip visual hardening (2026-10-03)

- Replaced the rounded card plus rotated-square pointer with one `QtQuick.Shapes` vector bubble: a 72–280 px wide, 34 px high pill body with a 14 px wide, 8 px deep integrated tail for all four edges. The body radius is 17 px, and the closed silhouette owns a single 1 px outline and one soft shadow.
- Kept `DockSurfaceGeometry` as the shared authority for body height, pointer dimensions, and the 3 px pointer-tip target gap. Placement uses 11 px from the body edge to the anchor and clamps the pointer center at `min(edgeLength / 2, bodyHeight / 2 + pointerWidth / 2)` so the tail base clears the pill corner arc.
- Added semantic tooltip surface, border, and shadow colors. Frosted uses a moderate 0.44–0.46-alpha theme-derived tint over compositor blur; Transparent retains its lighter existing-style tint without enabling blur; Default retains the elevated fill.
- Kept the backdrop source as a non-rendering rounded body proxy. The tail shares the translucent visual fill but has no separate glass layer; arbitrary-shape tail blur is outside the current backdrop-region protocol.
- Updated deterministic geometry and QML regressions. `dock-hover-qml-test`, `dock-surface-geometry-test`, and `bar-qml-smoke-test` passed (3/3); complete Dock CTest passed (14/14). `astrea-shell` and all requested targets built in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; `git diff --check` passed.
- Native visual qualification was not performed: no running Astrea Shell or Typhon process was available, and desktop automation could not attach. The body/tail material junction therefore remains visually unqualified on a live compositor.

## Circular Side Tail and Frosted Region Hardening (2026-10-03)

- Root cause of the vertical-tail stroke seam: the 34 px body is exactly two 17 px endcaps, so the left/right edge has no straight center segment. The old path reached the left midpoint `(bodyX, 17)`, then followed `V 24 → L (bodyX - 8, 17) → L (bodyX, 10) → V 17`. That retraced the endcap boundary around the pointer base and stroked an internal seam.
- Rebuilt all four orientations with SVG circular arcs. Side tails now splice between two distinct circle intersections and share one closed path, one fill, one outline, and one shadow. The base inset is `radius - sqrt(radius² - halfPointer²)`; with radius 17 and half-width 7 it is approximately 1.508 px.
- Added the optional `BackdropRegion.localRectangles` QVariantList. Empty lists preserve whole-item rounded behavior; explicit item-local rectangles map through `QQuickItem::mapRectToItem`, clip to window content bounds, and join the existing 96-rectangle aggregate budget. Owner transform scale controls radius scaling.
- Frosted now registers the rounded body proxy unioned with generated tail scanlines in the same binding. Top/bottom use up to seven non-empty 1 px high slices; left/right use eight 1 px wide slices derived from the circular-tail geometry. Default and Transparent remain blur-free.
- Resolved tooltip backdrop counts in the deterministic all-edge regression: bottom 39, top 39, left 40, right 40 rectangles (each ≤ 96). The scanline tests confirm tail coverage and reject points outside the triangular tail bounding area.
- Verification: requested targets `backdrop-effect-regions-test`, `dock-hover-qml-test`, `dock-surface-geometry-test`, and `astrea-shell` built from the verified Aether build tree. Focused tests passed 3/3; complete Dock CTest passed 14/14; relevant shared-effects CTest passed 3/3. One earlier full Dock-hover run transiently failed a model-move pointer-target assertion; the isolated case and subsequent full reruns passed without code changes.
- Native bottom/left/right Frosted visual qualification was unavailable: no Astrea Shell or Typhon process was running, and the CUA native pipe was unavailable. Whether a body/tail optical seam remains and any visual difference from the macOS reference are therefore unqualified.

## Tooltip Text and Frosted Pill Fidelity (2026-10-04)

- Root cause of premature text elision: card width used `tooltipText.implicitWidth` while `tooltipText.width` was already derived from the placed card, creating a measurement/placement feedback loop; the old 280 px cap also truncated content before the output required it. The running-app branch additionally appended window-count metadata to the application label.
- Replaced that loop with `FontMetrics.advanceWidth(displayName)` using the exact visual font. Natural card width is `ceil(textWidth) + 32 px`, clamped to a 72 px minimum and the smaller of the 420 px design maximum or available output width after 8 px edge insets. `Zen Browser` measures 78.5469 px and produces an 111 px card; the visible text is full and not truncated. A measured medium-long name grows beyond 280 px. A 1000-character label on a 330 px output is bounded at 314 px and elides on one line.
- The tooltip label is now `displayName` regardless of runtime state. The regression presents `Zen Browser` for a running delegate with four windows while preserving `windowCount == 4` in the model.
- Added 1 px `DockTooltipBubble.renderPadding` around the existing path inside the Shape item. The bubble body still maps exactly to `cardRect`; for all four edges the tail tip retains the existing 3 px target gap. Padding affects neither placement nor the visible card size.
- Replaced the separate rounded body proxy plus tail descriptor with one `BackdropRegion` on `DockTooltipBubble`. Its `backdropRectangles` combine 34 exact-circle body rows with integer-contained tail slices: bottom 34+6 (40 resolved), top 34+6 (40), left 34+7 (41), right 34+7 (41). Resolved-mask checks include the pill center/edge centers and tail, exclude the radius-17 corners and outside-tail points, and remain below the 96-rectangle budget.
- Built `dock-hover-qml-test`, `dock-surface-geometry-test`, `backdrop-effect-regions-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`. Focused tests passed 3/3, the complete Dock set passed 14/14, and the shared-effects subset passed 3/3. `git diff --check` passed.
- Native Frosted qualification was not possible: Wayland was present, but no Eclipse/Typhon window was running and CUA reported that the native pipe path was unavailable. No live comparison to the macOS visual reference is claimed.

## Short Tooltip Raster Quality (2026-10-05)

- Reproduced the compact-pill issue in the source model: the old body mask required an entire 1 px row band to fit inside the capsule. Its 72×34 top row started at `bodyX+17` and covered 38 pixels; the new exact-circle pixel-center row starts at `bodyX+13` and covers 46.
- Supersampled the already-layered `CurveRenderer` Shape at `max(2, validated Screen.devicePixelRatio)` with explicit `layer.textureSize` and smooth sampling. Sober requests a 148×88 texture for its complete 74×44 bottom-pointer bubble at 1×, 1.5×, and 2×; a 3× output requests 222×132.
- Changed body and integrated top/bottom/left/right tail backdrop slices to analytic pixel-center rasterization. The C++ reference checks all capsule rows, adjacent excluded pixels, and tail scanlines for every orientation. Counts are top 34+7=41, bottom 34+7=41, left 34+8=42, right 34+8=42; the maximum stays below the 96-rectangle budget.
- Sober remains a 72 px, untruncated tooltip with 34 px height, radius 17, 14×8 pointer, and 3 px gap. Zen Browser remains full at 111 px. Default/Frosted native comparison was unavailable because the desktop inventory contained no native app/window and the CUA native pipe was missing; no visual attribution is claimed from that absence.
- Built `dock-hover-qml-test`, `dock-surface-geometry-test`, `backdrop-effect-regions-test`, and `astrea-shell` in `/mnt/Aether/Desktop/GitHub/Eclipse-build`. Focused CTest passed 3/3, complete Dock CTest passed 14/14, and relevant shared-effects CTest passed 3/3. DPR-focused QML runs passed at 1×, 1.5×, 2×, and 3×.

## Typhon Analytic Background Coverage (2026-10-09)

- Copied `protocols/astrea-background-effect-coverage-v1.xml` byte-for-byte from Typhon commit `a0a45fc411b3bf18920ffdff5457d05a72c345f0` (Git blob `0f65ab3bcc31e81214d77750f3a29365fd222d39`). Eclipse generates the optional production client header/source only for Wayland + Typhon builds and keeps generated server headers inside the focused fake-compositor test target.
- Kept public Frosted capability defined solely by `ext-background-effect-v1` plus its Blur capability. The private manager and `TyphonShellAuthenticator` use the exact Qt Wayland display that owns the Qt-created tooltip surface. No analytic code uses `TyphonSharedConnection`; rejected auth and a missing private global leave public fallback enabled.
- Added one optional item-local `BackdropAnalyticCoverage` descriptor and renderer-independent resolved value per surface. Its rounded rect accepts translation, axis-aligned reflection, and uniform scale; unrepresentable rounded transforms fall back to scanlines. One 2 logical px guarded, outward-rounded public work rectangle bounds the shape and clips to the window; private shape points stay surface-local. `wl_region` remains binary; the private Typhon shape supplies fractional final-composite coverage.
- The public/private update queues private rounded rect/triangle or clear before the public region/removal, requests one Qt frame, and flushes once. One private proxy is reused per native surface generation and cleared while inactive; both proxy types reset at surface replacement. Qt remains responsible for commit publication.
- The Dock now exports and shares `bodyRect`, `tailTip`, `tailBaseA`, and `tailBaseB` across the vector path, binary fallback, and private triangle. The earlier one-pixel tail discrepancy came from the path using `pointerCenter` without the body origin while fallback included `bodyX`/`bodyY`; `tailCenter` now includes that origin and therefore accounts for the 1 px render padding. Sober remains 72×34, radius 17, with unchanged pointer, gap, material, and 2× raster minimum.
- Deterministic fallback counts remain top/bottom 41 and left/right 42. Analytic mode resolves to one coarse public rectangle. The parity test compares every binary pixel center against capsule union triangle for all four edges; transform, clipping, 96-rectangle budget, mode transitions, deduplication, and proxy generation tests pass.
- Build succeeded for `wayland-effects-state-test`, `backdrop-effect-regions-test`, `background-effect-coverage-protocol-integration-test`, `dock-hover-qml-test`, `dock-surface-geometry-test`, `astrea-shell`, `bar-qml-smoke-test`, and `shell-unified-runtime-integration-test`. Focused effects/Dock/Bar/Shell CTest passed 7/7; complete Dock CTest passed 14/14; relevant shared-effects and Typhon protocol CTest passed 11/11. The fake compositor verified same-client authorization and surface ownership, private requests before public coarse region/commit, missing-global fallback, and auth-rejection fallback.
- Native Typhon visuals were not qualified. The active Wayland registry exposes the public background-effect manager but no private coverage manager, and the CUA app/window inventory was empty. No Sober or Zen Default/Frosted comparison or private-path-active visual claim is made.
- No Typhon source was changed. Its pre-existing working-tree edits were left untouched.
