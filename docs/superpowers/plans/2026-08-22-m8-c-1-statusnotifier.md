# M8-C.1 StatusNotifier Protocol Correctness and Interoperability Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Close the committed M8-C implementation's protocol, lifecycle, wire-decoding, geometry, DBusMenu, and real isolated-session-bus interoperability gaps in one scoped milestone.

**Architecture:** Preserve one process-wide `StatusNotifierService` owned by `ShellRuntime` and shared by all bar outputs. Centralize D-Bus name and wire-type handling in the shared StatusNotifier library, keep watcher/host/item authority in `StatusNotifierWatcherBridge` and `StatusNotifierItemProxy`, and keep DBusMenu state keyed by node IDs. Use one `BarLayoutMetrics::trayAnchor` result for action, popup, and tooltip coordinates.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/DBus/Test, Qt D-Bus adaptors and pending calls, Qt Quick QML, CMake/CTest, `dbus-run-session` where available.

## Global Constraints

- Do not replace the existing M8-C architecture.
- Do not introduce `qdbus`, `gdbus`, `dbus-send`, Python, shell helpers, Quickshell, Waybar, QProcess, XEmbed, Control Center, Notification History, or workspace integration.
- All remote D-Bus calls remain asynchronous; no synchronous GUI-thread D-Bus calls.
- Keep all resource limits enforced: depth 8, 2048 nodes, 256 children, 512 label characters, and 1 MiB menu icon data unless the existing constants are intentionally tightened.
- Keep one global protocol model and per-output popup/tooltip presentation.
- Preserve M8-A.3.x bar visuals, conditional tray spacer behavior, input-transparent tooltip behavior, and legacy guards.
- Leave unrelated existing worktree changes unstaged.

---

### Task 1: Centralize bus and wire types

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierTypes.hpp`
- Modify: `shared/statusnotifier/StatusNotifierTypes.cpp`
- Modify: `shared/CMakeLists.txt`
- Test: `shared/tests/StatusNotifierTest.cpp`

**Interfaces:**
- Produce `bool isValidDBusServiceName(const QString &)` accepting valid well-known and unique names.
- Produce `bool isValidDBusObjectPath(const QString &)`, used by `ItemAddress` and registration normalization.
- Produce registered `StatusNotifierPixmap`, `StatusNotifierToolTip`, and DBusMenu wire-update metatypes with Qt D-Bus stream operators.
- Preserve `PixmapData` and `decodeArgb32NetworkPixmap`; route all wire conversion into those common types.

- [ ] Write failing validator and typed-wire tests for `org.example.Application`, `org.kde.SomeService`, `:1.42`, malformed names, path-only `:1.42`, and exact `a(iiay)`/ToolTip round trips.
- [ ] Run `statusnotifier-test` and confirm the new cases fail against the old dot-only validator and missing wire types.
- [ ] Implement the validators and metatype stream operators with bounded structure parsing.
- [ ] Run the focused tests and confirm the parser accepts unique names while retaining malformed-name rejection.

### Task 2: Make watcher authority and host ownership live

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.hpp`
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.hpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Test: `shared/tests/StatusNotifierTest.cpp`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`
- Modify: `shared/CMakeLists.txt`

**Interfaces:**
- `StatusNotifierWatcherBridge` exposes the owned host service name and derives local `hostRegistered` from the local host registry.
- Local watcher records contain canonical registration, service/path, and unique owner; local host records contain host service and unique owner.
- Every watcher-generation async callback captures a monotonic generation and becomes a no-op after detach/stop.
- External watcher attach/disconnect is centralized and removes old D-Bus signal subscriptions before a new attach.

- [ ] Add tests for owner-cache updates, external owner disappearance, alias reselection, local takeover, duplicate-subscription suppression, host registration/property consistency, host owner removal, and start-stop-start.
- [ ] Run those tests and confirm they expose stale owner selection, fictional host ownership, and duplicate subscriptions in the baseline.
- [ ] Implement generation invalidation, owner cache updates, external detach, authoritative local registries, real unique host service registration, and host/item owner cleanup.
- [ ] Run the watcher tests on an isolated session bus and verify host service ownership with `NameHasOwner`/`GetNameOwner`.

### Task 3: Correct item wire decoding and action surface

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierItemProxy.hpp`
- Modify: `shared/statusnotifier/StatusNotifierItemProxy.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.hpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Test: `shared/tests/StatusNotifierTest.cpp`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`

**Interfaces:**
- Add `StatusNotifierItemProxy::contextMenu(int x, int y)` and `StatusNotifierService::contextMenu(const QString &, int, int)`.
- Parse `ToolTip` as `(icon-name, icon-pixmaps, title, description)` and degrade only the tooltip on malformed input.
- Parse `IconPixmap`, `AttentionIconPixmap`, `OverlayIconPixmap`, and ToolTip pixmaps through registered `StatusNotifierPixmap` lists and existing ARGB32 decoding.
- Track all D-Bus signal connection lifetimes per active item interface and disconnect them on fallback, stop, or restart.

- [ ] Add failing transport-boundary tests for ToolTip title/description, all three pixmap properties, Activate, SecondaryActivate, ContextMenu, and vertical/horizontal Scroll.
- [ ] Run the isolated fake item test and confirm the old tuple and missing-method behavior fails.
- [ ] Implement typed parsing, fallback-safe refresh, ContextMenu, and action-generation guards.
- [ ] Run the fake item test and verify exact channels/dimensions and every action argument.

### Task 4: Unify tray coordinates and click semantics

