# M8-C.1.3 StatusNotifier Interoperability Polish and Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Close the remaining StatusNotifier watcher, host, icon, DBusMenu, lazy-submenu, geometry, qualification, and documentation gaps without changing the existing ShellRuntime-owned architecture.

**Architecture:** Keep one `StatusNotifierService` owned by `ShellRuntime`. Make `StatusNotifierWatcherBridge` the sole authority for both watcher aliases and the live host registry, keep item/menu generations on every asynchronous path, and route both initial and live DBusMenu data through one bounded parser/property application path. Keep recursive submenu presentation in the existing QML cards, but defer child lookup until `AboutToShow` has completed.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/DBus/Test, CMake/CTest, isolated `dbus-run-session`, production-QRC QML tests.

## Global Constraints

- Do not redesign the architecture, create a second tray service, move ownership away from `ShellRuntime`, or start M8-D.
- Preserve the existing isolated D-Bus fixture, path-only registration, unique-owner handling, typed ToolTip/pixmap parsing, ContextMenu actions, coordinate helper, live DBusMenu updates, subtree reconciliation, toggles, separators, and recursive menus.
- Enforce DBusMenu limits on `GetLayout`, `LayoutUpdated`, `ItemsPropertiesUpdated`, and `AboutToShow` refreshes: depth 8, 2048 nodes, 256 children, 512 label characters, and 1 MiB icon data.
- Keep malformed menu icon data from deleting its menu node; ignore only the icon and retain the item.
- Leave unrelated existing worktree changes unstaged and unmodified.

---

### Task 1: Add failing closure tests

**Files:**
- Modify: `shared/tests/StatusNotifierTest.cpp`
- Modify: `shared/tests/StatusNotifierIntegrationTest.cpp`
- Modify: `shared/tests/StatusNotifierDBusFixture.cpp`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- Tests cover alias conflicts, host naming/lifetime, icon precedence, all DBusMenu limits, removed properties, lazy submenu ordering, recursive output bounds, and start-stop-start signal uniqueness.
- The integration fixture exposes enough state to assert `NameHasOwner`, host cleanup, icon name/pixmap changes, lazy subtree creation, and one signal per registration.

- [ ] Add focused tests for two watcher aliases with different owners; assert deterministic freedesktop authority and a health warning without a second local registry.
- [ ] Add tests for the specification-compatible host name, live ownership, unique-owner disappearance, and `hostRegistered`/D-Bus property consistency.
- [ ] Add icon tests where both name and pixmap exist, then remove the name; assert named theme output first and pixmap fallback second, with only the affected item revision changing.
- [ ] Add parser tests with custom `DBusMenuLimits` for depth, nodes, children, label length, byte size, dimensions, malformed icon data, and the same limits through live property/layout updates.
- [ ] Add removed-property tests for label, enabled, visible, icon-name, icon-data, toggle-type, toggle-state, and children-display; assert defaults are restored and the node remains present.
- [ ] Add an integration fixture mode where a submenu starts without children, records `AboutToShow`, then exposes children; assert the child model appears only after the reply.
- [ ] Add QML source/production-QRC coverage for right-edge and multi-level submenu placement, verifying `x + submenuWidth` against the current output width and vertical clamping.
- [ ] Run the focused tests before production changes and confirm each new assertion fails for the identified baseline gap.

### Task 2: Converge watcher aliases and generations

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.hpp`
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`

**Interfaces:**
- Add a single authority-selection path that considers both well-known aliases before ownership and re-evaluates both while owned.
- Every async owner lookup, item enumeration, host verification, registration, property read, and signal transition captures watcher generation plus owner and alias where applicable.
- Authority transitions detach all old watcher signals and clear old items before attaching or owning the selected authority.

- [ ] Represent the selected authority as `(alias, owner)` and compare both aliases on every owner-change event; if owners differ, retain freedesktop precedence and emit one health warning.
- [ ] Before claiming either alias, re-read both owners; after claiming, verify the compatibility alias still resolves to the same owner and relinquish the owned registry if an external alias appears.
- [ ] Remove the `Owned` early-return that prevents dynamic convergence; use a guarded transition function so owned/external/unavailable transitions are idempotent.
- [ ] Increment `watcherGeneration` for every transition and capture generation, alias, and expected owner in all pending callbacks; ignore stale replies and stale `PropertiesChanged`/registration events.
- [ ] Centralize external signal subscription teardown for item registered/unregistered, watcher properties, and owner watches; prove start-stop-start emits one registration event.
- [ ] Run the watcher unit tests and isolated-bus integration test, including the alias-conflict and restart cases.

### Task 3: Make host naming, lifetime, and authority truthful

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.hpp`
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.cpp`
- Modify: `shared/tests/StatusNotifierTest.cpp`
- Modify: `shared/tests/StatusNotifierIntegrationTest.cpp`

**Interfaces:**
- Host names use `org.freedesktop.StatusNotifierHost-<unique-id>` and are owned on the session bus before `RegisterStatusNotifierHost`.
- The live host registry maps host service to unique owner and is the source for both local `IsStatusNotifierHostRegistered` and health reporting.

