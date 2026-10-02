# Dock App Tooltip Design

## Goal

Replace the Dock application's generic QtQuick Controls tooltip with a compact Eclipse themed speech bubble that points to the icon's current visual bounds.

## Behavior

- Preserve the existing 420 ms hover delay and current title/window-count text.
- Use `DockPanel.pointerTargetTaskKey` as the hover authority and the delegate's scale-aware `interactionRegion` as the anchor.
- Map that panel-local rectangle to output-local coordinates with `DockSurfaceGeometry.outputLocalDelegateRect`.
- Show a rounded elevated card with a subtle border, drop shadow, and a small pointer. Use shared shell material, text, font, and radius tokens.
- Place the card above a bottom Dock, inward of a left or right Dock, and clamp the card and pointer within the output. Flip the pointer edge if the preferred placement cannot fit.
- Render through a full-output, input-transparent, focusless Layer Shell surface on the Dock's output with `exclusive_zone = -1`. The tooltip reserves no desktop space and ignores other Layer Shell exclusive zones, keeping its coordinate space equal to the complete logical output. Keep it on the Top layer so Overlay context menus remain above it.
- Hide or cancel the tooltip when the pointer leaves, the target disappears, the Dock hides or autohides, a reorder starts, or a context menu opens.
- Keep the Dock's existing input mask and pointer handlers unchanged.

## Architecture

`DockPanel` remains the owner of hover, magnification, drag, and target identity. It delays presentation and publishes the current delegate identity, title, placement, and mapped visual anchor to a passive `DockTooltipSurface` window. `DockSurfaceGeometry` owns output-local anchor and card placement calculations. `DockLayerShellSurface` configures the tooltip window as a full-output passive sibling surface. The QML card consumes `ShellMaterialTheme` tokens and does not use QtQuick Controls.

## Constraints

- Preserve output-local logical coordinates and the existing primary-output Dock behavior.
- Do not add pointer or keyboard input to the tooltip surface.
- Keep the main Dock's exclusive-zone reservation, autohide, drag/reorder, and context-menu ownership unchanged. The tooltip itself uses `exclusive_zone = -1`: it reserves no space while ignoring other surfaces' exclusive zones.
- All build and test output stays under `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
