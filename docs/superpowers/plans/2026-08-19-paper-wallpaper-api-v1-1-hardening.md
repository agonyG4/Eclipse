# Astrea Paper Wallpaper API v1.1 Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Paper wallpaper mutations transactionally complete across Eclipse IPC, Settings, and Typhon while adding bounded clients, packaged canonical artwork, and reactive external-source recovery.

**Architecture:** Preserve Paper as the state owner and Eclipse as the renderer. Add a service-level operation/result model, use final-result IPC responses, make Settings and `astreactl` asynchronous/stateless clients, and attach a debounced `QFileSystemWatcher` to Paper for configured external sources.

**Tech Stack:** C++20, Qt 6.8 Core/Network/Gui/Qml/Quick/Test, QLocalSocket/QLocalServer, QFileSystemWatcher, QSaveFile, CMake, Rust/serde, existing CTest and Cargo test targets.

## Global Constraints

- Preserve the current Paper v1 architecture and XDG `AstreaOS/paper.ini` compatibility.
- `ok:true` for a mutation means final transaction success, never queue admission.
- Keep at most one active validation and one latest pending validation.
- Do not move wallpaper ownership into Typhon or Settings.
- Do not use blocking Settings GUI-thread waits or nested event loops.
- Do not use periodic filesystem polling.
- Preserve `0700` runtime directory and `0600` socket security.
- Reuse the existing canonical AstreaOS Sequoia artwork; do not fabricate or regenerate artwork.
- Do not add native lockscreen, per-output policy, dynamic wallpaper, video, or slideshow behavior.
- Never reset, restore, checkout, clean, stash, stage, or commit unrelated dirty work.
- Every Markdown file created by this plan is English.

---

### Task 1: Record current regressions and operation API expectations

