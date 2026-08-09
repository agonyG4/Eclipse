# M7-D Unified Eclipse Shell Runtime Design

## Goal

Consolidate the persistent Dock, AltTab, and Spotlight shell applications into one production `astrea-shell` process while preserving the M7-A, M7-B, and M7-C behavior contracts. M7-D changes process and ownership topology; it does not redesign window-management policy, shell visuals, or Typhon protocol semantics.

## Baseline and scope

- Eclipse starts at `6fc6f7fec12f78f7396ae57386753d2c4af2153f` on `main`.
- Typhon remains pinned at `211dfe835d1d6d6faf449e7a0239d6f099945e6e`.
- Only `/home/agony/GitHub/Eclipse` is modified.
- Native M7 qualification remains deferred.
- The future post-M7 `astrea-shell-runtime` / `astrea-overlay` process split is out of scope.

## Architecture

`astrea-shell` owns one `QGuiApplication`, one `QQmlApplicationEngine`, one explicit `AstreaShellApplication`, and one `ShellRuntime`. `ShellRuntime` owns shared services and injects them into three independent controllers:

```text
AstreaShellApplication
  ├── ShellRuntime
  │     ├── shared Typhon session
  │     ├── DesktopEntryCatalog
  │     ├── AppIdentityResolver
  │     ├── launch service/state
  │     ├── icon/cache and watcher ownership
  │     ├── shortcut dispatcher
  │     └── ShellIpcServer
  ├── DockController
  ├── AltTabController
  └── SpotlightController
```

The controllers remain separate. Dock owns resident visibility and launch/activation policy; AltTab owns transient selection and commit/cancel behavior; Spotlight owns transient search, selection, and launcher behavior. Shared state is exposed through typed C++ interfaces and signals. No internal local socket or general-purpose message bus is introduced.

## Shared Typhon session

The new shared Typhon session owns the `TyphonWaylandDisplay`, capability authentication, registry lifecycle, one reconnect timer, and one connection generation. The session authenticates exactly once per generation. Its protocol layer binds the toplevel and shortcut managers on that same native display, and dispatches all protocol events through the single display event loop.

The existing toplevel model/action state remains the authoritative source for exact `WindowId` snapshots and manager-owned action completion. The shared session adapts its state to the existing `TyphonToplevelConnection` and `TyphonShortcutClient` consumer contracts, preserving standalone constructors for current focused tests. Unified production wiring injects the shared session; neither controller creates a display, authenticates, reconnects, or owns a second pending-action state.

Reconnect behavior is generation-safe:

```text
disconnect
  -> shared degraded/disconnected state
  -> one cleanup of managers, handles, shortcuts, snapshot, and pending actions
  -> one scheduled reconnect
reconnect
  -> fresh display and ClientId
  -> fresh capability authentication
  -> manager rebinding and snapshot rebuild
  -> shortcut re-registration
  -> controllers resume from shared state
```

A feature-local failure is reported to that controller and does not tear down the shell. Shared transport/authentication failures are explicit and visible in shell status. No M7-C action result, target identity, token, or close-completion semantic changes.

## Shared catalog and identity

`DesktopEntryCatalog` becomes the sole production owner of desktop-file discovery, parsing, revisioning, directory recovery, debounce, and watchers. Its snapshot is extended only as needed to carry the fields already used by Spotlight search (localized display fields, keywords, categories, launch metadata, visibility, and desktop filename identity). Dock and Typhon application matching continue to consume the same authoritative snapshot.

Spotlight's Rust search backend receives a serialized snapshot from the shared catalog and builds its searchable/ranking projection from that data. It no longer scans XDG application directories or owns a duplicate watcher in the unified process. Spotlight reloads on the catalog's `indexUpdated` signal and retains its existing search, ranking, usage, icon, and launch behavior.

One `AppIdentityResolver` is created by `ShellRuntime` and injected into AltTab and other identity consumers. Its production desktop-entry lookup uses the shared catalog snapshot; its existing process, Steam, Wine, alias, and cache behavior remains feature-compatible. Dock's exact Typhon window selection still ends at `WindowId`; its app grouping uses the shared catalog normalization/matching path and never mutates by app ID, PID, or title.

## QML and root lifecycle

The shell target embeds the existing Dock, AltTab, and Spotlight QML files without merging their visual components. The one engine receives one context property per controller/model and one shared icon provider/cache. Root lifecycle is explicit:

- Dock root is created at startup and remains resident.
- AltTab root is created after startup, hidden when inactive, and resets selection, hover/focus state, pending shortcut state, and stale window identity on dismissal.
- Spotlight root is created after startup, hidden when inactive, and resets query, selection, focus, and transient results on dismissal.

