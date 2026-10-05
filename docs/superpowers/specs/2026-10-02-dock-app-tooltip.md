# Dock App Tooltip Design

## Goal

Replace the Dock application's generic QtQuick Controls tooltip with a compact Eclipse themed speech bubble that points to the icon's current visual bounds.

## Behavior

- Preserve the existing 420 ms hover delay. The tooltip label is the application's `displayName` only, regardless of runtime or window-count state.
- Use `DockPanel.pointerTargetTaskKey` as the hover authority and the delegate's scale-aware `interactionRegion` as the anchor.
- Map that panel-local rectangle to output-local coordinates with `DockSurfaceGeometry.outputLocalDelegateRect`.
- Render the presentation as one closed vector bubble silhouette: a compact pill body and short integrated tail, with one subtle outline and one soft shadow. Use shared Shell tooltip material and font semantics.
- Place the card above a bottom Dock, inward of a left or right Dock, and clamp the card and pointer within the output. Flip the pointer edge if the preferred placement cannot fit.
- Render through a full-output, input-transparent, focusless Layer Shell surface on the Dock's output with `exclusive_zone = -1`. The tooltip reserves no desktop space and ignores other Layer Shell exclusive zones, keeping its coordinate space equal to the complete logical output. Keep it on the Top layer so Overlay context menus remain above it.
- Hide or cancel the tooltip when the pointer leaves, the target disappears, the Dock hides or autohides, a reorder starts, or a context menu opens.
- Keep the Dock's existing input mask and pointer handlers unchanged.

## Architecture

`DockPanel` remains the owner of hover, magnification, drag, and target identity. It delays presentation and publishes the current delegate identity, application display name, placement, and mapped visual anchor to a passive `DockTooltipSurface` window. `DockSurfaceGeometry` owns output-local anchor and body placement calculations. `DockLayerShellSurface` configures the tooltip window as a full-output passive sibling surface. `DockTooltipBubble` renders the pill body and integrated tail in one vector path and derives its Frosted backdrop rectangles from that same geometry. A matching `FontMetrics` object measures the application name before placement; the `Text` item only renders into the resulting bounded body width. The QML consumes `ShellMaterialTheme` tooltip tokens and does not use QtQuick Controls.

## Tooltip Presentation Lifecycle

Initial presentation and visible refresh are separate operations. A hidden tooltip becomes visible only when the existing 420 ms `tooltipDelay` timer resolves and presents the current eligible target. A refresh may be queued only for an already-visible tooltip; it is coalesced by lifecycle generation and task key, then revalidates both identities, tooltip visibility, and eligibility when the deferred callback executes. If the lifecycle or target changed, the surface disappeared or became hidden, or presentation became ineligible, the callback is discarded. A stale refresh never presents a replacement target or restarts a hidden tooltip.

## Constraints

- Preserve output-local logical coordinates and the existing primary-output Dock behavior.
- Do not add pointer or keyboard input to the tooltip surface.
- Keep the main Dock's exclusive-zone reservation, autohide, drag/reorder, and context-menu ownership unchanged. The tooltip itself uses `exclusive_zone = -1`: it reserves no space while ignoring other surfaces' exclusive zones.
- All build and test output stays under `/mnt/Aether/Desktop/GitHub/Eclipse-build`.

## Visual Material Boundary (2026-10-03)

