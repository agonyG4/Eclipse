# Astrea Paper Wallpaper API v1 — Implementation Plan

> **Execution note:** Follow the test-first order below. Before each production implementation slice, add or extend the named failing test and run the narrowest relevant command to capture the red result. Existing dirty files are user work; inspect overlapping diffs and make additive patches only.

**Goal:** Make Eclipse’s native Paper service the sole wallpaper authority for configured/default/effective state, expose a secure local control API, render the effective image on current outputs, provide a native Settings consumer, and add a thin Typhon `astreactl wallpaper` client without moving state into Typhon.

**Repositories:** `/home/agony/GitHub/Eclipse` and `/home/agony/GitHub/Typhon`  
**Design:** `Eclipse/docs/superpowers/specs/2026-08-19-paper-wallpaper-api-v1-design.md`  
**Technology:** C++20, Qt 6, Qt Quick, LayerShellQt, QTest; Rust/Clap/Serde for the Typhon CLI

## Task 1: Add Paper core target and typed descriptor tests

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/CMakeLists.txt`.
- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperDescriptor.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperTypes.hpp` if the enum/value declarations need a separate public header.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperDescriptorTest.cpp`.
- Update `/home/agony/GitHub/Eclipse/CMakeLists.txt` to add `Paper` before consumers.
- Update `/home/agony/GitHub/Eclipse/shared/CMakeLists.txt` or Paper’s target links only if an existing shared target is required; do not duplicate shared helpers.

**Failing test first:** Add tests for enum string conversion, valid Image/Global descriptors, rejected Dynamic/Video/Slideshow and Output descriptors, URI/path round trips, fit conversion, and JSON/QVariant snapshot conversion. Run the Paper test target; the expected red result is missing Paper headers/target or unimplemented value conversion.

**Implementation:** Create Qt value types with typed enums. Keep source identity (`source`, `logicalId`, `sourceKind`) separate from runtime `resolvedSource` and presentation (`fit`, `scope`). Define stable serialization helpers for the control boundary without making JSON the internal authority. Link Qt Core/Gui and expose the target to Shell and Settings consumers.

**Verification:** Configure/build the focused Paper test target, run `ctest --test-dir <build> -R wallpaper-descriptor`, and run `git diff --check`. Commit boundary: descriptor and target only.

## Task 2: Implement resolver and factory/emergency source tests

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperResolver.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/assets/emergency.svg` and register it in Paper’s Qt resource/QML module.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperResolverTest.cpp`.
- Extend `/home/agony/GitHub/Eclipse/Paper/CMakeLists.txt`.

**Failing test first:** Add temporary-directory tests for absolute paths, `file://` URIs, leading `~`, spaces, Unicode, symlinked image files, directories, missing files, malformed URI/path, unsupported image, and factory default fallback. The expected red result is absent resolver behavior and no registered emergency resource.

**Implementation:** Resolve and canonicalize only at the API boundary. Validate regular readable images through `QImageReader`; reject unsupported kind/scope before touching disk. Implement a resource provider that checks configured Astrea product locations and Eclipse resources, then returns the embedded emergency image with an explicit fallback reason. Do not use shell commands or symlink state paths.

**Verification:** Run resolver tests under a clean temporary XDG environment; assert no test writes to the repository. Run the focused target and `git diff --check`. Commit boundary: resolver/resource behavior.