Root-load failures are diagnosed by root. Dock failure is fatal because it invalidates the resident shell surface; AltTab and Spotlight failures are recoverable when the remaining QML roots and shared runtime are valid. No QML visual behavior is redesigned.

## Unified IPC and compatibility

`ShellIpcServer` owns one bounded `astrea-shell-v1` endpoint. Commands have a fixed domain/action vocabulary for Dock, AltTab, Spotlight, and status; payloads are bounded and are dispatched directly to the matching controller. The endpoint is not a general-purpose bus.

The legacy executable names remain thin `QCoreApplication` clients where compatibility is useful:

- `astrea-dock` maps status/reload/show/hide/quit to the shell endpoint.
- `astrea-alt-tab` maps next/previous/commit/cancel/show/hide/reload/status.
- `astrea-spotlight` maps show/hide/toggle/query/activate/reload/status and retains icon diagnostics as a nonresident local command.

Legacy `--daemon` requests return a clear nonzero error and never create a persistent Qt, QML, Wayland, catalog, or IPC stack. Existing command names therefore cannot create duplicate shell ownership. Existing feature IPC server classes remain testable only where their focused tests require them; they are not production-owned by `astrea-shell`.

## Systemd

Install `astrea-shell.service` with the existing graphical-session ordering, restart policy, Qt Wayland environment, and session capability environment. Stop installing persistent `astrea-dock.service`, `astrea-alt-tabd.service`, and `astrea-spotlightd.service`; retained legacy service files are marked compatibility-only or removed according to the repository's install layout. Static tests prove that the installed topology cannot enable four persistent shell daemons.

## Error and ownership rules

- One production owner exists for each `QGuiApplication`, `QQmlApplicationEngine`, Typhon display/session, reconnect timer, catalog, identity resolver, launch state, shortcut dispatcher, and shell IPC endpoint.
- Feature controllers do not create shared services or global singletons.
- Exact `WindowId` is retained from selection through the final Typhon action request.
- Dock stale/unavailable targets reconcile without retargeting and without same-click launch fallback.
- AltTab and Dock action completion remains manager-owned and feature-local UI state does not consume or mutate another controller's state.
- Disconnect and shutdown clear signals, managers, watchers, timers, and transient roots exactly once.

## Testing design

Tests are layered and deterministic:

1. Shared ownership tests assert one runtime instance of each shared service, one Typhon session/display/authentication generation, and one shortcut registry owner.
2. Feature isolation tests assert Dock, AltTab, and Spotlight state changes do not cross-contaminate controller-local state.
3. Lifecycle tests drive at least 100 show/dismiss cycles for AltTab and Spotlight without sleeps and assert stable model, connection, watcher, timer, shortcut, query, and selection counts.
4. Reconnect tests assert one reconnect loop, fresh authentication, manager/toplevel rebuild, exactly-once shortcut delivery, and recovery of Dock, AltTab, and Spotlight.
5. M7-C preservation tests rerun exact Dock activation, recency, minimized, unavailable, stale-target, and no-duplicate-launch behavior; exact AltTab selection, minimized activation, stale selection, mutation races, and real-QML selected-count behavior; and Spotlight search, selection, launcher identity, watcher recovery, query reset, and show/dismiss behavior.
6. Compatibility tests invoke each retained legacy command and assert one shell action with no second daemon; unavailable-shell behavior is typed/nonzero.
7. Systemd/source-layout tests inspect installed service files and production ownership counts.
8. Resource evidence records process count, PSS/private memory, RSS for reference, threads, FDs, Typhon Wayland connections, and idle CPU before and after under equivalent conditions. PSS/private memory is the primary memory evidence; summed RSS is not used as savings evidence.

## Qualification boundary

M7-D can be marked implementation and deterministic qualification `PASS` only after the established Debug, Release, Clang, ASan, UBSan, and no-Typhon builds and full serial CTest suites pass, changed QML is validated, static/source-layout/whitespace checks pass, and the resource/process evidence is recorded. Native qualification remains `DEFERRED` for M7-A through M7-D.

## Alternatives considered

### Keep three daemons behind a supervisor

Rejected because it preserves multiple `QGuiApplication`, QML, Wayland, catalog, and authentication owners and cannot satisfy the one-process acceptance criterion.

### One generic shell controller

Rejected because it entangles Dock, AltTab, and Spotlight state and makes feature-local failure and lifecycle reset harder to test. The runtime owns shared services; controllers remain independent.

### One process with three independent transports and engines

Rejected because it would reduce process count while retaining duplicated Typhon authentication, reconnect loops, catalogs, watchers, caches, and QML engines. It does not satisfy M7-D's ownership goal.
