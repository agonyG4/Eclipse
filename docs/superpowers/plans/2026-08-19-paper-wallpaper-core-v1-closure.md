# Astrea Paper Wallpaper Core v1 Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Paper the single native wallpaper authority with stable IDs, a catalog, managed user imports, transactional selection persistence, safe effective fallback, committed events, and a narrow Paper control adapter.

**Architecture:** Evolve the existing `Paper::WallpaperService` into the controller compatibility surface. Add `WallpaperCatalog` for system/user discovery and atomic content-addressed imports, and add `WallpaperSelection`/store semantics behind the existing atomic INI persistence. Keep desktop/Settings consumers on the existing Paper snapshot and dedicated Paper socket; do not add wallpaper semantics to Typhon's compositor protocol.

**Tech Stack:** C++20, Qt 6.8 Core/Gui/Network/Qml/Quick/Test, CMake/CTest, QML, Rust `astreactl` direct Paper Unix-socket client.

## Global Constraints

- v1 supports static image wallpapers only.
- v1 is global scope only; do not expose per-output or per-workspace selection.
- Supported public modes are `cover`, `contain`, `stretch`, and `center`; retain `tile` only for compatibility where already required.
- Paper owns state, resolution, persistence, catalog access, events, and control semantics.
- QML is presentation-only and never writes wallpaper state or assets.
- Regulus is not added unless a usable implementation is already present; the current atomic INI backend remains behind the selection boundary.
- No `wallpaperctl` and no wallpaper messages in Typhon's compositor control protocol.
- Preserve unrelated dirty work and use the existing `build/debug`, `build/no-layer-shell`, and `build/release` directories.
- Every mutation publishes only after validation and successful persistence; failed mutations retain the previous authoritative state.

## File map

### Create

- `Paper/core/WallpaperCatalog.hpp/.cpp` — system/user catalog snapshot, stable IDs, validated atomic imports.
- `Paper/tests/WallpaperCatalogTest.cpp` — catalog listing, ID, import, duplicate, and failure tests.
- `Paper/tests/WallpaperSelectionStoreTest.cpp` — stable-ID persistence/reconstruction and reset compatibility tests if the existing persistence test becomes too broad.

### Modify

- `Paper/core/WallpaperDescriptor.hpp/.cpp` — origin/display metadata and stable-ID helpers while preserving existing JSON compatibility.
- `Paper/core/WallpaperPersistence.hpp/.cpp` — selection store boundary and stable-ID-first persistence with legacy descriptor migration.
- `Paper/core/WallpaperService.hpp/.cpp` — catalog-backed `current/default/list/import/select/reset`, serialized transactional commit, committed change event, and missing-ID fallback.
- `Paper/platform/ipc/WallpaperControlServer.cpp/.hpp` — `list`, `import`, stable-ID `set`, path-oriented `set` as `import + select`, and bounded JSON responses.
- `Paper/CMakeLists.txt` — catalog source and tests.
- `Paper/tests/WallpaperDescriptorTest.cpp`, `WallpaperPersistenceTest.cpp`, `WallpaperServiceTest.cpp`, `WallpaperControlServerTest.cpp` — extend existing coverage without removing current behavior.
- `Settings/services/wallpaper/SettingsWallpaperController.hpp/.cpp` — catalog/list/import/select projections and typed errors.
- `Settings/qml/pages/appearance/Wallpaper.qml` — consume catalog IDs and native import/select methods; no filesystem access.
- `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`, `Settings/tests/integration/SettingsQmlSmokeTest.cpp` — controller/catalog integration coverage.
- `/home/agony/GitHub/Typhon/src/astreactl/wallpaper.rs`, `src/bin/astreactl.rs`, and focused tests — route the existing direct Paper endpoint for `list`, `import`, ID `set`, and `reset`; do not touch the compositor protocol.
- `REPORT-2026-08-19-paper-wallpaper-core-v1-closure.md` — final evidence report.

---

### Task 1: Establish the current-source audit

**Files:**

