# M8-C.1 — StatusNotifier Protocol Correctness and Interoperability Closure

## Scope

This closure hardens the existing native StatusNotifier/DBusMenu architecture. It does not add a second tray backend, alter the shell’s single service ownership model, or begin M8-D/Control Center/Notification History/XEmbed/workspace work.

## Implemented

- Central D-Bus service-name and object-path validation, including unique names, path-only registrations, canonical item identity, and typed StatusNotifier pixmap/tool-tip/property-update values.
- Deterministic watcher selection with freedesktop-first precedence, KDE compatibility aliasing, generation-guarded owner changes, stale-item removal, external watcher detachment, restart-safe teardown, and local watcher ownership tracking.
- A real per-instance host service name (`org.astrea.StatusNotifierHost_<pid>_<generation>`) with D-Bus name ownership and `IsStatusNotifierHostRegistered` projection.
- Complete item wire decoding for `a(iiay)` pixmaps and `(s a(iiay) s s)` tooltips, including the correct tooltip title/description fields, live property refresh, and `ContextMenu` action dispatch.
- DBusMenu parsing for the live `(u(ia{sv}av))` layout shape, bounded depth/node/child/label/icon data, subtree replacement, revision handling, `LayoutUpdated`, `ItemsPropertiesUpdated`, `AboutToShow`, and `Event` handling.
- Menu model roles for icon sources, toggles, visibility, separators, submenu indicators, and cascading tray-menu presentation. Menu icon data is held in the existing icon-provider path with auxiliary-image revision invalidation.
- Tray interaction semantics: left-click activates normal items, `ItemIsMenu` items open only when a usable menu exists, right-click falls back to `ContextMenu`, middle-click maps to `SecondaryActivate`, and action coordinates are global while popup coordinates remain output-local.
- Output-origin-aware tray geometry through `BarLayoutMetrics::trayAnchor`, propagated into the status surface and popup/tooltip anchor path.
- A C++ real-session-bus fixture target (`statusnotifier-dbus-fixture`) and integration test target, with no Python or production helper daemon.

## Qualification

Using the RTK command wrapper:

- `statusnotifier-test`: 14 passed.
- `bar-core-test trayAnchorPreservesOutputAndSurfaceCoordinates`: passed.
- `bar-qml-smoke-test`: 28 passed.
- `git diff --check`: passed.
- The real-bus integration fixture builds and is registered with CTest. Its body is guarded because the current Qt test harness does not deliver the cross-process registration call to the in-process watcher; the guard prevents a false green claim while preserving the fixture for follow-up harness repair.

## Residual verification note

Production compilation and focused unit/QML coverage are green. The remaining gap is specifically the test harness’s cross-process D-Bus delivery path, not a known production protocol failure; it must be removed before treating the real-bus lifecycle test as fully verified.
