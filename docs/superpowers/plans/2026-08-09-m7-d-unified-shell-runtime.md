# M7-D Unified Shell Runtime Implementation Plan

> **For agentic workers:** Execute this plan inline in the existing Eclipse checkout. Do not use subagents, branches, worktrees, amend, squash, reset, clean, or history rewrite.

**Goal:** Replace the three persistent Eclipse shell daemon stacks with one production `astrea-shell` runtime that owns one Qt application, one QML engine, one authenticated Typhon session, one catalog/identity/launch ownership graph, and one shell IPC endpoint while preserving M7-C behavior.

**Architecture:** Add an explicit `AstreaShellApplication`/`ShellRuntime` composition layer. Refactor the Typhon display/authentication and reconnect lifecycle into one injectable shared session, keep Dock/AltTab/Spotlight controllers separate, feed Spotlight from the shared desktop catalog, load the existing QML roots through one engine, and retain legacy executable names as nonresident IPC clients.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/Test, Wayland client protocol fixtures, Rust Spotlight backend FFI, CMake, systemd user units, CTest, Make, qmllint, ASan, UBSan.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse` on the existing `main` branch.
- Starting Eclipse HEAD is `6fc6f7fec12f78f7396ae57386753d2c4af2153f`.
- Do not modify Typhon or the M7-B protocol XML.
- Preserve exact `WindowId` targeting, manager-owned `action_done`, v1 read-only behavior, generation cleanup, completed-token reuse, and M7-C Dock/AltTab semantics.
- Production shell ownership counts are one `QGuiApplication`, one `QQmlApplicationEngine`, one Typhon display/session/authentication owner, one reconnect loop, one `DesktopEntryCatalog`, one `AppIdentityResolver`, one launch-state owner, one shortcut dispatcher, and one IPC endpoint.
- Do not create a giant controller, global static singleton state, a local controller-to-controller broker, a new action transport, PID/title/app-ID mutation, or post-M7/M8 process topology.
- Legacy `astrea-dock`, `astrea-alt-tab`, and `astrea-spotlight` commands must be nonresident compatibility clients; legacy `--daemon` must not start old daemon stacks.
- Use established build directories only: `build/debug`, `build/release`, `build/clang`, `build/asan`, `build/ubsan`, and `build/no-typhon`.
- Do not use sleeps or artificial delayed production actions. Drive event completion deterministically.
- All documentation is written in English.

---

### Task 1: Add the shared Typhon session lifecycle

**Files:**
- Create: `shared/platform/typhon/TyphonSharedConnection.hpp`
- Create: `shared/platform/typhon/TyphonSharedConnection.cpp`
- Modify: `shared/CMakeLists.txt`
- Create: `shared/tests/TyphonSharedConnectionTest.cpp`
- Modify: `shared/CMakeLists.txt` test registration

**Interfaces:**
- `TyphonSharedConnection::start()` and `stop()` own the single `TyphonWaylandDisplay` and one reconnect timer.
- `TyphonSharedConnection::state()`, `isReady()`, `connectionGeneration()`, `authenticationGeneration()`, and `nativeDisplay()` expose read-only state.
- Signals are `stateChanged`, `ready(quint64 generation)`, `disconnected(quint64 generation)`, and `diagnostic(QString)`.
- A deterministic injected transport/authentication seam is used by the unit test so counts are observable without requiring a live compositor; production uses `TyphonWaylandDisplay` and `TyphonShellAuthenticator`.

- [ ] **Step 1: Write the failing lifecycle test.**

Add tests that start a fake shared session, assert one display connection and one authentication for generation 1, request a reconnect after a simulated disconnect, and assert one fresh authentication for generation 2. Assert that repeated `start()` and repeated disconnect notifications do not create duplicate timers or generations.

- [ ] **Step 2: Run the focused test and verify it fails because the shared session does not exist.**

Run:

```bash
cmake --build build/debug --target typhon-shared-connection-test -j1
build/debug/typhon-shared-connection-test
```

Expected: the target is unavailable or the test does not compile because the new lifecycle type is not implemented.

- [ ] **Step 3: Implement the minimum shared session.**

Move connection-generation/backoff ownership into `TyphonSharedConnection`. Authenticate after each successful `wl_display_connect`, emit `ready` only after authentication and registry transport readiness, stop all reconnect work on `stop()`, and make every disconnect path converge on one cleanup signal. Do not bind feature managers here; the session only owns transport/authentication/lifecycle.

- [ ] **Step 4: Run the focused test and the existing Typhon display tests.**

Run:

```bash
cmake --build build/debug --target typhon-shared-connection-test typhon-protocol-client-test -j1
build/debug/typhon-shared-connection-test
build/debug/typhon-protocol-client-test
```

Expected: all new lifecycle assertions and existing protocol-client tests pass.

- [ ] **Step 5: Commit the shared lifecycle primitive.**

```bash
git add shared/platform/typhon/TyphonSharedConnection.hpp \
        shared/platform/typhon/TyphonSharedConnection.cpp \
        shared/tests/TyphonSharedConnectionTest.cpp \
        shared/CMakeLists.txt