- Read-only: `Paper/core/*`, `Paper/platform/ipc/*`, `Paper/platform/wayland/*`, `Settings/services/wallpaper/*`, `Settings/qml/pages/appearance/Wallpaper.qml`, `Shell/runtime/ShellRuntime.cpp`, `Shell/app/AstreaShellApplication.cpp`.
- Create: `docs/superpowers/specs/2026-08-19-paper-wallpaper-core-v1-design.md`.

**Interfaces:** Consumes the existing working-tree baseline; produces the evidence map and selected architecture used by all later tasks.

- [x] Record `git rev-parse HEAD`, `git log --oneline -15`, `git status --short`, `git diff --stat`, and `git diff --name-only` before changes.
- [x] Classify current authority, persistence, mutations, consumers, lockscreen, cache, catalog, and external-control findings as current or historical.
- [x] Document the no-Regulus and no-Typhon-protocol-coupling decisions.
- [x] Validate the design document contains no unqualified historical claims.

**Validation:** `git diff --check` and manual review of the design against current source.

**Commit boundary:** `docs: design Paper wallpaper core v1 closure` (do not create automatically unless requested).

### Task 2: Add stable descriptor metadata and catalog authority

**Files:**

- Modify: `Paper/core/WallpaperDescriptor.hpp/.cpp`.
- Create: `Paper/core/WallpaperCatalog.hpp/.cpp`, `Paper/tests/WallpaperCatalogTest.cpp`.
- Modify: `Paper/CMakeLists.txt`.

**Interfaces:** `WallpaperCatalog(QString userDirectory = {}, QString systemDirectory = {})`; `QVector<WallpaperDescriptor> list() const`; `std::optional<WallpaperDescriptor> resolve(const QString &id) const`; `bool contains(const QString &id) const`; `std::optional<WallpaperDescriptor> importWallpaper(const QString &path, QString *error = nullptr)`; `void refresh()`.

- [ ] Write failing tests for stable system ID, user origin, catalog listing, and unknown-ID rejection.
- [ ] Run `cmake --build build/no-layer-shell --target paper-catalog-test` or configure the existing build target and verify the new tests fail because the catalog is absent.
- [ ] Add `WallpaperOrigin`, `displayName`, and stable-ID helpers without removing existing `logicalId`, JSON fields, or equality compatibility.
- [ ] Implement a catalog snapshot that registers the resolver's valid factory default as `astrea://wallpaper/default` and scans only the configured system/user roots.
- [ ] Ensure the catalog never returns a directory, special file, unsupported image, or incomplete import.
- [ ] Re-run `paper-catalog-test` and verify the tests pass.

**Validation:** `cmake --build build/no-layer-shell --target paper-catalog-test`; `ctest --test-dir build/no-layer-shell -R paper-catalog-test --output-on-failure`.

**Commit boundary:** `feat(paper): add stable wallpaper catalog`.

### Task 3: Implement atomic managed user import

**Files:**

- Modify: `Paper/core/WallpaperCatalog.cpp/.hpp`.
- Modify: `Paper/tests/WallpaperCatalogTest.cpp`.

**Interfaces:** Import returns a descriptor whose `logicalId` is `astrea://wallpaper/user/<sha256>` and whose source is the managed copy; repeated identical content returns the existing descriptor.

- [ ] Write failing tests for valid import, malformed/unsupported image, directory, FIFO/special file, failed source access, duplicate import, and no selection change on failure.
- [ ] Run only the catalog test and verify the new cases fail before implementation.
- [ ] Validate source metadata and image dimensions before copying; cap file size and decoded pixel count.
- [ ] Compute SHA-256 once, copy to a temporary file inside the user root, validate the copy with `QImageReader`, and atomically rename to the digest-based final path.
- [ ] Publish the descriptor only after the rename succeeds; clean up the temporary file on every failure.
- [ ] Re-run catalog tests and verify no partial file is listed.

**Validation:** focused CTest plus `git diff --check`.

**Commit boundary:** `feat(paper): import wallpapers into managed user library`.

### Task 4: Put stable selection behind the persistence boundary

**Files:**

- Modify: `Paper/core/WallpaperPersistence.hpp/.cpp`.
- Modify/create: `Paper/tests/WallpaperPersistenceTest.cpp`, `Paper/tests/WallpaperSelectionStoreTest.cpp`.

