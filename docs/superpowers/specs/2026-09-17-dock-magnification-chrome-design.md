# Eclipse Dock Magnification Chrome Design

## Goal

Restore visual Dock chrome magnification in Eclipse without resizing the
Layer Shell surface, changing shared Shell material, changing input-region or
drag geometry, modifying Typhon, or touching Dock runtime task identity.

## Approved design

`DockPanel.qml` will keep one fixed outer surface, owned by `surfaceWidth` and
`surfaceHeight`, and will add one mutable primary-axis expansion value:
`magnificationExtraPrimary`. `updateHoverEffect()` will assign that value from
the existing authoritative `totalExtra` after the delegate magnification
calculation, using zero whenever magnification is inactive. No magnification
calculation will be duplicated elsewhere.

The real `dockChrome` material rectangle will bind its width to
`restingWidth + magnificationExtraPrimary` for horizontal Docks and its height
to `restingHeight + magnificationExtraPrimary` for vertical Docks. The other
axis remains at its resting size. Existing width/height Behaviors remain the
only chrome animation. `primaryExtent()`, pointer coordinates, delegate
centers, delegate transforms, and the fixed root surface bindings remain
unchanged.

Because the existing backdrop region targets `dockChrome`, and the shared
effect implementation observes item geometry and synchronizes after animation
frames, the resolved frosted region will follow the animated chrome without
blurring unused envelope space. Existing dynamic input-region publication will
likewise continue to use the animated chrome rectangle plus delegate
interaction rectangles.

## Testing

The RED phase will update `DockHoverQmlTest.cpp` before production QML changes:

- horizontal magnification must expand chrome width, keep chrome height and
  root surface dimensions stable, and collapse on pointer exit;
- both vertical left and right Docks must expand chrome height only while the
  root surface and indicator-side semantics remain stable;
- the backdrop region must expand and contract with the settled chrome while
  remaining inside the fixed surface and retaining rounded-region semantics;
- the input mask must follow centered expanded chrome and magnified icon
  rectangles without activating transparent envelope corners;
- drag start must suspend magnification and collapse chrome without changing
  the drag target, dragged icon center, or surface dimensions;
- current expansion must stay within the preallocated maximum for
  representative horizontal and vertical configurations;
- lift and none modes, mode switches, and live model/configuration refreshes
  must leave expansion at zero or recompute it from the current delegate set.

The focused RED run must fail specifically because the current chrome remains
resting-sized. The GREEN implementation will then be followed by the focused
Dock and shared backdrop test targets, `git diff --check`, and the available
full Dock hover verification. Build artifacts remain in `build/release`.

## Scope boundaries

Only `Dock/qml/components/DockPanel.qml` and
`Dock/tests/DockHoverQmlTest.cpp` are expected to change for production and
regression coverage. The shared backdrop sources are inspection-only unless a
test proves their existing dynamic geometry capability is insufficient. No
Typhon source and no Dock runtime task-identity source will be modified.