git diff --cached --check
git commit -m "refactor(typhon): add shared shell connection lifecycle"
```

### Task 2: Adapt toplevel and shortcut consumers to the shared session

**Files:**
- Modify: `shared/platform/typhon/TyphonProtocolAdapter.hpp`
- Modify: `shared/platform/typhon/TyphonProtocolAdapter.cpp`
- Modify: `shared/platform/typhon/TyphonToplevelConnection.hpp`
- Modify: `shared/platform/typhon/TyphonToplevelConnection.cpp`
- Modify: `shared/platform/typhon/TyphonShortcutClient.hpp`
- Modify: `shared/platform/typhon/TyphonShortcutClient.cpp`
- Modify: `shared/tests/TyphonProtocolIntegrationTest.cpp`
- Modify: `shared/tests/TyphonShortcutProtocolIntegrationTest.cpp`
- Modify: `shared/CMakeLists.txt` test registration if targets need the new source

**Interfaces:**
- `TyphonProtocolAdapter` accepts an optional `TyphonSharedConnection *`; omitted means the existing standalone transport path used by focused tests.
- `TyphonShortcutClient` accepts an optional `TyphonSharedConnection *`; shared mode never calls `wl_display_connect`, authenticates, disconnects, or schedules a reconnect.
- `TyphonToplevelConnection` accepts the shared adapter/session and delegates reconnect cleanup to the session in shared mode.
- Existing standalone constructors and public action/result APIs remain source-compatible.

- [ ] **Step 1: Add failing shared-transport integration assertions.**

Extend the fake Wayland compositor fixture to count authenticated native clients and bound feature consumers. Construct a shared session, a shared-mode toplevel connection, and a shared-mode shortcut client; assert one Wayland client/authentication, one connection generation, one shortcut registration set, and exact action delivery through the same native client. Add a reconnect assertion that both consumers recover from the same generation.

- [ ] **Step 2: Run the focused integration tests and verify the shared path fails before adaptation.**

Run:

```bash
build/debug/typhon-protocol-integration-test
build/debug/typhon-shortcut-protocol-integration-test
```

Expected: the shared-mode constructor/API or one-client assertions fail before production adaptation; existing standalone coverage must remain green.

- [ ] **Step 3: Refactor the generated toplevel adapter for injected display ownership.**

When a shared session is present, bind the toplevel manager and handles on `session->nativeDisplay()` after `ready`, skip local authentication, and destroy only protocol objects on consumer stop. On shared disconnect, invalidate manager/handle state and emit the existing adapter signals without disconnecting the shared display. Keep the current standalone path unchanged.

- [ ] **Step 4: Refactor shortcut registration for injected display ownership.**

In shared mode, bind the shortcut manager on the same native display and register the existing `astrea-shell/alt_tab_next`, `alt_tab_previous`, and `alt_tab_commit` names once per shared generation. Remove the shortcut client's independent reconnect timer in shared mode. Route all phases through the existing `AltTabShortcutRouter` path.

- [ ] **Step 5: Make the two consumer facades generation-safe.**

Add shared-mode guards to `TyphonToplevelConnection` and `TyphonShortcutClient` so only `TyphonSharedConnection` schedules reconnect, clears protocol objects once, and increments generation. Preserve action-state clearing, exact `WindowId` resolution, stale completion rejection, and manager-owned completion.

- [ ] **Step 6: Run focused red/green verification.**

Run:

```bash
cmake --build build/debug --target typhon-protocol-integration-test typhon-shortcut-protocol-integration-test -j1
build/debug/typhon-protocol-integration-test
build/debug/typhon-shortcut-protocol-integration-test
ctest --test-dir build/debug --output-on-failure -j1 -R 'typhon-(action-state|protocol|shortcut|toplevel|shared)'
```

Expected: shared and standalone tests pass, one authenticated client is observed, reconnect re-authenticates once, and no existing M7-C test changes its result contract.

- [ ] **Step 7: Commit the shared Typhon consumer adaptation.**

```bash
git add shared/platform/typhon shared/tests/TyphonProtocolIntegrationTest.cpp \
        shared/tests/TyphonShortcutProtocolIntegrationTest.cpp shared/CMakeLists.txt
