# AstreaOS Global Context Menu Design

## Scope

Astrea Shell will own a reusable global Context Menu subsystem for desktop,
Dock, and StatusNotifier/Tray menus. Normal client surfaces remain application
owned. Typhon remains a generic compositor and is not changed by this design.

## Ownership and lifecycle

`ContextMenuController` is the only authority for Context Menu presentation.
It owns a single active presentation, a monotonically increasing generation,
the stable target descriptor, the action-token dispatch table, and the
`Closed -> Open -> Closing -> Closed` lifecycle. A close request is idempotent.
Replacing an open menu first invalidates the old generation and target, then
deterministically presents the new model.

QML receives model data and opaque action tokens only. Activation is routed as
`activate(generation, token)`. The controller rejects stale generations,
invalidated targets, missing/hidden/disabled actions, and provider-invalidated
actions. The controller also owns output-removal and shell-shutdown cleanup so
an unmapped surface cannot retain input or keyboard ownership.

The controller coordinates with `BarPopupController` through narrow close
operations. BarPopupController continues to own large Bar popups (Astrea Menu,
Network, Bluetooth, and Volume); it does not become the global menu controller.
Tray presentation moves to the Context Menu path while the existing Bar popup
implementation remains available to preserve current tests and large-popup
behavior during migration.

## Model and provider boundary

`ContextMenuModel` is a bounded `QAbstractItemModel` tree. Nodes are Action,
Separator, or Submenu and expose token, kind, label, icon, enabled, visible,
shortcut, check state/type, destructive, and child-state roles. Models are
normalized so leading, trailing, and duplicate separators are omitted. A
presentation is limited to eight submenu levels, 256 total nodes, and bounded
identifiers/labels; provider violations fail gracefully.

Providers create current state at presentation time:

- `DesktopContextMenuProvider` exposes real desktop actions, initially a
  normal Settings launch.
- `DockContextMenuProvider` keys targets by desktop-entry identity, projects
  current exact Typhon window IDs and titles, launches new instances through
  the desktop-entry launch path, dispatches exact-window activation/close, and
  offers pin/unpin only after a successful persistent write.
- `TrayContextMenuAdapter` preserves the live DBusMenu model, `aboutToShow`,
  revisions, nested submenus, check/radio state, icons, visibility, enabled
  state, depth limits, and remote fallback behavior. It adapts presentation
  and tokenized activation without converting the remote menu into an
  unrefreshable static snapshot.

## Surfaces and input

Each applicable output gets a transparent `DesktopInteractionSurface` on the
Bottom layer, with no exclusive zone and no keyboard interactivity. Paper
remains `WindowTransparentForInput` and never owns desktop pointer policy.
The interaction surface accepts the right-button desktop trigger and forwards
it to the controller.

An active presentation gets a dedicated `ContextMenuOverlaySurface` on the
Overlay layer. It is mapped only while the controller requires it and uses
short-lived Exclusive keyboard interactivity. The overlay owns outside pointer
and touch dismissal, Escape, and keyboard navigation; menu cards stop inside
events from reaching the dismissal shield. Destruction of an output or the
shell closes and unmaps it.

## Rendering and navigation

Reusable QML primitives render the bounded model: card, item, separator,
submenu, and view. They retain Astrea’s existing theme language and support
icons, disabled/check/radio/destructive states, shortcuts, submenu indicators,
hover/pressed/keyboard selection, entrance/exit animation, and appropriate
accessibility metadata. Mouse hover and keyboard focus share one active item.
Up/Down, Home/End, Enter/Return/Space, Right/Left, and Escape operate on
enabled visible items and never focus hidden submenu content. Submenu direction
is placement-driven and therefore ready for RTL.

## Placement

`ContextMenuPlacement` is a pure deterministic helper. It accepts output-local
geometry, a point or source rectangle anchor, menu size, submenu relationship,
and layout direction. Desktop menus prefer down/right, Dock menus prefer above
and center on the source item, and submenus prefer the logical trailing side.
All placements flip and clamp within output bounds, including narrow and
oversized-output cases. QML does not duplicate edge arithmetic.

## Desktop Actions and pin persistence

The desktop-entry parser will parse the declared `Actions=` order and matching
`[Desktop Action ...]` groups using existing decoding/localization rules. Only
declared actions with valid executable data are exposed. Launching uses the
existing supervised launcher and bounded Exec expansion; no shell command
interpolation is introduced.

`DockConfigStore` is separate from the read-only watcher. It validates the
existing config path and pin filename rules, preserves unrelated JSON/style
fields, preserves order without duplicates, enforces the existing bound,
creates the user file safely, and writes atomically with `QSaveFile`. A failed
write is reported and never reflected as a false UI state; the watcher then
reconciles a successful replacement.

## Verification

Tests cover controller lifecycle and stale activation, model normalization and
bounds, placement edge cases, surface contracts, Dock identity/window actions,
atomic pin persistence, Desktop Actions parsing/launching, QML input and
navigation, and Tray DBusMenu migration semantics. The final gates are the
configured Eclipse build and test presets, the QML gate, `git diff --check`,
and any focused tests. Typhon is not changed or claimed runtime-qualified.