- Tooltip presentation is one vector bubble silhouette with a 34 px pill body, 72–280 px body width, 16 px horizontal text padding, and 13 px single-line centered text.
- The body radius is half its height (17 px). A 14 px wide, 8 px deep tail is part of the same closed vector outline for top, bottom, left, and right placement. The vector tail is not a second card or material surface.
- `DockSurfaceGeometry` owns the shared body height, pointer width, pointer depth, 3 px target gap, placement, and pointer-center safety. The pointer-center safe inset is the smaller of half the edge length and body radius plus half the pointer width.
- The gap is measured from pointer tip to the current visual icon anchor. Placement uses pointer depth plus target gap, so the body edge sits approximately 11 px from the anchor while the tip remains approximately 3 px away.
- Default uses an opaque elevated tooltip fill. Frosted uses one compositor backdrop binding whose region is the rounded body proxy unioned with bounded tail scanlines, under a regular-glass-like tint around 0.44–0.46 alpha. Transparent uses the more transparent Shell surface tint without enabling backdrop blur.
- One continuous outline and one soft shadow belong to the vector bubble. The shadow does not apply to the tooltip text.
- Every orientation uses exact circular SVG arcs for the pill. A left/right tail replaces the center portion of its circular endcap and joins at the circle intersections. With radius r and half pointer width h, the horizontal inset is sideTailArcInset = r - sqrt(r² - h²). At the current 17 px radius and 7 px half-width, this is approximately 1.508 px. The tail tip remains 8 px beyond the extreme body edge; the path runs from one distinct endcap intersection through the tip to the other, so it does not retrace or internally stroke the endcap boundary.
- Frosted registers the body as a rounded 17 px proxy and registers the tail from at most eight one-logical-pixel local scanline rectangles generated from the vector pointer geometry. Bottom/top scans narrow toward the tip by row; left/right scans narrow by column using the derived endcap intersection. The tail uses radius 0. The descriptors share one BackdropEffectRegions binding and are enabled only while Frosted is visible. Conservative scanlines may under-cover antialiased edge pixels; they do not register the full tail bounding box.
- BackdropRegion.localRectangles is optional and item-local. An empty list retains the previous item.boundingRect() behavior; a non-empty list maps each rectangle through the owning QQuickItem transform into window content coordinates and clips it to the surface. Explicit rectangles join the existing aggregate 96-rectangle safety budget. Radius scaling follows the owner transform rather than an individual small rectangle.
- The deterministic body/tail region and circular-endcap regressions qualify the geometry model. Native Frosted appearance still requires live Wayland qualification and is recorded separately from those tests.
- This visual boundary does not change hover identity, delayed presentation, lifecycle-generation refresh, output-local geometry ownership, or the passive full-output Layer Shell surface contract above.

## Circular Endcap and Frosted Tail Hardening (2026-10-03)

The visual silhouette remains one Shape, one closed ShapePath, one fill, one outline, and one shadow. Top and bottom tails remain attached to the straight body edge; left and right tails replace only the center of the corresponding radius-17 endcap. Their base points are derived from bodyX + sideTailArcInset (left) and the mirrored right coordinate, with sideTailArcInset = radius - sqrt(radius² - halfPointer²). This avoids the old path's midpoint-to-base retrace, which stroked an internal vertical seam because a 34 px body has no straight side segment between its 17 px endcaps.

Frosted blur is the rounded body region unioned with a bounded scanline approximation of the tail. The local-rectangle descriptor maps through the bubble's actual item transform, clips to the output surface, and shares the existing 96-rectangle client budget. The tail scanlines follow the same pointer width, depth, edge, center, and body coordinates as the vector path. No tail bounding-box blur or second effect binding is used. Default and Transparent continue to request no backdrop blur, and the tooltip material tokens and target-gap geometry remain unchanged.

## Tooltip Text Sizing and Unified Frosted Silhouette (2026-10-04)

This follow-up records the current text and rendering boundaries; the dated 2026-10-03 material and backdrop descriptions above preserve their original qualification history.

