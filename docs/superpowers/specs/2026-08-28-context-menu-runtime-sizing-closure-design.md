# AstreaOS Context Menu Runtime Sizing Closure

## Scope

This focused closure fixes Eclipse's shared Context Menu geometry and the
remaining Desktop right-click observability/runtime path. It preserves the
existing `ContextMenuController`, `ContextMenuModel`, `ContextMenuPlacement`,
`ContextMenuSurfaceBundle`, `DesktopInteractionSurface`, global fullscreen
Overlay, generation protection, output-scoped mapping, Dock actions, Tray
DBusMenu integration, keyboard navigation, submenu lifecycle, and Paper input
contract. Typhon and its layer-shell configuration are out of scope.

## Design

### One generic metric authority

`ShellMenuTheme.qml` becomes the single generic Context Menu metric authority.
It retains the current font/theme values and adds the shared sizing contract:

- minimum width: 200 logical px;
- maximum width: 260 logical px;
- normal action/submenu row: 36 logical px;
- separator presentation row: 10 logical px;
- card padding: 10 logical px on each side;
- output edge margin: 8 logical px;
- existing row/icon/shortcut spacing and icon/check slot metrics.

`ContextMenuModel` measures visible root rows using the supplied Astrea font
family and sizes with Qt font metrics. The natural width accounts for card
padding, row margins, icon/check slot, label, shortcut, submenu arrow,
spacing, and card border chrome. `ContextMenuView` clamps that natural width
to the shared minimum/maximum contract and the available output width. The
same resolved width is consumed by the root menu, Dock menu, and generic
submenu Loader. Tray keeps its separate DBusMenu sizing contract.

### Exact content height and bounded viewport

`ContextMenuModel::presentationContentHeight(normalRowHeight,
separatorHeight)` deterministically sums visible root rows, assigning zero to
hidden nodes. The view's desired height is that exact content height plus the
card's top and bottom padding. It is resolved against output height minus two
edge margins. The exact model height, not `ListView.contentHeight`, is the
initial intrinsic sizing authority.

When desired height exceeds the available viewport, the generic `ListView`
receives the resolved viewport height, remains interactive, and uses its
existing `ListView.Contain` keyboard positioning. This preserves reachability
of the last action and enables pointer wheel scrolling. Normal Dock menus on
the qualified 1920x1080 output fit without scrolling.

Model reset/data changes invalidate the view's exact metric binding, and
submenu views use the same model-side contract independently.

### Placement order and diagnostics

The presentation order is explicit:

```text
model -> exact natural width/height -> output constraints
      -> final resolved width/height -> ContextMenuPlacement -> render
```

The shared 8 px edge margin is passed into placement. Point placement keeps
the click point as output-local coordinates, flips at right/bottom overflow,
and clamps after flipping. Dock placement remains horizontally centered on the
application rectangle and prefers above-Dock placement.

`DesktopInteractionSurface` emits a structured debug record when
`ASTREA_CONTEXT_MENU_DEBUG=1`, including output-local surface dimensions,
mouse coordinates, output key/origin/size, and window dimensions. The Overlay
debug record includes the controller anchor, model row count, natural and
resolved dimensions, final coordinates, and explicit placement input/output
fields. Together with the controller's existing presentation record, a real
right-click can be followed from source event to rendered card without scale
or compensation offsets.

### Test coverage

Focused deterministic tests cover exact height for one, three, mixed,
hidden, and submenu rows; width for short/medium/long labels, shortcuts, and
arrows; one-item Desktop presentation; all four point-placement corners;
bounded tall-menu scrolling; realistic Dock rows and final-row containment;
and immediate Dock-to-Desktop and Desktop-to-Dock model replacement. The
existing layer-shell policy and Typhon isolation tests remain unchanged.

Runtime qualification uses the requested debug environment variables and
records both the baseline fixed-width/fixed-height values and the final
resolved values. If the current session cannot deliver a desktop-background
click to the Bottom-layer surface because regular windows cover the output,
that limitation is reported explicitly rather than treated as a coordinate
failure.

## Self-review

- No generic `280` width remains in Card, Overlay, View, or submenu sizing.
- No arbitrary height padding is introduced; height is the model sum plus
  card padding and is capped only by available output space.
- Layer-shell policy and Typhon sources are not modified.
- Desktop actions remain the real `Settings` action; no filler actions are
  added.
- The design is one implementation-sized subsystem: model metrics, shared
  QML sizing/placement, diagnostics, and their focused tests.
