# Dock App Tooltip Design

## Goal

Replace the Dock application's generic QtQuick Controls tooltip with a compact Eclipse themed speech bubble that points to the icon's current visual bounds.

## Behavior

- Preserve the existing 420 ms hover delay and current title/window-count text.
- Use `DockPanel.pointerTargetTaskKey` as the hover authority and the delegate's scale-aware `interactionRegion` as the anchor.
- Map that panel-local rectangle to output-local coordinates with `DockSurfaceGeometry.outputLocalDelegateRect`.
- Render the presentation as one closed vector bubble silhouette: a compact pill body and short integrated tail, with one subtle outline and one soft shadow. Use shared Shell tooltip material and font semantics.
- Place the card above a bottom Dock, inward of a left or right Dock, and clamp the card and pointer within the output. Flip the pointer edge if the preferred placement cannot fit.
- Render through a full-output, input-transparent, focusless Layer Shell surface on the Dock's output with `exclusive_zone = -1`. The tooltip reserves no desktop space and ignores other Layer Shell exclusive zones, keeping its coordinate space equal to the complete logical output. Keep it on the Top layer so Overlay context menus remain above it.
- Hide or cancel the tooltip when the pointer leaves, the target disappears, the Dock hides or autohides, a reorder starts, or a context menu opens.
- Keep the Dock's existing input mask and pointer handlers unchanged.

## Architecture

`DockPanel` remains the owner of hover, magnification, drag, and target identity. It delays presentation and publishes the current delegate identity, title, placement, and mapped visual anchor to a passive `DockTooltipSurface` window. `DockSurfaceGeometry` owns output-local anchor and body placement calculations. `DockLayerShellSurface` configures the tooltip window as a full-output passive sibling surface. `DockTooltipBubble` renders the pill body and integrated tail in one vector path; the text is laid out in the body rectangle. The QML consumes `ShellMaterialTheme` tooltip tokens and does not use QtQuick Controls.

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
- Default uses an opaque elevated tooltip fill. Frosted uses the real compositor backdrop region for a rounded body-only proxy and a regular-glass-like tint around 0.44–0.46 alpha. Transparent uses the more transparent Shell surface tint without enabling backdrop blur.
- One continuous outline and one soft shadow belong to the vector bubble. The shadow does not apply to the tooltip text.
- Frosted blur registration remains rectangular-plus-rounded: the body proxy matches the pill body and uses a 17 px radius. The compositor API does not accept the vector tail mask, so the short tail shares the translucent visual fill without registering a separate glass region. The body/tail junction still requires native Frosted visual qualification.
- This visual boundary does not change hover identity, delayed presentation, lifecycle-generation refresh, output-local geometry ownership, or the passive full-output Layer Shell surface contract above.
