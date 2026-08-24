# Astrea Paper Wallpaper API v1.1.1 Renderer Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make same-path image revisions render new pixels, project authoritative fit state to Settings, provide a shared physical factory asset, separate transport and operation deadlines, and detect symlink retargets without changing Paper ownership.

**Architecture:** Keep `WallpaperService` authoritative and keep the existing two-slot QML renderer. Add a generation-specific renderer cache key and per-slot generation guards, add typed fit projection to the existing asynchronous Settings client, resolve the factory default from shared physical data, use one documented C++ deadline contract, and extend the existing event-driven watcher to configured and resolved paths.

**Tech Stack:** C++20, Qt 6.8 Core/Gui/Network/Qml/Quick/Test, QML `Image`, QFileSystemWatcher, CMake/GNUInstallDirs, Rust/serde, existing CTest and Cargo targets.

## Global Constraints

- Preserve Paper as the wallpaper authority and keep Typhon stateless.
- Preserve configured/default/effective separation, global scope, emergency fallback, terminal v1.1 operation results, latest-request-wins, bounded validation, secure IPC, and endpoint ownership.
- Preserve the front/back image-slot strategy and `Image.cache: true` for unchanged generations.
- Use the five current fit values: `cover`, `contain`, `stretch`, `center`, and `tile`.
- Keep the normal factory identity `astrea://wallpaper/default` separate from `astrea://wallpaper/emergency`.
- Use an installed physical factory asset under `${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers/` and a source-tree development fallback.
- Separate 1000 ms transport setup/write, 5000 ms Paper operation, and 1000 ms client completion margin deadlines.
- Watch filesystem events; do not add periodic polling or unbounded watch accumulation.
- Preserve unrelated dirty work and do not reset, restore, checkout, clean, stash, stage, or commit.
- Write all Markdown in English.

---

### Task 1: Add failing same-path renderer and stale-completion tests

**Files:**
- Modify: `Paper/qml/WallpaperSurface.qml`
- Modify: `Paper/tests/WallpaperSurfaceManagerTest.cpp`
- Modify: `Paper/CMakeLists.txt` only if the existing surface target needs an explicit Qt Quick test dependency

**Interfaces:**
- Consumes the existing `wallpaperSource`, `wallpaperGeneration`, `WallpaperSurfaceBundle`, and offscreen QML test target.
- Produces test-visible slot generation, load counters, and visible generation behavior.

- [ ] **Step 1: Write the failing QML regression**

Load revision 1 into a temporary file, create the QML surface with generation 1, and assert the surface reaches visible generation 1. Atomically replace the exact path with revision 2, set generation 2 without changing the source path, and assert the load-start count increases, the inactive slot source contains a different `astreaGeneration=2` key, and visible generation becomes 2. Repeat revisions 3 and 4 rapidly and assert the final visible generation is 4.

- [ ] **Step 2: Run the surface test to verify the failure**

Run `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R paper-surface-manager-test --output-on-failure`. Expected pre-fix failure: the QML surface has no generation-specific source/load instrumentation and assigning the same URL cannot prove a new decode.

- [ ] **Step 3: Add generation-aware slot assertions**

Add `requestedGeneration`, `requestedSource`, and a per-slot requested renderer URL. Keep the newest request pending if the inactive slot is still loading. On Ready, promote only if the slot source/generation matches the current request; otherwise start the newest pending load. Expose disabled-by-default counters and slot/visible generation properties for the test.

- [ ] **Step 4: Run the renderer test to verify it passes**

Rebuild the surface target and rerun the focused CTest. Expected: same-path revisions cause distinct load URLs and only the newest generation becomes visible.

- [ ] **Step 5: Commit boundary**

Do not commit; preserve the dirty worktree. If a later commit is explicitly requested, this boundary contains only QML renderer and renderer-test changes.

### Task 2: Add strict fit conversion and Settings projection tests

