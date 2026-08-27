# Global Context Menu Implementation Plan

## 1. Establish failing contracts and fixtures

- Add focused C++ tests for controller lifecycle, generations, stale targets,
  action validation, model roles/bounds/normalization, and pure placement.
- Add Desktop Actions parser/launcher cases and DockConfigStore persistence
  cases, including malformed input and failed writes.
- Add QML/resource smoke coverage for the new surface and renderer contracts,
  while preserving existing Tray DBusMenu tests.
- Run the focused tests and record the expected failures before implementation.

## 2. Implement Context Menu core

- Add bounded `ContextMenuModel` and node/action types.
- Add `ContextMenuPlacement` with point, rectangle, Dock, submenu, flip, clamp,
  narrow-output, oversized-menu, and RTL-ready inputs.
- Add `ContextMenuController`, target/anchor contracts, provider dispatch, and
  explicit lifecycle/generation invalidation.
- Add a narrow Bar coordination hook so large Bar popups and Context Menus
  close each other deterministically without merging controllers.

## 3. Implement providers and Dock persistence

- Add `DesktopContextMenuProvider` with the real Settings launch action.
- Add `DockConfigStore` using validated paths, bounded pins, JSON preservation,
  ordering/deduplication, safe creation, and atomic `QSaveFile` writes.
- Extend `DesktopEntryParser`/record/catalog for declared ordered Desktop
  Actions and bounded localized data.
- Extend `ApplicationLauncher` for supervised, bounded Desktop Action Exec
  expansion without `sh -c`.
- Add Dock provider actions for current exact Typhon windows, new-instance
  launch, exact activation/close, and successful pin/unpin mutation only.

## 4. Add surfaces and shared QML primitives

- Add output-scoped Bottom desktop interaction surfaces and their manager/bundle.
- Add on-demand Overlay context-menu surfaces with Exclusive keyboard policy,
  outside/touch/Escape dismissal, and cleanup on output removal/shutdown.
- Add model-driven card/item/separator/submenu/view QML primitives with shared
  active-item state, keyboard navigation, accessibility metadata, and bounded
  placement inputs.
- Register resources and wire ShellRuntime/AstreaShellApplication ownership.

## 5. Wire Desktop, Dock, and Tray

- Route desktop right-button events to the global controller without changing
  Paper input transparency.
- Add Dock delegate right-button handling while preserving left activation,
  hover, and launch error behavior.
- Adapt Tray’s existing live DBusMenu presentation to the controller, retaining
  aboutToShow, revisions, nested menus, state, fallback, and exact semantics.
- Remove production Tray lifecycle use from BarPopupController while leaving
  large Bar popup behavior and compatibility test coverage intact.
- Update Dock documentation for persistent configuration mutation.

## 6. Verify and hand off

- Run focused Shell/ContextMenu, Dock, DesktopEntry, launcher, Bar/Tray, Paper,
  and QML tests.
- Run `cmake --build --preset debug`, `ctest --preset debug --output-on-failure`,
  `tools/ci/run-qml-gate.sh`, and `git diff --check`.
- Confirm Typhon has no modifications and report that real Wayland behavior is
  unqualified unless exercised under Typhon.
- Commit implementation in focused Eclipse-only commits and report all test
  results and remaining runtime qualification.