**Files:**
- Modify: `Bar/core/BarLayoutMetrics.hpp`
- Modify: `Bar/core/BarLayoutMetrics.cpp`
- Modify: `Bar/platform/wayland/BarSurfaceBundle.cpp`
- Modify: `Bar/qml/StatusSurface.qml`
- Modify: `Bar/qml/Tray.qml`
- Modify: `Bar/qml/TrayTooltipSurface.qml`
- Modify: `Bar/qml/PopupOverlaySurface.qml`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Test: `Bar/tests/BarCoreTest.cpp`
- Test: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- Add one `BarLayoutMetrics::trayAnchor(outputOriginX, outputOriginY, statusLeft, statusTop, itemLocalCenterX, itemLocalCenterY)` invokable returning `localX`, `localY`, `globalX`, and `globalY`.
- Pass screen geometry origin and status-surface geometry into every status/tray/tooltip/popup surface.
- Left click opens native menu only when `ItemIsMenu` and a usable DBusMenu client are both true; otherwise it calls Activate. Right click uses DBusMenu first, otherwise ContextMenu. Middle and wheel retain their protocol actions.

- [ ] Add failing geometry and QML interaction tests for both zero-origin and secondary-output coordinates plus the six `hasMenu`/`ItemIsMenu` click cases.
- [ ] Run focused Bar tests and confirm baseline sends status-window-local coordinates and treats any Menu path as a left-click menu.
- [ ] Implement the helper and route all three presentation/action consumers through it.
- [ ] Run BarCore and production-QRC QML smoke tests.

### Task 5: Implement live DBusMenu state and bounded menu icons

**Files:**
- Modify: `shared/statusnotifier/DBusMenuModel.hpp`
- Modify: `shared/statusnotifier/DBusMenuModel.cpp`
- Modify: `shared/statusnotifier/StatusNotifierIconStore.hpp`
- Modify: `shared/statusnotifier/StatusNotifierIconStore.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/Tray.qml`
- Test: `shared/tests/StatusNotifierTest.cpp`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`
- Test: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- `DBusMenuNode` includes `iconData` and `iconSource`; model roles expose type, visibility, enabled, toggle type/state, separator, icon source, and child model.
- `DBusMenuClient` subscribes to `LayoutUpdated` and `ItemsPropertiesUpdated`, accepts/rejects revisions, reconciles a requested subtree by stable ID, and invokes `AboutToShow` before root/submenu presentation.
- `StatusNotifierIconStore` stores menu icon images in memory and serves them through the existing `astrea-tray` provider; invalid icon data removes only the icon.
- QML uses `MenuSeparator`, hides invisible rows, renders disabled/check/radio/indeterminate states, and opens child cards adjacent to the parent with right/left fallback and output clamping.

- [ ] Add failing parser/model tests for all declared limits, hidden/separator/toggle/icon fields, stale revisions, root/subtree updates, incremental property changes, and mnemonic/label bounds.
- [ ] Add failing QML tests for titled 220px card geometry, header icon/title/separator, cascading submenu branch replacement, and outside-close behavior.
- [ ] Run tests against the baseline and confirm one-shot layout, Back navigation, hidden/separator loss, and missing icon-data presentation.
- [ ] Implement typed QDBusArgument layout/property parsing, live signal subscriptions, ID-based subtree reconciliation, AboutToShow refresh, in-memory icon sources, and reference-equivalent QML.
- [ ] Run shared and QML tests with malformed payloads and owner/menu teardown during pending calls.

### Task 6: Build the real isolated D-Bus fixture

**Files:**
- Create: `shared/tests/StatusNotifierIntegrationTest.cpp`
- Modify: `shared/CMakeLists.txt`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`

**Interfaces:**
- The test-only fake item exports the full StatusNotifierItem property/method/signal surface at `/org/test/Tray` and registers path-only over the real session bus.
- The test-only fake DBusMenu exports `GetLayout`, `AboutToShow`, `Event`, `LayoutUpdated`, and `ItemsPropertiesUpdated` at `/org/test/Menu` with normal/separator/disabled/hidden/toggle/icon/submenu nodes.
- The test runs only against `dbus-run-session` when that executable is available; no developer desktop bus is used.

- [ ] Add the fake objects and real registration/action/update/restart tests before changing production integration behavior.
- [ ] Run the fixture on the isolated bus and record the expected baseline failures.
- [ ] Connect the fixture to the production service/proxy/menu/model and cover registration, tooltip, exact pixmaps, item updates, every action, live menu updates, AboutToShow, Event, item restart, and watcher start-stop-start.
- [ ] Run the fixture repeatedly to prove no duplicate rows/signals and no stale callback mutations.

### Task 7: Qualification, report, and one scoped commit

**Files:**
- Create: `docs/M8-C.1_IMPLEMENTATION_REPORT.md`
- Modify: `docs/M8-C_IMPLEMENTATION_REPORT.md` only if a historical claim needs a correction link
- Modify: `shared/statusnotifier/README.md`
- Modify: all M8-C.1 source/test files above

- [ ] Run `git diff --check`, targeted tests in protocol-to-QML order, isolated integration tests, broader relevant CTest regressions, Debug/Release/Clang, ASAN/UBSAN, no-Typhon, and no-LayerShell builds where supported.
- [ ] Run the bad-pattern and synchronous-D-Bus searches over the production diff.
- [ ] Check final `git status` and ensure unrelated existing changes are not staged.
- [ ] Write the report from final source/test output, including real applications tested (or explicitly none), unavailable visual/LayerShell/real-app checks, sanitizer caveats, and explicit remaining limitations.
- [ ] Stage only M8-C.1 files and create one commit titled `M8-C.1: close StatusNotifier protocol interoperability`.