**Files:**
- Modify: `Paper/core/WallpaperDescriptor.hpp`
- Modify: `Paper/core/WallpaperDescriptor.cpp`
- Modify: `Paper/tests/WallpaperDescriptorTest.cpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.cpp`
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml`

**Interfaces:**
- Produces a strict Paper fit conversion for API validation and `configuredFit`/`effectiveFit` controller properties.
- Keeps `wallpaperFitFromString` compatibility behavior for legacy reads while rejecting unknown client values before mutation.

- [ ] **Step 1: Write failing fit tests**

Add a strict conversion test for all five values and an unknown value. Extend the fake Settings response snapshot with descriptor fits, then assert `configuredFit` is `contain` while `effectiveFit` is `cover` during fallback. Add a response test for each fit and assert the controller exposes the exact string. Add a test that an unsupported Settings fit does not send a socket request.

- [ ] **Step 2: Run the focused tests to verify failure**

Run `cmake --build build/no-layer-shell --target paper-descriptor-test settings-wallpaper-controller-test -j2` followed by the two test executables through CTest. Expected pre-fix failure: no strict conversion or fit properties exist and Settings does not project descriptor fit.

- [ ] **Step 3: Implement strict conversion and projection**

Add `wallpaperFitFromStringStrict` returning `std::optional<WallpaperFit>`. Use it in API entry points that accept external strings. Parse and validate configured/effective descriptor `fit` in `applyResponse`; reject malformed snapshots. Add controller properties and synchronize QML's selected index from configured fit when present, otherwise effective fit. Disable the source field and selector while busy and preserve the selected fit when only the source changes.

- [ ] **Step 4: Run fit tests and QML checks**

Run the descriptor, Settings controller, QML smoke, and Settings structure tests. Expected: all five modes round-trip and external Paper fit state cannot be overwritten by a stale Cover default.

- [ ] **Step 5: Commit boundary**

Do not commit. If later requested, keep Paper conversion, Settings controller, QML, and their tests in one fit-authority boundary.

### Task 3: Install and resolve a cross-process factory asset

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `Paper/CMakeLists.txt`
- Modify: `Paper/core/WallpaperResolver.cpp`
- Modify: `Paper/tests/WallpaperResolverTest.cpp`
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`
- Modify: `Settings/tests/CMakeLists.txt` if the test needs a source-directory compile definition

**Interfaces:**
- Produces installed `${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers/default.jpg` and a resolver result with logical ID `astrea://wallpaper/default` plus a readable physical path.
- Keeps embedded emergency fallback independent and leaves the normal resource bundle out of the cross-process factory selection.

- [ ] **Step 1: Write failing production-like asset tests**

Unset `ASTREA_ROOT` and `ASTREA_WALLPAPER_DEFAULT`, resolve the factory default, assert its resolved source is physical and readable, and assert it is not a `qrc:` URL. Feed that physical source into the Settings preview fixture and use `QImageReader` to confirm a consumer can read it. Assert a missing shared/source asset still resolves the independent emergency resource.

- [ ] **Step 2: Run resolver and Settings tests to verify failure**

Run the resolver and Settings controller tests. Expected pre-fix failure: the resolver selects the shell-local QML resource and the cross-process preview fixture cannot consume that source.

- [ ] **Step 3: Add the shared install and development candidates**

Include `GNUInstallDirs`, install `Paper/assets/default.jpg` to `${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers`, define the Paper source directory for its development-only resolver fallback, and order candidates as explicit override, environment override, installed XDG data, source-tree asset, legacy `ASTREA_ROOT`, then emergency. Keep the descriptor's logical ID stable.

- [ ] **Step 4: Run resource and preview verification**

Build the resolver and Settings targets, run the focused tests, and inspect the installed staging path with `cmake --install` into a temporary prefix if the build configuration supports it. Expected: both processes receive a physical, readable factory source without `ASTREA_ROOT`.

- [ ] **Step 5: Commit boundary**

Do not commit. If later requested, the boundary contains install metadata, resolver selection, and cross-process asset tests.

### Task 4: Separate transport and Paper operation deadlines

