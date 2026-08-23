# M8-C.1.4 Native Tray Runtime, Lifetime, Bounds and Reference-Parity Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the final M8-C runtime invariants and literal tray reference-parity gaps without redesigning StatusNotifier or starting M8-D.

**Architecture:** Keep one `StatusNotifierService` owned by `ShellRuntime`, with the existing watcher bridge, item proxies, item model, DBusMenu clients/models, icon store, and per-output Bar surfaces. Make topology observation, menu-client identity, and presentation state explicit at their existing boundaries. Validate live DBusMenu mutations against the existing model graph before applying them.

**Tech Stack:** C++17/Qt 6, Qt D-Bus, Qt Test, Qt Quick/QML, existing CMake/CTest build directories, isolated session-bus fixture.

## Global Constraints

- Work directly in the current Eclipse source tree and use M8-C.1.3 as the baseline.
- This is the final M8-C closure; do not start M8-D.
- Do not add Control Center, Notification History, XEmbed, Typhon workspace work, or application-specific tray special cases.
- Preserve existing StatusNotifier protocol behavior: KDE/Freedesktop interoperability, path-only registration, unique owners, real host ownership, typed tooltips, ARGB32 decoding, icon-name priority, actions, LayoutUpdated, ItemsPropertiesUpdated, AboutToShow, subtree reconciliation, states, icon-data, recursive menus, and the isolated D-Bus fixture.
- Use the bundled Quickshell Tray reference for visual and interaction semantics, while Eclipse remains the runtime authority.
- Use RTK for shell commands, `apply_patch` for edits, and stage only files in this plan plus the final report.

---

## File Map

- Modify `shared/statusnotifier/StatusNotifierWatcherBridge.cpp`: preserve both compatibility aliases in the topology watcher for the entire started lifetime; retain live owner convergence during authority changes.
- Modify `shared/statusnotifier/StatusNotifierItemProxy.cpp`: emit `menuPathChanged` only for real path transitions and prevent stopped proxies from publishing queued status snapshots.
- Modify `shared/statusnotifier/StatusNotifierService.hpp/.cpp`: make menu updates same-path/idempotent, expose narrow title/icon presentation accessors, remove or neutralize the misleading visual `closeMenu` transport API, and close menu state exactly once on item removal.
- Modify `shared/statusnotifier/DBusMenuModel.hpp/.cpp`: add live-model tree statistics and bounded replacement validation with distinct target-not-found versus limit-rejected outcomes; keep terminal client stop semantics.
- Modify `shared/tests/StatusNotifierTest.cpp`: add proxy terminal-signal and cumulative node/depth mutation tests.
- Modify `shared/tests/StatusNotifierIntegrationTest.cpp` and `shared/tests/StatusNotifierDBusFixture.cpp`: add real session-bus watcher-alias convergence, menu-client stability, real menu-path replacement, and item-owner disappearance coverage.
- Modify `Bar/qml/Tray.qml`: exact 28×28 delegate and pointer target, reference hover/pressed tokens, pointing cursor, immediate tooltip presentation, and compact label semantics.
- Modify `Bar/qml/TrayTooltipSurface.qml`: reference dimensions, +18 sizing, semantic aliases, compact label, and dead-item cleanup.
- Modify `Bar/qml/TrayContextMenu.qml` and `Bar/qml/TrayMenuCard.qml`: presentation-only item refresh, real item header icon/title, conditional separator, shared `MenuSeparator`, reference submenu glyph, and no-actions styling.
- Modify `Bar/qml/PopupOverlaySurface.qml`: close a visible tray popup when its item disappears.
- Modify `Bar/core/BarSurfacePolicy.hpp/.cpp` and `Bar/tests/BarCoreTest.cpp`: apply and verify the reference-derived tooltip top offset of 43 px.
- Modify `Bar/tests/BarQmlSmokeTest.cpp`: cover tray hitboxes/cursor/colors, tooltip semantics/style/placement, context-menu header/separators/glyph/no-actions presentation, and removal-driven popup/tooltip cleanup.
- Create `docs/M8-C.1.4_IMPLEMENTATION_REPORT.md`: final English qualification report distinguishing feature scenarios from framework totals.

## Task 1: Add failing runtime and model tests

**Files:**
- Modify: `shared/tests/StatusNotifierTest.cpp`
- Modify: `shared/tests/StatusNotifierIntegrationTest.cpp`
- Modify: `shared/tests/StatusNotifierDBusFixture.cpp`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `Bar/tests/BarCoreTest.cpp`

**Interfaces:**
- Tests will use production `StatusNotifierItemProxy`, `StatusNotifierService`, `DBusMenuModel`, `DBusMenuClient`, Bar QML components, and the existing fixture; no transport mocks will replace the real code paths.