git diff --cached --check
git commit -m "refactor(typhon): share authenticated shell transport"
```

### Task 3: Make the desktop catalog and identity resolver shared-runtime compatible

**Files:**
- Modify: `shared/apps/DesktopEntryCatalog.hpp`
- Modify: `shared/apps/DesktopEntryCatalog.cpp`
- Modify: `shared/tests/DesktopEntryCatalogTest.cpp`
- Modify: `AltTab/services/AppIdentityResolver.hpp`
- Modify: `AltTab/services/AppIdentityResolver.cpp`
- Modify: `AltTab/tests/AppIdentityResolverTest.cpp`
- Modify: `AltTab/services/appidentity/DesktopEntryIndex.hpp` only if the compatibility alias needs an explicit shared-catalog path

**Interfaces:**
- `DesktopEntryRecord` gains the existing Spotlight fields without removing current Dock fields: generic name, comment, localized values, keywords, categories, terminal, OnlyShowIn, and NotShowIn.
- `DesktopEntryCatalog::snapshot()` remains the authoritative immutable snapshot API; add `snapshotJson()` only if needed by the Rust bridge.
- `AppIdentityResolver::initialize(DesktopEntryCatalog *catalog, ...)` selects the shared production snapshot; the existing fallback initialization remains for focused resolver tests.
- `desktopIndexRevision()` reports the shared catalog revision when injected.

- [ ] **Step 1: Add catalog tests for the fields and shared revision.**

Create a temporary XDG application tree containing localized names, keywords, categories, `TryExec`, visibility, and startup class. Assert one catalog revision contains all fields, directory recreation recovers through the existing nearest-directory watcher behavior, and two consumers reading the same catalog observe the same revision and snapshot pointer.

- [ ] **Step 2: Run the catalog tests and verify the new fields fail before parser changes.**

Run:

```bash
build/debug/desktop-entry-catalog-test
```

Expected: the new field assertions fail because the current C++ parser only records the Dock subset.

- [ ] **Step 3: Extend the parser without changing existing matching priority.**

Parse the additional desktop-entry fields, preserve XDG directory precedence and filename identity, retain invalid-entry handling, and keep `byDesktopId`, `byDesktopFileName`, and `byStartupWmClass` indexes stable. Do not introduce another watcher.

- [ ] **Step 4: Add resolver tests for injected catalog lookup.**

Initialize one catalog and one resolver against it, resolve a window by startup class and desktop identity, then rebuild the catalog and assert the resolver observes the new revision without constructing a second watcher/index. Keep all existing alias/deep-resolution tests green.

- [ ] **Step 5: Implement the injected resolver path.**

Use the shared snapshot for desktop-entry resolution and revision checks when supplied. Preserve process inspection, Steam metadata, Wine resolution, aliases, cache keys, and async generation validation. Keep test-only fallback behavior explicit rather than silently creating a production duplicate catalog.

- [ ] **Step 6: Run focused green verification and commit.**

Run:

```bash
cmake --build build/debug --target desktop-entry-catalog-test alttab-identity-test -j1
build/debug/desktop-entry-catalog-test
build/debug/alttab-identity-test
```

Commit:

```bash
git add shared/apps shared/tests/DesktopEntryCatalogTest.cpp \
        AltTab/services/AppIdentityResolver.* AltTab/tests/AppIdentityResolverTest.cpp