- `DockPanel` supplies `targetItem.displayName` verbatim. Runtime-known/running state and `windowCount` remain application-model metadata and do not alter the tooltip label.
- `DockTooltipSurface` uses `FontMetrics` with the same family, pixel size, and medium weight as the visual text. Its `advanceWidth(title)` is the natural text width; the natural card width is `ceil(advanceWidth) + 32 px`. The body is at least 72 px when output space allows, has 16 px horizontal padding per side, and grows to a 420 px design maximum. Its effective maximum is also bounded by the logical output width minus the existing 8 px inset on both sides. A one-line `NoWrap`, `PlainText`, `ElideRight` text item elides only after reaching that bound.
- `DockTooltipBubble` remains the vector silhouette authority: the existing 34 px body, radius-17 circular arcs, 14 px pointer, 8 px depth, 3 px target gap, and one path/shadow are preserved. A 1 px `renderPadding` enlarges only the Shape drawing container, keeping the centered 1 px outline and antialiased source inside the texture. It is excluded from `cardRect`, body placement, target anchoring, and gap calculations.
- Frosted uses one `BackdropRegion` descriptor on `DockTooltipBubble`, with `localRectangles` set to `backdropRectangles`. The bubble derives body and tail slices from its local path geometry and concatenates them. The 34 body rows use the capsule circle equation and integer-contained rectangles; integer-contained tail scanlines retain the bounded-tail architecture. The descriptor uses radius 0, so the compositor receives the exact body-and-tail rectangle union without a separate rounded-body proxy or full bounding-box blur.
- The deterministic all-edge regression maps the resolved output region and confirms pill center/edge-center inclusion, rounded-corner exclusion, body/card equality, unchanged three-pixel tail gap, tail inclusion, outside-tail exclusion, and the existing 96-rectangle ceiling. Latest counts are bottom 34 body + 6 tail (40 resolved), top 34 + 6 (40), left 34 + 7 (41), and right 34 + 7 (41).
- Default/Transparent/Frosted token values, hover ownership and timing, placement policy, Layer Shell policy, and Dock input behavior remain unchanged. Native Frosted visual qualification is recorded separately and remains unobserved when no attachable native desktop is available.

## Short Tooltip Edge Raster Quality (2026-10-05)

The tooltip has two raster boundaries with different capabilities. The visible bubble remains the existing continuous `ShapePath` rendered by `Shape.CurveRenderer`; its `MultiEffect` shadow still uses that Shape as an offscreen source. `DockTooltipBubble` now sizes that layer to `ceil(item width × rasterScale)` by `ceil(item height × rasterScale)` and uses smooth linear sampling when the texture is composited back to the logical item size. `rasterScale` is `max(2, validated Screen.devicePixelRatio)`: 1× and 1.5× outputs use 2×, 2× uses 2×, and a 3× output uses 3×. DPR validation falls back to 1 only for invalid or non-positive values and does not cap valid high-DPR outputs.

The 72×34 logical pill body, radius 17, 14×8 integrated pointer, 3 px target gap, 1 px outline, 1 px render padding, and path arcs are unchanged. For a bottom-pointer Sober tooltip, the complete bubble item is 74×44 logical pixels and requests a 148×88 texture at 2×. These dimensions describe only the raster layer; card placement and pointer geometry remain in logical coordinates.

The Frosted backdrop remains a binary integer `wl_region` approximation. The body now samples each row at `rowY + 0.5`, evaluates the exact capsule circle equation there, and includes pixels whose centers fall within the resulting horizontal interval. The previous rule required the entire 1 px row band to fit inside the pill. On the first row of a 72×34 body, that conservative rule started at `bodyX + 17` and covered 38 pixels; pixel-center inclusion starts at `bodyX + 13` and covers 46. C++ QML regressions independently enumerate analytic capsule pixel centers, verify adjacent excluded pixels, and compare the new span with that old diagnostic rule.

Top/bottom tail slices sample each row center and derive their triangle width from `pointerWidth` and `pointerDepth`. Left/right slices sample each column center and derive their height from the integrated side-tail triangle and `sideTailArcInset`. The all-edge reference tests compare every generated scanline to those analytic pixel-center samples. Current body/tail/resolved rectangle counts are top 34+7=41, bottom 34+7=41, left 34+8=42, and right 34+8=42, all below the existing 96-rectangle budget.

`wl_region` carries integer rectangle add/subtract operations and cannot express fractional pixel coverage. Pixel-center inclusion is the nearest practical bounded binary raster approximation; it does not make the Frosted mask mathematically antialiased. The compositor still receives exactly one `BackdropEffectRegions` and one `BackdropRegion` for `tooltipBubble.backdropRectangles`. No Typhon protocol change is included.

The deterministic Sober case retains a 72 px card, full untruncated text, 34 px body height, 17 px radius, unchanged 14×8 pointer, and the 3 px target gap. The existing Zen Browser full-name regression remains in place. No native Eclipse/Typhon window or native desktop pipe was available during this pass, so Default-versus-Frosted visual similarity and any remaining visible quantization were not observed. A future compositor soft-mask task is only justified if native Frosted qualification finds the remaining integer-mask edge unacceptable.
