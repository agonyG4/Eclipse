# M8-C.1.3 — StatusNotifier Interoperability Polish and Closure

Date: 2026-08-23

## Result

The StatusNotifier bridge now has deterministic dual-alias watcher convergence, live host ownership, bounded DBusMenu parsing, safe icon fallback behavior, lazy submenu synchronization, and output-safe recursive menu placement. The existing ShellRuntime-owned service, isolated DBus fixture, typed tooltip boundary, and native menu architecture remain intact.

## Implemented closure

### Watcher and host lifecycle

- The freedesktop and KDE watcher aliases are treated as one logical registry. Authority selection is deterministic, prefers the freedesktop alias on conflict, and re-evaluates when either alias owner changes.
- External watcher attach/detach, item registration, item unregistration, `PropertiesChanged`, host registration, and owner-loss paths are generation-safe.
- Local watcher takeover and release clear stale items and host state before re-attempting ownership.
- The host uses the protocol-shaped name `org.freedesktop.StatusNotifierHost-<unique-id>`, registers the service before advertising, watches live host owners including unique bus names, and removes its registry entry on stop.
- Conflict/recovery state is surfaced through the existing health signal and JSON diagnostics.

### Icons and cache invalidation

- Normal icons prefer `IconName` over pixmap data; attention and overlay icons use named and pixmap candidates before their documented fallbacks.
- Per-item icon revisions remain the invalidation boundary, so updating one item does not invalidate unrelated provider sources.
- Invalid or unavailable icon payloads resolve safely without crashing or poisoning the model.

### DBusMenu and lazy menus

- Initial layout, live property updates, and `AboutToShow` responses share one property-normalization path.
- Bounds cover depth, node count, child count, label size, serialized payload size, icon dimensions, and total icon pixels.
- Malformed or oversized icon data clears only the icon, preserving the menu node and its other properties.
- Removed properties reset label, icon, toggle, and submenu state instead of leaving stale values.
- Lazy submenu rows advertise `HasChildren` before their children arrive; opening a submenu requests `AboutToShow` first and resolves only after the matching generation becomes ready.

### QML geometry

- Recursive cascade placement checks the complete candidate rectangle at every depth, falls back left when the right side cannot fit, and clamps both axes to the active output bounds.
- Production-resource QML smoke coverage exercises centered and edge anchors through the same geometry helpers used by the tray menus.

## Verification

All builds used the repository's existing Unix Makefiles build directories because Ninja is unavailable in the environment. The implementation targets compiled successfully in each configuration:

- `build/debug`: focused tests and the 42-test regression matrix passed.
- `build/release`: focused StatusNotifier/Bar suite, 4/4 passed.
- `build/clang`: focused suite, 4/4 passed.
- `build/no-typhon`: focused suite, 4/4 passed.
- `build/no-layer-shell`: focused suite, 4/4 passed.
- `build/asan`: focused suite, 4/4 passed with `ASAN_OPTIONS=detect_leaks=0`.
- `build/ubsan`: focused suite, 4/4 passed.

The direct Debug QTest totals were:

- `StatusNotifierTest`: 20 passed.
- `BarCoreTest`: 25 passed.
- `BarQmlSmokeTest`: 29 passed.

The isolated DBus integration test passed in every focused matrix. `git diff --check` was clean, and the production-QRC QML smoke test passed with the new recursive placement assertions.

## Real application qualification

No real tray application was launched. The local inventory contains a Steam wrapper at `/home/agony/.local/share/SLSsteam/path/steam` and Discord at `/usr/bin/discord`; Telegram Desktop is not installed. There was no live compositor/reference-host harness available for a meaningful end-to-end visual qualification, so the report makes no claim that Steam or Discord has been manually qualified against a running shell. No additional software was installed.

## Remaining limitations

Live external-app and compositor qualification remains outside this isolated test environment. The existing M8-C scope boundaries also remain unchanged: this closure does not add XEmbed support or unrelated shell surfaces.