- [ ] Generate a per-shell host name from the session unique owner and a collision-safe instance suffix, validate it, and require `registerService` success before advertising it.
- [ ] Watch every registered well-known host and its unique owner; remove host records immediately when either the well-known name or unique owner disappears.
- [ ] Ensure pending host-owner verification is invalidated by request generation and owner changes.
- [ ] Query/track external watcher host registration from the same live registry projection rather than setting an unrelated boolean after a successful method reply.
- [ ] Assert host disappearance changes `IsStatusNotifierHostRegistered` and `healthJson()` immediately, then run `git diff --check`.

### Task 4: Correct themed icon priority and targeted invalidation

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierIconStore.cpp`
- Modify: `shared/statusnotifier/StatusNotifierIconStore.hpp`
- Modify: `shared/tests/StatusNotifierTest.cpp`

**Interfaces:**
- Normal icon resolution is `IconName` through the existing XDG/theme resolver, then `IconPixmap`.
- Attention resolution is `AttentionIconName`, then `AttentionIconPixmap`, then the normal resolved icon.
- Overlay resolution is `OverlayIconName`, then `OverlayIconPixmap`, without flushing unrelated entries.

- [ ] Add a small selection helper that receives a named image and a pixmap list and returns named-first fallback behavior.
- [ ] Preserve the current bounded ARGB32 decoder and use it only after named lookup fails.
- [ ] Keep `updateItem()` and item removal scoped to one key; keep theme changes scoped to entries whose theme assets are affected and preserve per-item revisions.
- [ ] Run the icon priority/revision tests and the integration fixture’s icon-change sequence.

### Task 5: Unify bounded DBusMenu parsing and removed-property cleanup

**Files:**
- Modify: `shared/statusnotifier/DBusMenuModel.hpp`
- Modify: `shared/statusnotifier/DBusMenuModel.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Modify: `shared/tests/StatusNotifierTest.cpp`
- Modify: `shared/tests/StatusNotifierIntegrationTest.cpp`

**Interfaces:**
- `parseMenuLayout`, `parseMenuLayoutArgument`, live `LayoutUpdated`, live `ItemsPropertiesUpdated`, and `AboutToShow` subtree refresh all use `DBusMenuLimits` and the same node/property helpers.
- Removed properties restore protocol defaults: empty strings/bytes, `enabled=true`, `visible=true`, `state=0`, and no separator/toggle/children-display flags unless explicitly present.
- Invalid icon bytes are rejected by the icon validator only; the node remains in the model.

- [ ] Move icon safety limits into `DBusMenuLimits` and make `isSafeIconData()` enforce the passed byte, dimension, pixel, and decodability limits; remove unused parameters.
- [ ] Make layout parsing and node decoration return a valid node with an empty icon source when icon bytes are malformed, while preserving an error for structural/limit violations.
- [ ] Create one normalized property application helper for initial node construction and live updates; apply removal lists after changed properties and clear every listed field.
- [ ] Treat `children-display=submenu` as a child affordance even when the initial node has no children, so lazy submenus can be opened.
- [ ] Apply the same bounded parser to all live subtree refreshes and ignore stale revision/generation replies.
- [ ] Run shared unit and real-bus integration tests, including malformed live payloads and item/menu teardown while requests are pending.

### Task 6: Enforce AboutToShow-before-child lookup and output-safe geometry

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierService.hpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- Add a service signal for menu-content state changes so QML can retry a pending child after the asynchronous `AboutToShow`/layout sequence.
- QML stores `(ownerModel,nodeId,anchorY)` as pending state, calls `aboutToShowMenu()` before any child lookup, and resolves the child only after the menu client leaves `Loading`.

- [ ] Connect each `DBusMenuClient::changed` signal to a per-item service notification without resetting unrelated menu presentation.
- [ ] Change both root and recursive menu cards to defer `childModel(nodeId)` until the pending node’s AboutToShow refresh reaches `Ready`/`Empty` and the model exposes the child.
- [ ] Add generation-safe pending state cancellation on context change, popup close, item removal, and menu stop.
- [ ] Replace origin-only submenu checks with right-candidate bounds `candidateX + submenuWidth <= outputWidth`; prefer right, then left, then clamp within the current output and clamp Y at every depth.
- [ ] Add QML smoke assertions for center, right edge, multiple submenu levels, and a narrow/multi-output-sized presentation parent.

### Task 7: Qualification, report, and one scoped commit

**Files:**
- Create: `docs/M8-C.1.3_IMPLEMENTATION_REPORT.md`
- Modify: `shared/statusnotifier/README.md`
- Modify: all changed source and test files above

- [ ] Run `statusnotifier-test`, `statusnotifier-dbus-integration-test`, BarCore, BarQmlSmoke, lifecycle, ShellRuntime, Theme, Dock, Spotlight, AltTab, Paper, shared-system, and legacy guard suites with RTK.
- [ ] Run supported Debug, Release, Clang, ASan, and UBSan configurations; record actual pass/fail output and environmental sanitizer caveats.
- [ ] Qualify available installed real applications without installing software; record exact applications tested or explicitly report none were available.
- [ ] Update the StatusNotifier README to describe alias authority, host lifetime, icon precedence, live DBusMenu bounds/removals, AboutToShow ordering, geometry, tests, and limitations without claiming unrun checks.
- [ ] Write the final report with feature-scenario counts separate from QTest framework cases and with manual visual/real-application limitations explicit.
- [ ] Verify only M8-C.1.3 files are staged, preserve unrelated worktree changes, run `git diff --check`, and create one commit titled `M8-C.1.3: close StatusNotifier interoperability polish`.