**Files:**
- Create: `shared/platform/paper/PaperProtocol.hpp`
- Modify: `Paper/core/WallpaperService.hpp`
- Modify: `Paper/core/WallpaperService.cpp`
- Modify: `Paper/platform/ipc/WallpaperControlServer.hpp`
- Modify: `Paper/platform/ipc/WallpaperControlServer.cpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.cpp`
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`
- Modify: `Typhon/src/astreactl/wallpaper.rs`
- Modify: `Typhon/src/bin/astreactl.rs`

**Interfaces:**
- Produces documented 1000 ms transport, 5000 ms Paper operation, and 1000 ms client margin constants.
- Adds terminal `timed-out` Paper result semantics and makes Settings/`astreactl` wait through the Paper operation plus margin.

- [ ] **Step 1: Write failing delayed-operation tests**

Make the fake Settings socket delay a final response for 1500 ms and assert the controller remains busy until the response and then succeeds. Add a Rust fake Paper listener with the same delay and assert `wallpaper::request` succeeds with a 6-second default-equivalent timeout. Add tests that a response after the chosen operation deadline produces a timeout and no late success is accepted. Add a Paper service result serialization assertion for `timed-out`.

- [ ] **Step 2: Run delayed tests to verify failure**

Run the Settings controller test and focused Rust wallpaper tests. Expected pre-fix failure: Settings times out at 1000 ms and `astreactl` defaults to 2000 ms.

- [ ] **Step 3: Implement the deadline contract**

Add the shared C++ constants and a `TimedOut` operation status. Start a service-owned deadline for accepted mutation operations; on expiry restore the authoritative base, emit one terminal timeout result, invalidate the late worker token, and ignore its eventual completion. Keep the server's client wait longer than the Paper deadline and preserve endpoint/idle bounds. In Settings, use the transport timer through connect/write and then a 6000 ms completion timer. In Rust, keep the global user timeout override, default wallpaper mutations to 6000 ms, use a 1000 ms write deadline, and use the operation timeout for final response reads.

- [ ] **Step 4: Run timeout verification**

Run the Paper service/IPC tests, Settings controller test, focused Rust wallpaper tests, `cargo fmt --check`, and `cargo check --bin astreactl`. Expected: delayed completion before the Paper deadline succeeds; Paper timeout is terminal and late worker output cannot publish success.

- [ ] **Step 5: Commit boundary**

Do not commit. If later requested, keep shared deadline definitions, Paper timeout behavior, and both clients together.

### Task 5: Extend the watcher for symlink retargeting and bounded paths

**Files:**
- Modify: `Paper/core/WallpaperSourceWatcher.hpp`
- Modify: `Paper/core/WallpaperSourceWatcher.cpp`
- Modify: `Paper/tests/WallpaperServiceTest.cpp`

**Interfaces:**
- Produces configured-entry, configured-parent, resolved-target, and resolved-parent monitoring with deduplication and a test-visible bounded watch count.

- [ ] **Step 1: Write failing symlink tests**

Set a symlink path to revision A, wait for Ready, retarget the same symlink path to revision B, and assert configured source remains the symlink while effective source becomes B and generation advances. Repeatedly retarget A/B/C and assert the watch count remains bounded.

- [ ] **Step 2: Run the watcher test to verify failure**

Run `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R paper-service-test --output-on-failure`. Expected pre-fix failure: only the canonical target is watched, so a symlink retarget may not trigger reconciliation.

- [ ] **Step 3: Implement the minimal watch-set lifecycle**

Track configured and resolved local paths separately. Rebuild a deduplicated set containing each existing entry/target and each parent directory, remove all obsolete paths before adding the new set, and keep the existing 75 ms event debounce. Leave resource/non-file descriptors unwatched.

- [ ] **Step 4: Run watcher and service regressions**

Run the service test repeatedly and the full Paper-focused CTest regex. Expected: symlink retarget, in-place replacement, atomic replacement, corrupt fallback, and restoration all reconcile without polling or unbounded watches.

- [ ] **Step 5: Commit boundary**

Do not commit. If later requested, keep watcher and service regression changes together.

### Task 6: Run end-to-end closure verification and native qualification

**Files:**
- Modify: `Paper/tests/WallpaperSurfaceManagerTest.cpp` if the integrated watcher-to-renderer test needs a shared fixture
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp` if the factory preview requires a QML-level assertion
- Create: `REPORT-2026-08-19-paper-wallpaper-api-v1-1-1-renderer-integration.md`

**Interfaces:**
- Produces evidence for renderer loads, fit authority, shared factory preview, timeout behavior, symlink retarget, fallback/recovery, rapid switching, and native qualification.

- [ ] **Step 1: Run fresh Eclipse verification**

Build `astrea-shell` and `astrea-settings` in the existing no-layer-shell build, run all Paper/Settings/Shell wallpaper-related tests with `QT_QPA_PLATFORM=offscreen`, run configured QML/static checks, and run `git diff --check`.

- [ ] **Step 2: Run fresh Typhon verification**

Run `cargo fmt --check`, focused wallpaper tests, `cargo test --bin astreactl`, and `cargo check --bin astreactl`. Record the broader suite's baseline failures separately.

- [ ] **Step 3: Attempt native qualification**

On the available Wayland/Typhon session, exercise factory startup, Settings preview, all fit modes, external CLI fit projection, same-path replacement, atomic replacement, corrupt/fallback/restore, rapid switching, and symlink retarget if supported. If the session is unavailable or the shell exits before its endpoint is usable, record the exact blocker and do not claim native proof.

- [ ] **Step 4: Write and self-review the final report**

Record baseline status, root causes, cache behavior, renderer/stale-load evidence, fit roundtrip, physical factory preview, timeout results, symlink behavior, end-to-end replacement/recovery, rapid switching, native status, performance/idle behavior, validation commands, blockers, commits, and final Eclipse/Typhon status. Scan the report and new design/plan for unfinished markers and contradictions.

- [ ] **Step 5: Commit boundary**

Do not commit or stage. The final boundary is the report and verified working-tree changes only.