- [ ] Add a `StatusNotifierItemProxy` meta-object test that invokes the queued `NewStatus` slot after `stop()` and asserts zero `snapshotChanged` emissions.
- [ ] Add model tests for a same-path presentation refresh, service-level same-path client identity, cumulative node-count rejection, cumulative depth rejection, accepted replacement, and preservation of the old tree after rejection.
- [ ] Extend the fixture with mutable title/icon/menu-path signals and deterministic watcher-alias claim/release controls; add integration assertions for unchanged-menu client/root identity and exactly one client replacement on a real menu-path change.
- [ ] Add the real session-bus alias sequence: external KDE owner first, external Freedesktop owner second, then KDE owner disappearance/change, asserting the bridge still converges from the KDE alias event after authority switched.
- [ ] Add production-QRC assertions for 28×28 tray geometry, cursor and color tokens, immediate tooltip behavior/style/label, 43 px tooltip surface placement, real header icon/title typography, conditional header separator, shared separators, `󰅂`, no-actions row, and item-removal popup/tooltip closure.
- [ ] Build the focused targets and run the new tests before implementation; expected RED failures must identify the missing behavior rather than test syntax errors.

## Task 2: Preserve watcher topology observation

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierWatcherBridge.cpp`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`

- [ ] Remove selected-authority watcher-name removal from `disconnectExternalWatcher()`; only selected signal subscriptions and generation state may be detached during authority changes.
- [ ] Keep both compatibility aliases registered with the long-lived `QDBusServiceWatcher` until `stop()` destroys it.
- [ ] Keep `m_ownerByName` updated for either alias and make every authority re-evaluation use those live values.
- [ ] Run the isolated alias regression and focused StatusNotifier tests; confirm no duplicate selected-watcher signal subscriptions occur during repeated authority changes.

## Task 3: Separate menu identity from presentation and make client lifetime stable

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierItemProxy.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.hpp/.cpp`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Test: `shared/tests/StatusNotifierIntegrationTest.cpp`, `Bar/tests/BarQmlSmokeTest.cpp`

- [ ] Capture the old `Menu` path before applying each coherent item snapshot and emit `menuPathChanged` only when the path actually changes, including empty-to-path and path-to-empty transitions.
- [ ] Add `DBusMenuClient::itemGeneration()` and use menu path plus live item generation as the service idempotence key; same-path updates must not stop/delete/recreate the client.
- [ ] Remove the unused visual `StatusNotifierService::closeMenu()` transport call, keeping `DBusMenuClient::stop()` solely for terminal transport teardown.
- [ ] Add narrow `displayTitleForItem()` and `iconSourceForItem()` service accessors; do not expose proxies, raw snapshots, or QDBus interfaces to QML.
- [ ] Change `TrayContextMenu` item-change handling to refresh a presentation revision used by header bindings, without resetting the root model, cascades, or pending AboutToShow state.
- [ ] Keep `menuClientChanged` as the only identity-change reset trigger and `menuContentChanged` as the live content/pending-submenu trigger.
- [ ] Verify title/icon changes preserve the existing menu client, root model, and open cascade; verify a real menu-path change creates one fresh client and closes the old presentation generation.

## Task 4: Enforce terminal lifetimes and item-removal UI closure

**Files:**
- Modify: `shared/statusnotifier/DBusMenuModel.cpp`
- Modify: `shared/statusnotifier/StatusNotifierItemProxy.cpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`
- Modify: `Bar/qml/PopupOverlaySurface.qml`
- Modify: `Bar/qml/TrayTooltipSurface.qml`
- Test: `shared/tests/StatusNotifierTest.cpp`, `Bar/tests/BarQmlSmokeTest.cpp`

- [ ] Remove the dead `m_stopped = false` resurrection assignment from `DBusMenuClient::prepareForPresentation()`; stopped clients remain stopped forever.
- [ ] Guard `onNewStatus()` and `emitSnapshot()` with `m_started` so queued signals cannot republish state after proxy stop.
- [ ] Ensure item removal stops/deletes its proxy and menu client once, clears icon state/model state, and emits the existing removal lifecycle without stale menu authority.
- [ ] Connect the popup overlay to `StatusNotifierService::itemRemoved`; close a visible tray popup for the removed context through the normal `BarPopupController` close animation.
- [ ] Connect the tooltip surface to item removal; hide immediately and clear its item key so no dead tooltip remains.
- [ ] Verify the click shield and popup surface are gone after the close lifecycle completes.

## Task 5: Validate cumulative live DBusMenu bounds before mutation

**Files:**
- Modify: `shared/statusnotifier/DBusMenuModel.hpp/.cpp`
- Modify: `shared/statusnotifier/DBusMenuClient` implementation in `shared/statusnotifier/DBusMenuModel.cpp`
- Test: `shared/tests/StatusNotifierTest.cpp`, `shared/tests/StatusNotifierIntegrationTest.cpp`

- [ ] Add `DBusMenuTreeStats { nodeCount, maxDepth }` helpers that walk the live `DBusMenuModel` graph and prefer materialized child models over stale duplicated `node.children` snapshots.
- [ ] Before a non-root replacement, locate the live target depth and old branch stats, compute candidate branch stats, and enforce `currentNodes - oldNodes + candidateNodes <= maxNodes` plus `targetDepth + candidateRelativeDepth <= maxDepth`.
- [ ] Keep initial root parsing and live property normalization limits unchanged, including children, labels, serialized payloads, icon bytes, dimensions, and pixels.
- [ ] Distinguish target-not-found from limit rejection: target-not-found may request one bounded root refresh; limit rejection preserves the current valid tree, emits a health/failure signal, and does not retry the same oversized subtree.
- [ ] Ensure accepted replacements update the existing child model graph and preserve recursive AboutToShow/content behavior.
- [ ] Run node-count/depth rejection and accepted-replacement tests under normal, ASan, and UBSan builds.

## Task 6: Close literal Tray and reference-parity gaps

**Files:**
- Modify: `Bar/qml/Tray.qml`
- Modify: `Bar/qml/TrayTooltipSurface.qml`
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayMenuCard.qml`
- Modify: `Bar/qml/PopupOverlaySurface.qml`
- Modify: `Bar/core/BarSurfacePolicy.hpp/.cpp`
- Test: `Bar/tests/BarQmlSmokeTest.cpp`, `Bar/tests/BarCoreTest.cpp`