**Files:**
- Modify: `Paper/tests/WallpaperServiceTest.cpp`
- Modify: `Paper/tests/WallpaperControlServerTest.cpp`
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`
- Modify: `Typhon/src/astreactl/wallpaper.rs` tests

**Interfaces:**
- Consumes current v1 `WallpaperService`, `WallpaperControlServer`, Settings controller, and Rust Paper socket adapter.
- Produces failing regression cases that define final operation responses, endpoint ownership, async client state, and typed Paper errors.

- [ ] **Step 1: Write the failing tests**

Add tests for immediate-invalid-set response, server B destruction preserving server A, service operation completion/supersede identity, Settings `busy`/nonblocking behavior, and a Rust response with `ok:false`, `completed:true`, and `errorCode:source-missing`.

- [ ] **Step 2: Run the focused tests**

Run the existing Paper, Settings, and Typhon focused tests. Expected pre-fix behavior: the current IPC set test receives `ok:true` before validation, the ownership test exposes a missing endpoint after server B destruction, Settings has no busy property and blocks in its request path, and the Rust typed failure test cannot represent the Paper error.

- [ ] **Step 3: Set the commit boundary**

Do not stage or commit the dirty baseline. Keep these tests as the v1.1 regression boundary for the implementation changes.

### Task 2: Add operation identity and terminal service results

**Files:**
- Modify: `Paper/core/WallpaperService.hpp`
- Modify: `Paper/core/WallpaperService.cpp`
- Modify: `Paper/tests/WallpaperServiceTest.cpp`

**Interfaces:**
- Produces `WallpaperOperationId`, `WallpaperOperationStatus`, `WallpaperOperationResult`, `setWallpaper()`/`resetWallpaper()` IDs, and `wallpaperOperationFinished`.
- Keeps internal validation tokens private and separate from public operation identity.

- [ ] **Step 1: Write failing service assertions**

Assert one success for valid set, one rejection for missing source, one persistence failure with unchanged configured/effective/generation, unique monotonic IDs, explicit supersede for active and pending requests, and reset cancellation of in-flight set.

- [ ] **Step 2: Run `paper-service-test`**

Expected pre-fix result: the service has no operation result signal or public operation IDs, so the new test does not compile or cannot observe terminal outcomes.

- [ ] **Step 3: Implement the minimal result model**

Track active/pending mutation IDs alongside validation tokens. Emit exactly one terminal result per accepted mutation. Mark active/pending operations superseded when a newer mutation replaces them; preserve the base authoritative snapshot for failure/supersede results; publish successful state only after persistence succeeds.

- [ ] **Step 4: Run the service tests**

Run `cmake --build build/no-layer-shell --target paper-service-test -j2` and `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R paper-service-test --output-on-failure`. Expected: all service transaction tests pass.

- [ ] **Step 5: Commit boundary**

If commits are requested later, this task is one reviewable boundary containing only service and service-test files.

### Task 3: Refactor control IPC to final responses and safe ownership

**Files:**
- Modify: `Paper/platform/ipc/WallpaperControlServer.hpp`
- Modify: `Paper/platform/ipc/WallpaperControlServer.cpp`
- Modify: `Paper/tests/WallpaperControlServerTest.cpp`
- Modify: `Paper/CMakeLists.txt` only if a new test source is required

**Interfaces:**
- Consumes `wallpaperOperationFinished`.
- Produces final JSON responses with `completed`, `requestId`, `snapshot`, `errorCode`, and `message`.

- [ ] **Step 1: Write failing control tests**

Test that invalid set returns `ok:false` with `source-missing` and the prior snapshot; valid set response itself is `ready` and effective B; server B cannot remove server A’s endpoint; 16 clients are accepted, the next is rejected, and incomplete clients expire.

- [ ] **Step 2: Run `paper-control-server-test`**

Expected pre-fix result: set returns immediate `ok:true`/loading, server B teardown removes the endpoint, and there is no bounded-client behavior.

- [ ] **Step 3: Implement ownership and bounds**

Add `m_endpointOwned`, set it only after successful listen/stale recovery, remove the endpoint only when owned, cap accepted clients at 16, attach per-client single-shot idle timers, and clean all socket/operation associations on disconnect.

- [ ] **Step 4: Implement final mutation waiting**

Associate a pending operation with a `QPointer<QLocalSocket>`, connect the service completion signal, enforce one response timeout, and send exactly one terminal response. A disconnected client no longer cancels the Paper operation.

- [ ] **Step 5: Run the control tests**

Build and run `paper-control-server-test`; expected: valid/invalid/final-response, ownership, bounds, malformed, oversized, Unicode, spaces, and injection-shaped tests pass.

- [ ] **Step 6: Commit boundary**

Keep the IPC implementation and regression tests together as one reviewable boundary if committing is later requested.

### Task 4: Package the canonical factory wallpaper

**Files:**
- Create: `Paper/assets/default.jpg` by copying the existing canonical AstreaOS Sequoia artwork unchanged
- Modify: `Paper/CMakeLists.txt`
- Modify: `Paper/core/WallpaperResolver.cpp`
- Modify: `Paper/tests/WallpaperResolverTest.cpp`

**Interfaces:**
- Produces packaged resource `qrc:/qt/qml/Astrea/Paper/assets/default.jpg` with stable logical ID `astrea://wallpaper/default`.

- [ ] **Step 1: Write the production-layout failing test**

Unset `ASTREA_ROOT` and `ASTREA_WALLPAPER_DEFAULT`, construct the default resolver without an explicit source, and assert the result is normal default rather than emergency.

- [ ] **Step 2: Run `paper-resolver-test`**

Expected pre-fix result: current Eclipse Paper has only `emergency.svg`, so the resolver reaches `astrea://wallpaper/emergency` unless the developer checkout is available.

- [ ] **Step 3: Copy and package the existing asset**

Copy `/home/agony/.local/share/Astrea/src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg` to `Paper/assets/default.jpg` without modifying its contents, list it in `qt_add_qml_module(RESOURCES)`, and check the packaged resource before compatibility paths.

