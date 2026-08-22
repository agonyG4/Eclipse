# M8-C Native StatusNotifier and DBusMenu Implementation Plan

## Goal

Deliver the attached M8-C native tray contract as one coherent, reviewable commit while preserving unrelated existing worktree changes.

## Steps

1. Establish the shared protocol core and icon pipeline.
   - Add `shared/statusnotifier/StatusNotifierTypes.hpp` with normalized addresses, item snapshots, watcher state, bounded pixmap input, ARGB32 decoder results, menu limits, and typed action/health values.
   - Add `StatusNotifierIconStore` and `StatusNotifierIconProvider`; implement named XDG/Astrea theme lookup, item-local theme paths, bounded network-order ARGB32 conversion, size selection, attention/overlay composition, revisions, and `image://astrea-tray` lookup.
   - Add focused tests for exact pixels, selection order, malformed payloads, limits, attention/overlay fallback, revision invalidation, and provider URI parsing.

2. Add the watcher, item registry, and DBusMenu clients.
   - Add `StatusNotifierWatcherBridge` and watcher adaptors for both protocol names; implement external-owner discovery, deterministic alias selection, race-safe owned registration, host registration, registered-item enumeration, and owner-change signals.
   - Add registration normalization and `StatusNotifierItemProxy` with interface fallback, initial property refresh, signal refresh, generation guards, menu-path discovery, and typed activate/secondary/scroll calls.
   - Add `StatusNotifierItemModel` with stable roles and deterministic registration order, plus `StatusNotifierService` as the single lifecycle/model/action owner.
   - Add bounded `DBusMenuModel`/client support for recursive `GetLayout`, property updates, `LayoutUpdated`, `ItemsPropertiesUpdated`, `AboutToShow`, standard menu roles, nested models, and typed event routing.
   - Add tests for service-only/path-only/combined registrations, malformed/deduplicated entries, watcher mode/races/recovery, disappearance/restart, item fallback/signals/generations, menu parsing/updates/bounds, and action routing.

3. Integrate the service with the shell runtime and build graph.
   - Add the new shared target to `shared/CMakeLists.txt`, its tests, and Shell linkage.
   - Make `ShellRuntime` construct/start/stop one service and expose it through a typed getter.
   - Register the provider from `AstreaShellApplication` with the same QML engine, expose the service context property, and include service health in JSON without private menu/tooltip data.
   - Ensure service shutdown precedes any surface dereference and bar disable/reenable leaves the watcher/model alive.

4. Integrate bar surfaces and interaction routing.
   - Extend `PopupKind` and `BarPopupController` with `TrayMenu` and a stable per-output tray context key.
   - Extend `BarSurfacePolicy`, `BarSurfaceBundle`, and `BarSurfaceManager` with a mapped-on-demand 28px input-transparent tooltip surface and shared service injection. Keep popup/tooltip state output-local.
   - Add production `Tray.qml`, `TrayContextMenu.qml`, and `TrayTooltipSurface.qml`; update `StatusSurface.qml` so tray precedes Network and the conditional spacer/width logic is dynamic. Keep the prescribed pointer, middle, wheel, hover, pressed, submenu, and no-actions behavior.
   - Add/adjust QML, popup lifecycle, submenu geometry, status-width, and multi-output tests, including the invisible-shield regression.

5. Verify and document.
   - Run focused shared/statusnotifier, ShellRuntime, BarCore, BarQml, lifecycle, and legacy-guard tests, then available full builds and matrix variants.
   - Run static checks and inspect the final diff/status. Record actual session-bus fixtures and real applications tested, visual/manual checks, unavailable variants, and remaining limitations in `docs/M8-C_IMPLEMENTATION_REPORT.md` and `shared/statusnotifier/README.md`.
   - Stage only M8-C implementation/docs/tests and create the requested single commit; leave all unrelated pre-existing changes unstaged.

## Completion criteria

The implementation must compile without external tray helpers, pass the focused and available broader tests, preserve the existing topbar behavior, provide honest qualification evidence, and leave a clean scoped commit while unrelated worktree changes remain intact.
