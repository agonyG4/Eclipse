# AstreaOS Global Context Menu Correctness Closure

## Scope

This closure preserves the existing Eclipse-owned global Context Menu architecture and fixes the identified integration defects. Typhon remains unchanged unless a new deterministic, compositor-independent regression proves a generic defect.

## Design

`ContextMenuController` will distinguish presentation validity from action authorization. Static presentations continue to authorize through `ContextMenuModel`; Tray presentations provide a live authorizer that resolves the current DBusMenu node, validates its prefix, visibility, enabled state, leaf/separator status, and current model, and then dispatches through the existing DBusMenu model. Generation and target validation remain mandatory for every activation.

`ContextMenuSurfaceBundle` owns output-local mapping. An Overlay maps only when the bundle is mapped, the controller has an active presentation, and the controller target output key equals the bundle output key. Removing an output synchronously settles an active presentation before destroying its bundle, while removal of an unrelated output leaves the presentation intact.

Dock delegates will provide a layout-local rectangle to a pure `DockSurfaceGeometry` helper. That helper derives the output-local rectangle from output size, centered surface size, bottom margin, and delegate rectangle; virtual desktop origins are not part of the calculation. Magnification and item position remain inputs to the same helper.

Generic and Tray submenu views will explicitly return focus to their parent after closing a child. Generic submenu placement will use the selected delegate's mapped visual rectangle, including ListView scrolling. The generic menu will use the smallest shared Shell theme-token dependency rather than importing a Bar-specific component directly.

The global Tray path becomes the only production Tray lifecycle owner. Bar retains only its four large popups. Context Menu close transitions will animate opacity/scale, disable action input immediately, and settle from animation completion; output destruction and shutdown remain synchronous lifecycle authorities.

## Verification

Add behavior-level tests for live Tray authorization, stale and removed targets, multi-output mapping/removal, Dock geometry, nested submenu focus and placement after scrolling, animation/input dismissal, and the absence of the obsolete Tray lifecycle. Run focused tests first, then the configured build, full CTest suite, QML gate, and whitespace checks. Report any runtime-only Typhon qualification separately.