- [ ] **Step 4: Run resolver/build checks**

Build `paper-resolver-test` and run it with both development override variables unset. Expected: the normal logical default resolves from the packaged resource.

- [ ] **Step 5: Commit boundary**

If committing later, keep the binary asset, CMake resource declaration, resolver, and resolver test together.

### Task 5: Add Paper-owned reactive source watching

**Files:**
- Create: `Paper/core/WallpaperSourceWatcher.hpp`
- Create: `Paper/core/WallpaperSourceWatcher.cpp`
- Modify: `Paper/core/WallpaperService.hpp`
- Modify: `Paper/core/WallpaperService.cpp`
- Modify: `Paper/CMakeLists.txt`
- Modify: `Paper/tests/WallpaperServiceTest.cpp`

**Interfaces:**
- Produces a watcher that reports debounced configured-source changes to `WallpaperService`.
- Uses `QFileSystemWatcher` on canonical file and parent directory, with no periodic timer.

- [ ] **Step 1: Write failing watcher tests**

Test active-file deletion, recreation, valid atomic replacement, corrupt replacement, valid restoration, configured-intent retention, and bounded validation work.

- [ ] **Step 2: Run `paper-service-test`**

Expected pre-fix result: deletion/recreation and atomic replacement do not update the service without an explicit `reload()` call.

- [ ] **Step 3: Implement watcher and reconciliation**

Attach/detach watches with configured state, debounce event bursts with a single-shot timer, queue an internal reconciliation validation, publish fallback without clearing configured state, and restore configured effective state when validation succeeds. A valid same-path replacement increments generation and emits an effective reload notification.

- [ ] **Step 4: Run watcher tests**

Run the service test repeatedly enough to cover file notification ordering; assert no more than one active and one pending validation and no persistent debounce timer after events settle.

- [ ] **Step 5: Commit boundary**

Keep watcher code and service regression tests as one boundary if commits are later requested.

### Task 6: Preserve renderer two-slot behavior while supporting generation reload

**Files:**
- Modify: `Paper/platform/wayland/WallpaperSurfaceBundle.cpp`
- Modify: `Paper/qml/WallpaperSurface.qml`
- Modify: `Paper/tests/WallpaperSurfaceManagerTest.cpp`

**Interfaces:**
- Produces a generation/reload property consumed by the existing two-slot QML renderer.

- [ ] **Step 1: Write failing generation-reload test**

Assert that a same-path effective-source refresh carries a new generation to the bundle/renderer without changing the two-slot no-blank-frame behavior.

- [ ] **Step 2: Run the surface test**

Expected pre-fix result: the bundle forwards only source and fit, so an unchanged path cannot force QML to reload after atomic replacement.

- [ ] **Step 3: Add the minimal reload identity**

Forward snapshot generation as a QML property and trigger the existing hidden-slot load path on generation changes. Do not replace the current front/back swap model.

- [ ] **Step 4: Run offscreen surface tests**

Run `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R paper-surface-manager-test --output-on-failure`.

- [ ] **Step 5: Commit boundary**

Keep renderer-only changes separate from the service/IPC boundaries if committing later.

### Task 7: Replace Settings blocking calls with an async client

**Files:**
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.cpp`
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml`

**Interfaces:**
- Produces `busy`, `pendingAction`, asynchronous `refresh/set/reset/default`, request-generation protection, and final snapshot/error signals.

- [ ] **Step 1: Write failing async client tests**

Use a local fake Paper server that delays its response. Assert invocations return before the delayed response, `busy` transitions true/false, valid set publishes final B, invalid set preserves A and exposes typed error, service unavailable returns to the event loop, and stale replies cannot overwrite newer state.

- [ ] **Step 2: Run Settings tests**

Expected pre-fix result: the controller has no busy state and blocks in `waitForConnected`, `waitForBytesWritten`, and nested `QEventLoop`.