## Task 3: Add persistence interface/adapter and service state-machine tests

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperPersistence.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperService.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperPersistenceTest.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperServiceTest.cpp`.
- Extend `/home/agony/GitHub/Eclipse/Paper/CMakeLists.txt`.

**Failing test first:** Write tests for empty startup, valid persisted override, configured source missing at startup, transactional set success/failure, configured-source retention, reset, factory fallback, emergency fallback, generation changes, last-error/state signals, and atomic-write failure. Run the target before implementation; the expected red result is missing service/persistence symbols.

**Implementation:** Define the narrow `WallpaperPersistence` interface and an XDG INI adapter. Persist source plus typed kind/fit/scope, never a symlink or transient resolved path. Use a same-directory temporary file and atomic rename with restrictive permissions. Implement `WallpaperService` as the authority: load configured/default/effective state, retain invalid configured values, make set transactional, reset persistence, and publish typed snapshots/signals. Add an injectable resolver/persistence seam so tests do not depend on the current desktop session.

**Verification:** Run service/persistence tests with isolated XDG config/data/runtime directories. Verify a failed set leaves both persisted and effective state unchanged. Verify reset does not delete the source image. Commit boundary: core synchronous state and persistence.

## Task 4: Implement bounded async validation and watcher behavior

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/core/WallpaperValidationWorker.hpp` and `.cpp` (or keep the worker private to `WallpaperService` if that preserves the same bounded queue contract).
- Extend `/home/agony/GitHub/Eclipse/Paper/core/WallpaperService.hpp` and `.cpp`.
- Extend `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperServiceTest.cpp` or add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperConcurrencyTest.cpp`.

**Failing test first:** Add deterministic fake-worker tests for `A -> B -> C` latest-request-wins, stale success after a newer success, stale failure after a newer success, reset during validation, replacing a configured file in place, and service destruction during active work. The expected red result is stale commits, queued-work growth, or missing cancellation tokens.

**Implementation:** Use one worker thread with one active request and one replaceable pending request. Attach request tokens to every completion and commit only the current token on the service thread. Validate/decode off the UI thread. Publish factory/effective state while configured validation is pending, and preserve the previous renderable image on failed replacement. Use a bounded QFileSystemWatcher scope for configured file/parent changes; watcher events trigger a coalesced reload, not polling.

**Verification:** Run concurrency tests repeatedly, including an instrumented rapid-set loop. Assert the worker’s maximum pending count is one and final effective state is C. Run ThreadSanitizer if the existing Eclipse build supports it; otherwise record the unavailable check. Commit boundary: async state machine.

## Task 5: Add the native background renderer

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/platform/wayland/WallpaperSurfaceManager.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/qml/WallpaperSurface.qml`.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperSurfaceManagerTest.cpp` and/or an offscreen QML test.
- Extend `/home/agony/GitHub/Eclipse/Paper/CMakeLists.txt` with Qt Quick, LayerShellQt, and QML module/resource wiring.

**Failing test first:** Add tests for a surface per current screen, background layer/all-edge anchors, transparent input, no exclusive zone, effective-only source updates, atomic front/back image swap, failed-load preservation, and screen add/remove. The expected red result is absent manager/QML implementation.

**Implementation:** Follow existing Eclipse Bar surface-manager lifecycle patterns and `AstreaLayerShellConfig`. Create a non-interactive background `QQuickWindow` for every current screen and mirror the global effective descriptor. Keep a renderable background visible until the replacement image is ready. QML receives only the effective descriptor and fit presentation; it does not read config, spawn processes, or inspect filesystem state.

**Verification:** Run offscreen QML tests and the focused manager tests. If a live Wayland session is unavailable, run construction/configuration tests and record live-surface qualification as unavailable. Commit boundary: renderer.

## Task 6: Integrate Paper into Shell and expose the secure control socket

**Files:**

- Add `/home/agony/GitHub/Eclipse/Paper/platform/ipc/WallpaperControlServer.hpp` and `.cpp`.
- Add `/home/agony/GitHub/Eclipse/Paper/tests/WallpaperControlServerTest.cpp`.
- Update `/home/agony/GitHub/Eclipse/Shell/runtime/ShellRuntime.hpp` and `.cpp`.
- Update `/home/agony/GitHub/Eclipse/Shell/app/AstreaShellApplication.hpp` and `.cpp` only at the service/renderer lifecycle and QML context wiring points.
- Update `/home/agony/GitHub/Eclipse/Shell/CMakeLists.txt` only as required to link Paper.

**Failing test first:** Add control-server tests for runtime path creation/modes, get/set/reset/default, malformed/oversized JSON, unknown action, injection-shaped source strings, spaces/Unicode, unsupported enum, bounded response, and client disconnect. The expected red result is missing endpoint/server behavior.

**Implementation:** Create `$XDG_RUNTIME_DIR/astrea-shell` with mode `0700` and listen on a dedicated `wallpaper.sock`. Reuse transport primitives only where safe; do not change the existing `/tmp/astrea-shell-v1` contract. Accept the four Paper actions, parse bounded JSON, call `WallpaperService`, and return a snapshot/error. Integrate service startup before QML initialization, renderer startup after screens are available, and clean shutdown. Expose a native Paper controller/service object to shell QML as needed.

**Verification:** Run control tests under isolated runtime directories and inspect modes with `stat`. Run existing shell IPC tests to ensure the legacy endpoint remains compatible. Build Shell and run its existing test suite. Commit boundary: native Shell integration/control.

## Task 7: Replace the native Settings wallpaper path with a consumer page

**Files:**

- Add/update `/home/agony/GitHub/Eclipse/Settings/core/wallpaper/WallpaperController.hpp` and `.cpp`.
- Update `/home/agony/GitHub/Eclipse/Settings/core/SettingsController.hpp` and `.cpp`.
- Update `/home/agony/GitHub/Eclipse/Settings/app/SettingsApplication.cpp`.
- Add `/home/agony/GitHub/Eclipse/Settings/qml/pages/appearance/Wallpaper.qml` (use the existing Settings QML page convention discovered in the repository).
- Update `/home/agony/GitHub/Eclipse/Settings/qml/CMakeLists.txt`.
- Update `/home/agony/GitHub/Eclipse/Settings/core/navigation/SettingsNavigationCatalog.cpp`.
- Update `/home/agony/GitHub/Eclipse/Settings/tests/unit/SettingsControllerTest.cpp` and relevant QML smoke tests.

**Failing test first:** Extend unit tests for the wallpaper navigation entry and controller context property; add a QML/offscreen test that calls set/reset through the native controller. The expected red result is the missing route/context/controller.

**Implementation:** Make C++ own the Paper client/service connection, state model, errors, and actions. Keep QML presentation-only. Use Qt’s file picker for choosing a source; serialize through the native controller and secure Paper endpoint or in-process service according to the existing Shell/Settings process boundary. Do not add `Quickshell.Io`, `Process`, Python, zenity, direct filesystem reads, symlink inspection, or shell calls. Preserve existing Settings visual conventions and update exact catalog row assertions.

**Verification:** Run Settings unit tests and offscreen QML smoke tests. Search the new wallpaper page/controller for forbidden process/shell/filesystem APIs. Build Settings with its current target. Commit boundary: Settings consumer.

## Task 8: Add Typhon `astreactl wallpaper` client routing

**Files:**

- Add `/home/agony/GitHub/Typhon/src/astreactl/wallpaper.rs` or the smallest existing module boundary for a secure Unix-socket client.
- Update `/home/agony/GitHub/Typhon/src/bin/astreactl.rs` for wallpaper subcommands and help text.
- Update `/home/agony/GitHub/Typhon/src/astreactl/client.rs` and/or `output.rs` for typed snapshot/human/JSON output.
- Update `/home/agony/GitHub/Typhon/src/astreactl/mod.rs` if present.
- Add/update Typhon CLI tests adjacent to the existing `astreactl` tests.

**Failing test first:** Add parser tests for get/set/reset/default, fit validation, paths with spaces/Unicode, rejection of Typhon-only socket overrides for wallpaper, JSON encoding, bounded response, and secure endpoint path construction. The expected red result is unknown command/missing route.

**Implementation:** Route wallpaper commands before Typhon compositor discovery. Connect to `$XDG_RUNTIME_DIR/astrea-shell/wallpaper.sock`, validate owner/parent permissions where the existing Rust platform helpers permit, send one bounded line, and decode the Paper JSON response. Keep `--json` output consistent with existing `astreactl`; do not add wallpaper fields to Typhon daemon state or control protocol.

**Verification:** Run focused Rust tests, `cargo fmt --check`, `cargo clippy --all-targets --all-features -- -D warnings` if the repository’s current dependency state permits, and the existing `astreactl` test suite. Preserve unrelated dirty Rust changes. Commit boundary: CLI adapter only.

## Task 9: Compatibility migration and documentation evidence

**Files:**

- Extend `/home/agony/GitHub/Eclipse/Paper/core/WallpaperPersistence.cpp` only if the one-time legacy import can be implemented without coupling the service to symlink authority.
- Add migration tests to `WallpaperPersistenceTest.cpp` for valid target, stale target, absent record, and idempotence.
- Update the design document only for implementation-discovered facts.
- Add `/home/agony/GitHub/Eclipse/REPORT-2026-08-19-paper-wallpaper-api-v1.md`.

**Failing test first:** Add migration tests before adding the legacy import. The expected red result is no migration result or incorrect mutation of legacy files.

**Implementation:** On first launch only, inspect the documented AstreaOS legacy link if the new Paper record is absent; resolve and validate the target; persist through Paper; never create/repair/delete links and never continuously synchronize. If the implementation cannot safely establish this boundary in the current process layout, leave migration disabled and record compatibility status rather than adding a fragile heuristic.

**Verification:** Run the complete focused test matrix and inspect the legacy paths to confirm no writes. The report must list baseline commits, changed files, tests and outcomes, current-session limitations, and explicit non-claims for Regulus, native lockscreen, multi-output live qualification, and factory-artwork packaging where not verified. Commit boundary: report/migration only.

## Task 10: Final verification and handoff

**Commands:**

- Eclipse configure/build and focused Paper, Shell, and Settings tests.
- Typhon focused CLI tests, formatting, and lint checks where available.
- `git diff --check` in both repositories.
- `rg` checks confirming no new wallpaper QML uses process/shell/filesystem APIs and no new implementation creates `wallpaper.jpg` symlinks.
- Inspect `git status --short` and diffs for accidental edits to pre-existing dirty files.

**Completion criteria:**

- Only Paper owns configured/default/effective wallpaper state in the native Eclipse path.
- Missing configured sources fall back without erasing configuration.
- Set is transactional; reset clears the override.
- Async validation is latest-request-wins with bounded pending work.
- Renderer consumes effective state only and swaps without blanking.
- Settings and `astreactl` use native/control boundaries rather than QML process spawning or Typhon ownership.
- Control endpoint is user-runtime scoped and injection-safe.
- Tests and limitations are reported accurately.

Do not create a commit or PR as part of this plan unless separately requested; leave the user’s existing branch and unrelated changes intact.