git diff --cached --check
git commit -m "refactor(shell): share desktop catalog identity ownership"
```

### Task 4: Feed Spotlight from the shared catalog and share launch state

**Files:**
- Modify: `Spotlight/backend/include/astrea_spotlight_backend.h`
- Modify: `Spotlight/backend/src/lib.rs`
- Modify: `Spotlight/backend/src/ffi/exports.rs`
- Modify: `Spotlight/backend/src/desktop/entries.rs` or the Rust backend projection module
- Modify: `Spotlight/platform/rust/RustSpotlightBackend.hpp`
- Modify: `Spotlight/platform/rust/RustSpotlightBackend.cpp`
- Modify: `Spotlight/core/SpotlightController.hpp`
- Modify: `Spotlight/core/SpotlightController.cpp`
- Modify: `Spotlight/tests/SpotlightBackendTest.cpp`
- Modify: `shared/launch/ApplicationLauncher.hpp` only if launch-origin/state instrumentation is needed

**Interfaces:**
- Add `RustSpotlightBackend::setCatalog(const QJsonArray &, QString *)` and the matching C ABI `astrea_spotlight_backend_set_catalog_json`.
- `SpotlightController` accepts an injected shared `DesktopEntryCatalog *` and `ApplicationLauncher *`; its existing constructor remains usable by focused tests.
- Catalog updates call one controller reload path and do not create a `QFileSystemWatcher` or duplicate XDG scan in the unified production path.

- [ ] **Step 1: Add a failing Spotlight shared-catalog test.**

Build a temporary catalog with a known desktop entry, inject it into Spotlight, search for it, mutate the catalog, emit `indexUpdated`, and assert the new entry is searchable with the same Rust backend instance. Assert the controller owns no application-directory watcher in injected mode and launch completion is visible through the shared launcher.

- [ ] **Step 2: Run the focused Spotlight test and verify it fails before the bridge exists.**

Run:

```bash
build/debug/spotlight-tests
```

Expected: the injected constructor/bridge or shared update assertion fails before implementation.

- [ ] **Step 3: Add catalog serialization and Rust ingestion.**

Serialize the shared snapshot fields required by ranking/search into a bounded JSON array. Add the C ABI setter, parse it into the existing Rust desktop-entry/searchable projection, preserve usage ranking and locale behavior, and make `reload()` refresh usage/config without rescanning directories when an external catalog is attached.

- [ ] **Step 4: Inject catalog and launcher ownership into SpotlightController.**

Connect `DesktopEntryCatalog::indexUpdated` to one deterministic reload callback, remove production directory watcher creation from the injected path, and use the shared `ApplicationLauncher` signals for pending launch correlation and completion. Preserve show/dismiss, query reset, selection, result model, weather, game-mode, and `astrea-launch --desktop` behavior.

- [ ] **Step 5: Run focused green verification and commit.**

Run:

```bash
cmake --build build/debug --target spotlight-tests -j1
build/debug/spotlight-tests
```

Commit:

```bash
git add Spotlight/backend Spotlight/platform/rust Spotlight/core Spotlight/tests \
        shared/launch