- [ ] **Step 3: Implement asynchronous framing**

Use `QLocalSocket` signal handlers, bounded line buffers, a single-shot timeout, and request IDs. Apply only the matching response; keep the controller a projection/client.

- [ ] **Step 4: Update QML state**

Disable Apply/Reset while busy, display pending and final states/errors, and keep preview bound to the controller snapshot without adding state variables for authority.

- [ ] **Step 5: Run Settings build/tests**

Build `astrea-settings`, then run the controller, QML smoke, and structure tests. Expected: all async state tests pass without GUI-thread waits.

- [ ] **Step 6: Commit boundary**

Keep Settings client/QML/tests together as one reviewable boundary if committing later.

### Task 8: Make `astreactl` consume final typed Paper results

**Files:**
- Modify: `Typhon/src/astreactl/wallpaper.rs`
- Modify: `Typhon/src/astreactl/output.rs` if human error rendering needs the existing convention
- Modify: `Typhon/src/bin/astreactl.rs` tests
- Modify: `Typhon/src/control_snapshots.rs` only if response/result types need a typed status field

**Interfaces:**
- Produces final-result-only `AstreactlResult::Wallpaper` success and typed `AstreactlError::Server`/Paper error mapping.

- [ ] **Step 1: Write failing Rust tests**

Feed valid final, invalid `source-missing`, and `superseded` responses to the local Unix listener. Assert success only for valid final completion, non-zero typed errors for failures, and preserved Unicode/spaces.

- [ ] **Step 2: Run focused Cargo tests**

Expected pre-fix result: a loading response with `accepted:true` is accepted as success and a Paper failure becomes generic `Internal`.

- [ ] **Step 3: Implement final-response decoding**

Require `completed:true`, preserve `requestId`, map `errorCode`/`message` into a typed control error without moving state into Typhon, and retain bounded read behavior.

- [ ] **Step 4: Run formatting and focused tests**

Run `cargo fmt --check`, `cargo test --lib astreactl::wallpaper --no-default-features`, and `cargo test --bin astreactl --no-default-features`.

- [ ] **Step 5: Commit boundary**

Keep only the Typhon Paper adapter/result changes in this boundary if committing later.

### Task 9: Run cross-repo verification and native qualification attempt

**Files:**
- Modify: `docs/superpowers/specs/2026-08-19-paper-wallpaper-api-v1-1-hardening-design.md` only for verified corrections
- Create: `REPORT-2026-08-19-paper-wallpaper-api-v1-1-hardening.md`

**Interfaces:**
- Produces evidence separating offscreen/unit results, broader-suite failures, factory-resource proof, and live native qualification.

- [ ] **Step 1: Run native build and focused CTest**

Run the existing Eclipse build targets and the Paper/Settings/Shell focused CTest regex with `QT_QPA_PLATFORM=offscreen` where required.

- [ ] **Step 2: Run Typhon formatting/tests**

Run Cargo formatting, focused wallpaper tests, CLI tests, and the broader relevant astreactl suite; report unchanged failures exactly rather than dismissing new failures.

- [ ] **Step 3: Attempt the normal Eclipse + Typhon launch workflow**

If a usable session is available, exercise startup, Settings valid/invalid/reset, `astreactl`, deletion/restoration, atomic replacement, rapid A/B/C, screen lifecycle, and idle completion. If unavailable, record the concrete blocker and do not claim native success.

- [ ] **Step 4: Write the final report**

Include baseline HEAD/status, v1 preservation, operation/result behavior, endpoint bounds/ownership, async clients, packaging source, watcher behavior, renderer/global qualification, idle behavior, native status, validation commands, blockers, commits, and final status for both repositories.

- [ ] **Step 5: Final checks**

Run `git diff --check` in both repositories and scan all new Markdown for unfinished markers or placeholder text before reporting.