- [ ] Set the ready tray delegate and its `MouseArea` to exactly 28×28 while leaving the containing status row at 36 px; use `Qt.PointingHandCursor` only for actionable ready items.
- [ ] Match reference radius, transparent idle, separator hover, pressed `Qt.rgba(1, 1, 1, 0.2)`, and `Theme.animationFast` behavior.
- [ ] Remove the 420 ms timer and show a non-empty compact tooltip immediately on pointer entry; hide immediately on exit.
- [ ] Render only `tooltipTitle || title || ""`; keep description in the native model but do not append it visually. Use max width 260, implicit text width +18, height 28, `background`, `border`, `shellTextActive`, `fontFamily`, and caption semantics.
- [ ] Set the tooltip layer-shell top margin to the reference-derived 43 px, preserving input transparency, no focus, and exclusive zone -1.
- [ ] Render the real item icon at 18×18 in the context-menu header and use `tooltipTitle || title || id || "Tray item"` with body-size, DemiBold, active-color typography.
- [ ] Show the header separator only for a real remote menu, use `MenuSeparator` for root and cascade rows, use glyph `󰅂`, and set the no-actions row to 32 px/secondary/small/reference family.
- [ ] Preserve disabled/hidden rows, icon-name/data, check/radio/indeterminate state, lazy AboutToShow, and recursive bounds-safe cascades.
- [ ] Run production-QRC parity tests in dark and light theme state where the existing test harness permits; report live 1920×1080 visual comparison as Not Run if no compositor is available.

## Task 7: Qualify, document, and commit

**Files:**
- Create: `docs/M8-C.1.4_IMPLEMENTATION_REPORT.md`
- Modify: only the files listed above and the plan file.

- [ ] Run focused tests in the required order: StatusNotifier unit, isolated D-Bus integration, BarCore, BarQmlSmoke, and Bar lifecycle/surface tests.
- [ ] Run the existing Debug broader matrix covering ShellRuntime, Theme, Dock, Spotlight, AltTab, Paper, shared/system, and legacy guards.
- [ ] Rebuild/run Debug, Release, Clang, no-Typhon, no-layer-shell, ASan, and UBSan using existing build directories; record exact commands and outcomes.
- [ ] Run `git diff --check`, inspect the final scoped diff, and verify no legacy process-helper strings or unrelated refactoring entered the production tray path.
- [ ] Write the report with baseline, each closure area, focused feature-scenario counts separate from QTest lifecycle totals, isolated D-Bus results, build/sanitizer matrix, manual visual qualification, real applications actually tested, Not Run reasons, and explicit remaining limitations.
- [ ] Stage only the scoped implementation, tests, report, and plan; create one commit titled `M8-C.1.4: final native tray runtime closure`.