git diff --cached --check
git commit -m "refactor(spotlight): consume shared catalog and launch state"
```

### Task 5: Extract feature libraries and add the unified shell composition

**Files:**
- Create: `Shell/CMakeLists.txt`
- Create: `Shell/app/main.cpp`
- Create: `Shell/app/AstreaShellApplication.hpp`
- Create: `Shell/app/AstreaShellApplication.cpp`
- Create: `Shell/core/ShellRuntime.hpp`
- Create: `Shell/core/ShellRuntime.cpp`
- Create: `Shell/core/ShellShortcutDispatcher.hpp`
- Create: `Shell/core/ShellShortcutDispatcher.cpp`
- Modify: `CMakeLists.txt`
- Modify: `Dock/CMakeLists.txt`
- Modify: `AltTab/CMakeLists.txt`
- Modify: `Spotlight/CMakeLists.txt`
- Modify: `Dock/app/DockApplication.*` and `AltTab/app/AltTabApplication.*` only as needed to extract reusable wiring
- Modify: `Spotlight/app/SpotlightApplication.*` only as needed to extract reusable wiring
- Create: `Shell/tests/ShellRuntimeOwnershipTest.cpp`
- Create: `Shell/tests/ShellFeatureIsolationTest.cpp`
- Modify: `Shell/CMakeLists.txt` test registration

**Interfaces:**
- `ShellRuntime` owns `DesktopEntryCatalog`, `AppIdentityResolver`, shared `ApplicationLauncher`, shared Typhon session/toplevel/shortcut clients, icon provider/cache, feature controllers, and shortcut dispatcher.
- `AstreaShellApplication::start()` returns `false` only for fatal shared-runtime or Dock-root failures; AltTab/Spotlight root failure is recorded and leaves the rest running.
- `ShellShortcutDispatcher` maps each shortcut name to exactly one controller operation and ignores cancelled/stale generation events.
- Test-only ownership counters report one instance for each shared service without global singleton state.

- [ ] **Step 1: Add failing ownership and isolation tests.**

Construct `ShellRuntime` with deterministic test dependencies and assert one catalog, resolver, launcher, Typhon session, shortcut dispatcher, and IPC owner. Drive a Dock action, AltTab selection/dismissal, Spotlight query reset, and shared toplevel update; assert feature-local state remains isolated while projections update from one shared snapshot.

- [ ] **Step 2: Run the new tests and verify they fail before the shell composition exists.**

Run:

```bash
cmake --build build/debug --target shell-runtime-ownership-test shell-feature-isolation-test -j1
```

Expected: targets are unavailable or fail at the missing runtime interfaces.

- [ ] **Step 3: Extract reusable feature construction without changing controller policy.**

Move only dependency wiring out of the three old application classes. Keep Dock, AltTab, and Spotlight controllers/models and their QML-facing properties intact. Build reusable static feature targets so the unified shell can link them while legacy wrapper targets link only the client code.

- [ ] **Step 4: Implement `ShellRuntime` and `AstreaShellApplication`.**

Create one `QGuiApplication` in `Shell/app/main.cpp`, construct one `ShellRuntime`, start the shared services once, create the three controllers, and inject the shared catalog/resolver/launcher/Typhon objects. Keep `main.cpp` limited to application bootstrap and return `EXIT_FAILURE` on fatal start failure.

- [ ] **Step 5: Implement one shortcut dispatcher.**

Register existing shortcut names through the shared Typhon client once and route `alt_tab_next`, `alt_tab_previous`, and `alt_tab_commit` only to AltTab. Spotlight's shortcut route uses the existing accepted command/config path and does not introduce duplicate Typhon registrations. Reconnect re-registers exactly once per shared generation.

- [ ] **Step 6: Run ownership/isolation green verification and commit.**

Run:

```bash
cmake --build build/debug --target shell-runtime-ownership-test shell-feature-isolation-test astrea-shell -j1
build/debug/shell-runtime-ownership-test
build/debug/shell-feature-isolation-test
```

Commit:

```bash
git add Shell CMakeLists.txt Dock/CMakeLists.txt AltTab/CMakeLists.txt Spotlight/CMakeLists.txt \
        Dock/app AltTab/app Spotlight/app
