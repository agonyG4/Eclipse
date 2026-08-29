# Dock Personalization v1 Correctness Design

## Goal

Close the remaining Dock Personalization v1 correctness gaps while preserving
the shared Dock configuration contract, the unified resident Dock, the existing
Bottom behavior, the fixed magnification envelope, the bounded input region,
the icon-resolution pipeline, context menus, and identity-based pin reorder.

## Architecture

Introduce one compositor-independent runtime placement policy beside the
existing Dock surface geometry helpers. The policy consumes the stored
`DockConfig` and the controller's already-derived runtime `autoHideActive`
state, and produces the physical Layer Shell edge margin, the visual chrome
inset, and the physical-edge reveal state. `DockController` exposes these
derived values as read-only properties and emits a placement-specific change
signal whenever configuration or Typhon obstruction state changes.

`AstreaShellApplication` passes the same derived placement to
`DockLayerShellSurface`; the Layer Shell surface uses its physical edge margin
and the QML panel uses its chrome inset. In normal floating mode the surface
keeps the configured Layer Shell margin and the chrome remains at the surface
edge. While auto-hide is active, the one mapped surface uses zero Layer Shell
margin, grows only by the configured floating gap on the cross axis, keeps the
chrome inset by that gap when revealed, and places the bounded reveal target at
the physical edge. Attached mode continues to use zero gap. The exclusive zone
remains the controller's resting reservation and is zero while auto-hidden.

The panel's fixed envelope calculations remain structurally based on resting
geometry and bounded visual headroom. The runtime chrome inset is added only to
the cross-axis surface extent when the physical-edge placement is active; it is
not applied to the primary content geometry or magnification calculations.
Context-menu output-local conversion receives the actual Layer Shell margin,
so the surface origin is not double-counted when the reveal surface reaches the
output boundary.

Vertical content uses the existing primary/cross-axis model and centers the
resting grid in the chrome's cross thickness without position-specific pixel
offsets. The existing inward transform and edge-side indicator rules remain in
force.

## Testing

Extend the pure Layer Shell/policy tests for Bottom, Left, and Right with a
non-zero floating margin. They will prove normal floating placement, physical
edge reveal placement, preserved visual gap, zero auto-hidden and temporary
reveal reservation, attached placement, and intelligent obstruction transition.
Retain explicit Bottom compatibility assertions.

Extend the real offscreen `DockHoverQmlTest` with Left and Right drag cases:
Y-primary source centers and movement, both reorder directions, Y neighbor
preview displacement, inward cross-axis lift, one identity-based finish,
cancel without reorder, no activation, stable captured origin while
magnification collapses, invariant surface dimensions, and bounded input
regions. Update `Dock/docs/TESTING.md` to distinguish deterministic QML/input
arbitration from live compositor pointer delivery.

Extend the Settings static source guard with the Dock controller C++ files and
Dock Settings page, add the new controller to the production-source reuse
invariant, and keep all existing forbidden dependency tokens in force.

Deterministic tests prove policy values, QML geometry, input-region bounds,
exclusive-zone signals, and output-local arithmetic. Only a live Wayland/Typhon
qualification can prove physical pointer delivery at the output edge, routing
through the floating gap, application click-through outside the Dock mask,
and visual symmetry/feel on a real compositor. No offscreen direct controller
call is treated as proof of the physical edge reveal.

## Scope

This design does not add blur, shaders, opacity or glass controls, global theme
settings, a top Dock, another sensor process, another resident Dock,
Settings-to-Shell IPC, QML filesystem/JSON access, overlap detection, or any
rewrite of icon source resolution.