**Interfaces:** `WallpaperSelection { QString wallpaperId; WallpaperFit fit; }`; `WallpaperPersistence::loadSelection/saveSelection/clear` with compatibility defaults for existing descriptor callers.

- [ ] Write failing tests that save a stable ID and fit, reconstruct the persistence object, and load the same selection without requiring the original absolute source path.
- [ ] Write a failing test that reads the old descriptor format and exposes it as a migration candidate rather than losing it.
- [ ] Run the persistence tests and confirm failure before changing persistence.
- [ ] Store ID and fit as authoritative fields; retain legacy source only as migration/recovery metadata.
- [ ] Keep `QSaveFile` atomic writes and preserve `clear()` behavior.
- [ ] Re-run persistence tests, including legacy symlink idempotence and reset persistence.

**Validation:** `ctest --test-dir build/no-layer-shell -R 'paper-persistence|paper-selection' --output-on-failure`.

**Commit boundary:** `feat(paper): persist stable wallpaper selection`.

### Task 5: Make WallpaperService the transactional controller

**Files:**

- Modify: `Paper/core/WallpaperService.hpp/.cpp`.
- Modify: `Paper/tests/WallpaperServiceTest.cpp`.
- Modify: `Shell/runtime/ShellRuntime.cpp` to construct/inject the catalog.

**Interfaces:** `current()`, `defaultWallpaper()`, `listWallpapers()`, `importWallpaper(path, fit)`, `selectWallpaper(id, fit)`, `setWallpaperSource(path, fit)` as an `import + select` convenience, `resetWallpaper()`, and a committed `wallpaperChanged(previous,current,generation,reason)` signal or equivalent.

- [ ] Write failing tests for configured/default/fallback resolution, missing configured ID, persistence failure, reset, one success event, zero success events on failure, and rapid A/B/C requests.
- [ ] Run `paper-service-test` and verify each new test fails for the missing catalog-backed operation or event.
- [ ] Resolve configured stable IDs through the catalog while retaining missing configured intent in the snapshot.
- [ ] Serialize mutation requests with the existing validation token/generation machinery; never publish before persistence succeeds.
- [ ] Keep the existing emergency fallback and route path-oriented compatibility through managed import.
- [ ] Emit one committed change event only when effective authoritative state changes; treat initialization as an initial snapshot.
- [ ] Add stale-completion coverage proving an older validation cannot roll back a newer committed selection.
- [ ] Re-run service tests and confirm all old and new tests pass.

**Validation:** `ctest --test-dir build/no-layer-shell -R paper-service-test --output-on-failure`.

**Commit boundary:** `feat(paper): make wallpaper service the transactional controller`.

### Task 6: Extend the dedicated Paper control adapter

**Files:**

- Modify: `Paper/platform/ipc/WallpaperControlServer.cpp/.hpp`.
- Modify: `Paper/tests/WallpaperControlServerTest.cpp`.

**Interfaces:** JSON commands `wallpaper get {}`, `wallpaper list {}`, `wallpaper import {"path":"...","fit":"cover"}`, `wallpaper set {"id":"...","fit":"cover"}`, and `wallpaper reset {}`; a path-oriented `set {"source":"..."}` remains a compatibility convenience implemented as `import + select`.

- [ ] Write failing IPC tests for list, import, stable-ID set, malformed ID, and bounded response behavior.
- [ ] Run `paper-control-server-test` and verify the new commands fail as unknown or invalid.
- [ ] Add command parsing with strict action/argument validation and typed domain errors.
- [ ] Serialize import/select operations through the controller, route path-oriented `set` through `importWallpaper`, and return only final completed results.
- [ ] Keep the secure endpoint ownership, client bounds, timeout, and Unicode/path-as-data behavior.
- [ ] Re-run the complete Paper IPC test.

**Validation:** `ctest --test-dir build/no-layer-shell -R paper-control-server-test --output-on-failure`.

**Commit boundary:** `feat(paper): expose catalog and stable-id control operations`.

### Task 7: Migrate Settings to IDs and catalog snapshots

**Files:**

- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp/.cpp`.
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml`.
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`, `Settings/tests/integration/SettingsQmlSmokeTest.cpp`.

**Interfaces:** native controller properties expose catalog entries and current IDs; QML invokes `importWallpaper`, `selectWallpaper`, and `reset` without filesystem access.

- [ ] Write failing controller tests for list projection, import/select request serialization, and stable-ID snapshot projection.
- [ ] Run the focused Settings tests and verify failure before adding properties/methods.
- [ ] Add bounded catalog projection and typed error mapping in native C++.
- [ ] Replace path-as-identity UI state with catalog ID state while retaining a compatibility path entry point.
- [ ] Keep asynchronous preview loading and busy/error presentation in QML only.
- [ ] Re-run focused Settings unit and QML smoke tests.

**Validation:** `ctest --test-dir build/no-layer-shell -R 'settings-wallpaper-controller|settings-qml-smoke' --output-on-failure`.

**Commit boundary:** `feat(settings): consume Paper wallpaper catalog`.

### Task 8: Extend `astreactl` through the Paper endpoint only

**Files:**

- Modify: `/home/agony/GitHub/Typhon/src/astreactl/wallpaper.rs`, `/home/agony/GitHub/Typhon/src/bin/astreactl.rs`, focused Rust tests.

**Interfaces:** `astreactl wallpaper get|list|import <path>|set <id>|reset`; preserve existing direct Paper endpoint discovery and final-completion validation. No Typhon compositor protocol changes.

- [ ] Write failing parser/client tests for list/import/ID set and path quoting.
- [ ] Run `cargo test astreactl::wallpaper --lib` and `cargo test --bin astreactl` to verify the new cases fail.
- [ ] Add JSON body construction and result projection for the new Paper commands.
- [ ] Preserve secure socket validation, request/operation timeout margin, bounded frames, and error-code propagation.
- [ ] Re-run focused Rust tests and `cargo fmt --check`.

**Validation:** `cargo fmt --check`; `cargo test astreactl::wallpaper --lib`; `cargo test --bin astreactl`; `cargo check --bin astreactl`.

**Commit boundary:** `feat(astreactl): route wallpaper catalog operations to Paper`.

### Task 9: Validate consumers, migration, cache boundaries, and idle behavior

**Files:**

- Modify: `Paper/core/WallpaperSourceWatcher.*` only if tests expose a missing-asset race.
- Modify: `Paper/platform/wayland/WallpaperSurfaceManager.*`, `WallpaperSurfaceBundle.*`, and `Paper/qml/WallpaperSurface.qml` only for ID/generation propagation.
- Add tests under `Paper/tests` for stale source recovery and disposable derived state.
- Create: `REPORT-2026-08-19-paper-wallpaper-core-v1-closure.md`.

**Interfaces:** renderer consumes effective descriptor/generation only; no persistence or catalog filesystem knowledge enters renderer/QML.

- [ ] Add a missing-source test proving configured intent remains while effective fallback stays valid.
- [ ] Add a stale generation test proving an old QML/image completion cannot promote over a newer generation.
- [ ] Verify the current source has no lockscreen consumer; document this as an explicit future integration, not a claimed implementation.
- [ ] Run the full relevant CTest subset, Rust focused tests, `git diff --check`, and final `git status --short`.
- [ ] Record native qualification only if the actual Eclipse/Astrea session is available; otherwise state the limitation.
- [ ] Complete the final report with baseline, authority map, implementation evidence, tests, blockers, and future milestones.

**Validation:** `ctest --test-dir build/no-layer-shell --output-on-failure` for the relevant suite, `cargo fmt --check`, focused Cargo tests, and manual report review.

**Commit boundary:** `docs: report Paper wallpaper core v1 closure`.

## Execution status

Tasks 2 through 8 are implemented in the current working tree, including managed import, stable selection persistence, legacy-symlink import migration, committed events, Paper IPC, Settings catalog projection, and direct `astreactl` support. Focused tests were compiled and run with the installed Qt 6 toolchain, and the configured `no-layer-shell` tree ran the focused Paper/Settings CTest selection with 8/8 tests passing. Repository-level CMake regeneration remains blocked by an unrelated dirty-work reference to the missing `Bar/qml/NetworkPopup.qml`; that source was intentionally not recreated or changed. Task 9's native-session qualification and a clean full-repository CTest run remain explicitly unavailable and are recorded in the closure report.