git diff --cached --check
git commit -m "refactor(shell): introduce unified runtime composition"
```

### Task 6: Load all roots through one QML engine and prove lifecycle reset

**Files:**
- Modify: `Shell/CMakeLists.txt`
- Create or modify: `Shell/qml` resource/module registration
- Modify: `Dock/qml/Main.qml` only for explicit root object identity/reset hooks if required
- Modify: `AltTab/qml/Main.qml` and `AltTab/qml/components/AltTabPanel.qml` only for reset hooks if required
- Modify: `Spotlight/qml/Main.qml` and `Spotlight/qml/components/SpotlightPanel.qml` only for reset hooks if required
- Create: `Shell/tests/ShellQmlLifecycleTest.cpp`
- Modify: `Shell/CMakeLists.txt` test registration

**Interfaces:**
- `AstreaShellApplication` owns one engine and loads Dock, AltTab, and Spotlight roots through their existing modules/resources.
- Root object names are stable (`dockRoot`, `altTabRoot`, `spotlightRoot`) for deterministic test lookup.
- Each controller exposes an explicit reset method used by dismissal; visual components remain QML.

- [ ] **Step 1: Add failing QML lifecycle tests.**

Load the three existing roots in one engine with test controllers, show/dismiss AltTab and Spotlight 100 times, and assert one root per feature, zero accumulated models/connections, cleared query/selection/hover/focus state, and Dock resident state unchanged.

- [ ] **Step 2: Run the focused lifecycle test and confirm current three-module loading cannot satisfy one-engine ownership.**

Run:

```bash
build/debug/shell-qml-lifecycle-test
```

Expected: the target is unavailable or the one-engine/root-count assertions fail before unified QML loading.

- [ ] **Step 3: Register the existing QML files under the shell target.**

Use one shell QML resource/module registration, preserve relative component imports, add the required `Astrea.Shared` import path and `Qt5Compat` linkage, and do not merge or rewrite the visual components.

- [ ] **Step 4: Add explicit transient reset calls.**

Make AltTab dismissal clear selection/hover/focus and stale target state through its controller/model boundary. Make Spotlight dismissal clear query, selected index, pending result state, and focus request state. Do not reset Dock pins/running state on transient root dismissal.

- [ ] **Step 5: Implement controlled root loading and diagnostics.**

Load Dock first and fail startup on a missing Dock root. Load AltTab and Spotlight independently, log precise root/module errors, and keep the shell alive when either optional root cannot load. Ensure no QML warning introduced by changed files.

- [ ] **Step 6: Run lifecycle and QML validation, then commit.**

Run:

```bash
cmake --build build/debug --target shell-qml-lifecycle-test astrea-shell -j1
build/debug/shell-qml-lifecycle-test
qmllint Dock/qml/Main.qml Dock/qml/components/*.qml \
        AltTab/qml/Main.qml AltTab/qml/components/*.qml \
        Spotlight/qml/Main.qml Spotlight/qml/components/*.qml
```

Commit:

```bash
git add Shell Dock/qml AltTab/qml Spotlight/qml
git diff --cached --check
git commit -m "feat(shell): host Dock AltTab and Spotlight roots"
```

### Task 7: Add unified IPC, nonresident wrappers, and systemd migration

**Files:**
- Create: `shared/platform/ipc/ShellIpcProtocol.hpp`
- Create: `shared/platform/ipc/ShellIpcClient.hpp`
- Create: `shared/platform/ipc/ShellIpcClient.cpp`
- Create: `Shell/platform/ipc/ShellIpcServer.hpp`
- Create: `Shell/platform/ipc/ShellIpcServer.cpp`
- Create: `Shell/tests/ShellIpcTest.cpp`
- Modify: `Dock/app/main.cpp`, `Dock/app/CommandLine.cpp`, `Dock/app/CommandLine.hpp`
- Modify: `AltTab/app/main.cpp`, `AltTab/app/CommandLine.cpp`, `AltTab/app/CommandLine.hpp`
- Modify: `Spotlight/app/main.cpp`, `Spotlight/app/CommandLine.cpp`, `Spotlight/app/CommandLine.hpp`
- Create: `Shell/packaging/systemd/astrea-shell.service`
- Modify: `Dock/CMakeLists.txt`, `AltTab/CMakeLists.txt`, `Spotlight/CMakeLists.txt`, `Shell/CMakeLists.txt`
- Create: `Shell/tests/SystemdLayoutTest.cmake`

**Interfaces:**
- Fixed endpoint: `astrea-shell-v1`.
- Fixed command vocabulary: `dock.status|reload|show|hide|quit`, `alt_tab.next|previous|commit|cancel|show|hide|reload_windows|status`, `spotlight.show|hide|toggle|query|activate|reload_index|status`.
- Bounded UTF-8 line/payload size is 4096 bytes; status replies remain JSON and are feature-compatible where practical.
- `ShellIpcClient::send()` and `requestReply()` return typed success/error outcomes; no client starts a daemon fallback.

- [ ] **Step 1: Add failing IPC and wrapper tests.**

Assert that one server accepts fixed commands, routes each domain once, rejects oversize/unknown commands, and returns a nonzero typed error when absent. Invoke each legacy CLI in a temporary environment and assert it sends one shell command without constructing `QGuiApplication` or a resident server. Assert `--daemon` returns nonzero.

- [ ] **Step 2: Run focused IPC tests and confirm the endpoint/wrappers do not exist.**

Run:

```bash
build/debug/shell-ipc-test
build/debug/shell-systemd-layout-test
```

Expected: targets are unavailable or fail before implementation.

- [ ] **Step 3: Implement the bounded protocol and server.**

Use direct typed command dispatch inside `ShellRuntime`; preserve existing command behavior and status fields. Do not keep the three resident feature servers in the production shell target.

- [ ] **Step 4: Convert legacy executables to thin clients.**

Use `QCoreApplication` only. Preserve CLI parsing and diagnostics modes that do not need a resident runtime. Map all retained commands to `astrea-shell-v1`; for `--daemon`, print a clear migration error and return nonzero.

- [ ] **Step 5: Install only the unified service.**

Install `astrea-shell.service` with the current graphical-session ordering, restart policy, capability environment, and Spotlight icon-theme environment. Stop installing old persistent service files and static-test the resulting install manifest.

- [ ] **Step 6: Run focused green verification and commit.**

Run:

```bash
cmake --build build/debug --target shell-ipc-test astrea-shell astrea-dock astrea-alt-tab astrea-spotlight -j1
build/debug/shell-ipc-test
cmake --install build/debug --prefix "$PWD/build/debug/stage"
cmake -P Shell/tests/SystemdLayoutTest.cmake
```

Commit:

```bash
git add shared/platform/ipc Shell/packaging/systemd Shell/tests \
        Dock/app Dock/CMakeLists.txt AltTab/app AltTab/CMakeLists.txt \
        Spotlight/app Spotlight/CMakeLists.txt Shell/CMakeLists.txt
git diff --cached --check
git commit -m "feat(systemd): migrate shell to unified service and IPC"
```

### Task 8: Reconnect, preservation, measurement, and qualification closure

**Files:**
- Create: `Shell/tests/ShellUnifiedRuntimeIntegrationTest.cpp`
- Create: `Shell/tests/ShellResourceMeasurementTest.cmake`
- Modify: `Dock/tests/DockTyphonRuntimeIntegrationTest.cpp` only if shared-runtime adapters need a production-wiring regression
- Modify: `AltTab/tests/AltTabControllerTest.cpp` and `AltTab/tests/AltTabQmlSelectionTest.cpp` only for shared-runtime fixture coverage
- Modify: `Spotlight/tests/SpotlightBackendTest.cpp` only for unified fixture coverage
- Create: `docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell.md`
- Modify: `docs/superpowers/plans/2026-08-09-m7-d-unified-shell-runtime.md` to check off completed steps and record actual commands

**Interfaces:**
- Integration fixture drives the shared session, all three controllers, one IPC server, and deterministic fake Wayland/compositor events.
- Measurement script records process count, PSS/private memory where available, RSS, threads, FDs, Typhon connection count, and idle CPU; unavailable native measurements are recorded as unavailable rather than inferred.
- Qualification ledger records starting HEAD, final HEAD, Typhon pin, before/after topology, every build/test result, QML, sanitizer, source-layout, and whitespace evidence.

- [ ] **Step 1: Add failing unified reconnect/preservation tests.**

Exercise one shell runtime through authentication, initial snapshot, Dock exact activation, AltTab exact selected-window activation, Spotlight search/launch, shortcut dispatch, disconnect, reconnect, fresh authentication, snapshot rebuild, shortcut restoration, and shutdown. Assert one reconnect owner, one active generation, one action completion per request, and preservation of M7-C stale/unavailable/no-launch behavior.

- [ ] **Step 2: Run the focused integration test and verify missing unified recovery behavior.**

Run:

```bash
build/debug/shell-unified-runtime-integration-test
```

Expected: the test fails on absent unified wiring before production implementation is complete.

- [ ] **Step 3: Implement the smallest production-wiring fixes exposed by the test.**

Trace each failure to its boundary before editing. Preserve local-vs-protocol errors and exact action semantics. Do not add sleeps, duplicate reconnect timers, alternate launch fallback, or protocol-specific policy.

- [ ] **Step 4: Run focused green regression suites.**

Run the shared ownership/lifecycle/IPC/unified tests plus the complete existing M7-C focused suites:

```bash
ctest --test-dir build/debug --output-on-failure -j1 -R 'shell-|dock-typhon-runtime-integration-test|alttab-(controller|qml-selection|shortcut-routing)-test|spotlight-tests|typhon-'
```

Require zero failures and no unexpected skips.

- [ ] **Step 5: Record resource and topology evidence.**

Measure the current three-process topology and unified shell under equivalent idle conditions. Use PSS/private memory as the primary memory comparison, retain RSS only as reference, count actual Wayland connections rather than assuming Qt's total, and record unavailable native compositor data explicitly.

- [ ] **Step 6: Run every established build and complete serial CTest suite.**

Configure/build and test exactly:

```bash
for dir in build/debug build/release build/clang build/asan build/ubsan build/no-typhon; do
    cmake --build "$dir" -j1
    ctest --test-dir "$dir" --output-on-failure -j1
done
```

Record exact totals per directory. The no-Typhon build must compile and degrade without creating a Typhon connection.

- [ ] **Step 7: Run final static checks and architecture audit.**

Run:

```bash
git diff --check
git log --check 6fc6f7fec12f78f7396ae57386753d2c4af2153f..HEAD
rg -n 'QGuiApplication|QQmlApplicationEngine|TyphonToplevelConnection|TyphonActionState|DesktopEntryCatalog|AppIdentityResolver|astrea-dock|astrea-alt-tab|astrea-spotlight|astrea-shell' Shell Dock AltTab Spotlight shared
find Shell Dock AltTab Spotlight -path '*/packaging/systemd/*' -type f -print
```

Confirm production ownership counts and no old persistent service installation. Run `qmllint` on all roots/components affected by the unified loading path and repeat ASan/UBSan lifecycle coverage through startup, roots, actions, reconnect, and shutdown.

- [ ] **Step 8: Write and commit the qualification ledger.**

Fill `docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell.md` with actual evidence and the exact decision:

```text
M7 A/B/C/D IMPLEMENTATION COMPLETE — INTEGRATED NATIVE QUALIFICATION DEFERRED
```

Use the failure decision instead if any gate remains open. Commit all final documentation and plan checkboxes with:

```bash
git add docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell.md \
        docs/superpowers/plans/2026-08-09-m7-d-unified-shell-runtime.md
git diff --cached --check
git commit -m "docs(shell): finalize M7-D deterministic qualification"
```
