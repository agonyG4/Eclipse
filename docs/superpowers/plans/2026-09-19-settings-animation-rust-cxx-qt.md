# Settings Animations Rust/CXX-Qt Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Replace the production Settings Animations/Typhon C++ backend with a behavior-compatible Rust backend exposed through a generated CXX-Qt QObject.

**Architecture:** `SettingsController` continues to own the generated `SettingsAnimationController` QObject. CXX-Qt generates the Qt boundary from `Settings/backend/src/animation/qobject.rs`; typed Rust state, protocol, secure discovery, and one bounded worker own backend semantics. `Animations.qml` and its `SettingsController.animations` API remain unchanged.

**Tech Stack:** Rust 2024, serde/serde_json, CXX-Qt 0.10.0, CXX-Qt-CMake 0.10.0, Qt 6.8+, CMake/Corrosion, C++20, Qt Test, CTest, Ninja.

## Global Constraints

- Work directly on `main`; do not create a branch or worktree.
- Keep build and Cargo target output inside the repository/build tree.
- Do not use sub-agents or Python.
- Commit `Settings/backend/Cargo.lock`; use CXX-Qt 0.10.0.
- Keep JSON at the Typhon wire boundary only; never use `serde_json::Value` as the animation domain model.
- Preserve one in-flight request, 64 KiB requests, 1 MiB responses, 2000 ms default deadline, monotonic deadlines, strict validation, and secure endpoint discovery.
- Never block the Qt thread on socket I/O; never create a thread per request or slider event.
- Make late completion and QObject shutdown safe without `unwrap()`, `expect()`, or panic in production boundary code.
- Do not change `Animations.qml` behavior or migrate another Settings/Eclipse backend.

---

### Task 1: Baseline and toolchain

**Files:** Read `CMakePresets.json`, `Settings/AGENTS.md`, the existing animation test, Spotlight Rust/CMake files, and `tools/ci/run-rust-gate.sh`.

- [ ] Confirm `git status --short --branch`, active branch `main`, and repository root.
- [ ] Record `rustc --version`, `cargo --version`, `cmake --version`, `ninja --version`, `qmake6 -query QT_VERSION` (or qmake fallback), and the CMake Qt installation path.
- [ ] Run `cmake --preset debug` before changes. Record exact missing dependency/runtime failures if configuration is unavailable.
- [ ] Keep this baseline in the final report; no source commit is needed.

### Task 2: Typed animation state crate slice

**Files:**
- Create `Settings/backend/Cargo.toml`, `build.rs`, `src/lib.rs`, `src/animation/mod.rs`, `src/animation/state.rs`, and `src/typhon/mod.rs`.
- Test in `Settings/backend/src/animation/state.rs`.

**Produces:** `AnimationConfiguration`, `AnimationSnapshot`, catalog/effect/slot types, `AnimationMutation`, `AnimationState`, and typed mutation/projection errors.

- [ ] Write failing tests named `defaults_are_enabled_astrea_speed_one_and_have_no_overrides`, `speed_is_ignored_when_non_finite_and_clamped_to_supported_range`, `planned_and_unavailable_effects_are_projected_separately`, `unavailable_effect_selection_returns_the_existing_user_error`, `pending_speed_is_folded_into_the_next_mutation`, and `snapshot_without_config_is_rejected_without_replacing_authoritative_state`. Use typed fixtures, not JSON.
- [ ] Verify RED with `rtk cargo test --manifest-path Settings/backend/Cargo.toml --locked`; fix only harness errors.
- [ ] Add the Rust 2024 library manifest with `cxx-qt`, `cxx-qt-lib`, `serde` derive, `serde_json`, and locked CXX-Qt build dependency versions `0.10.0`. Include the `links` metadata required by CXX-Qt export.
- [ ] Implement typed defaults (`enabled=true`, `preset="astrea"`, `speed=1.0`, empty overrides), finite speed clamp `0.5..=2.0`, authoritative snapshot replacement only when `config` exists, typed overrides, and capability buckets `availableEffects`, `plannedEffects`, and `unavailableEffects`.
- [ ] Verify GREEN with `rtk cargo test --manifest-path Settings/backend/Cargo.toml --locked animation::state`.
- [ ] Run `cargo fmt --manifest-path Settings/backend/Cargo.toml -- --check` and `rtk cargo clippy --manifest-path Settings/backend/Cargo.toml --locked --all-targets -- -D warnings`, then commit `feat: add typed settings animation state`.

### Task 3: Typed Typhon protocol

**Files:**
- Create `Settings/backend/src/typhon/protocol.rs`.
- Modify `Settings/backend/src/typhon/mod.rs`.
- Test in `protocol.rs`.

**Produces:** typed `AnimationRequest`, `AnimationResponse`, `ProtocolError`, `encode_request`, and `decode_response`.

- [ ] Write failing tests for request protocol/version/newline, 64 KiB request rejection, invalid JSON, protocol/version/id/ok validation, required object result/error, valid server rejection distinction, trailing data/frame rejection, and 1 MiB response rejection.
- [ ] Verify RED with `rtk cargo test --manifest-path Settings/backend/Cargo.toml --locked typhon::protocol`.
- [ ] Implement typed serde request/response structures. Preserve top-level `astrea.control`, version `1`, integral matching `u64` IDs, boolean `ok`, object success `result`, object failure `error`, exact one-frame behavior, and bounded sizes. Keep server rejection as a distinct outcome with its message.
- [ ] Verify GREEN with the same focused test command, format, and commit `feat: add typed Typhon animation protocol`.

### Task 4: Secure endpoint discovery

**Files:**
- Create `Settings/backend/src/typhon/discovery.rs`.
- Modify `Settings/backend/src/typhon/mod.rs`.
- Test in `discovery.rs`.

**Produces:** `discover_socket()`, `valid_instance_name()`, and testable secure metadata helpers.

- [ ] Write failing tests for missing/non-absolute/insecure `XDG_RUNTIME_DIR`, insecure Astrea/Typhon directories, wrong owner/mode/type, exact secure `WAYLAND_DISPLAY` preference, ambiguous instances, invalid/overlong names, and symlink rejection using `lstat` semantics.
- [ ] Verify RED with `rtk cargo test --manifest-path Settings/backend/Cargo.toml --locked typhon::discovery`.
- [ ] Implement effective-UID checks, exact `0700` directories, exact `0600` sockets, socket type checks, bounds/name validation, sorted fallback scanning, exact-display preference, and unique-candidate requirement. Protect environment-mutating tests with a process mutex.
- [ ] Verify GREEN, run fmt/clippy, and commit `feat: secure Typhon endpoint discovery in Rust`.

### Task 5: Bounded asynchronous client

**Files:**
- Create `Settings/backend/src/typhon/client.rs`.
- Modify `Settings/backend/Cargo.toml` and `src/typhon/mod.rs`.
- Test in `client.rs`.

**Produces:** `TyphonClient`, typed `ClientRequest`/`ClientOutcome`, and one bounded worker.

- [ ] Write failing Unix-socket fixture tests for off-caller-thread completion, bounded timeout, second-request rejection, connection failure, bounded worker submission/no thread-per-request, and safe drop while I/O is outstanding.
- [ ] Verify RED with `rtk cargo test --manifest-path Settings/backend/Cargo.toml --locked typhon::client`.
- [ ] Add only the minimal Unix socket dependency needed for deadline-bounded nonblocking connect. Perform discovery/connect/write/read off the Qt thread, use `Instant` remaining time, cap response bytes at 1 MiB, and reject trailing data.
- [ ] Implement a single worker with `sync_channel(1)` and a shared debounce deadline/condition state. No request or slider edit may call `spawn`; worker shutdown must end cleanly after the bounded socket deadline.
- [ ] Verify GREEN, format, clippy, and commit `feat: add bounded asynchronous Typhon client`.

### Task 6: Thin CXX-Qt QObject bridge

**Files:** Create/modify `Settings/backend/src/animation/qobject.rs`. Modify `Settings/backend/src/lib.rs`, `Settings/backend/build.rs`, and `Settings/backend/Cargo.toml`. Test Rust operation-token/state callbacks and generated bridge compilation.

**Produces:** generated `SettingsAnimationController` with the exact existing properties, grouped notify signals, and invokable names.

- [ ] Write failing tests named `stale_completion_is_ignored_when_operation_token_is_old`, `server_rejection_keeps_available_true_but_transport_failure_clears_it`, and `speed_debounce_has_one_pending_deadline_for_repeated_edits`; add the CXX-Qt declaration before implementations.
- [ ] Verify RED with `rtk cargo check --manifest-path Settings/backend/Cargo.toml --locked`.
- [ ] Declare CXX-Qt aliases for `QString`, `QVariant`, `QList<QVariant>`, and `QMap<QString,QVariant>`. Define custom-read/custom-notify properties `available`, `busy`, `enabled`, `preset`, `speed`, `generation`, `source`, `hasOverrides`, `slots`, `presets`, `lastError`, signals `availabilityChanged`, `busyChanged`, `snapshotChanged`, `errorChanged`, and all nine existing invokables.
- [ ] Implement projection only in the bridge. Keep mutations typed, use the single worker and operation tokens, preserve 80 ms debounce/folding, and handle every `CxxQtThread::queue` result without panic.
- [ ] Implement `build.rs` with `CxxQtBuilder::new().qt_module("Network").files(["src/animation/qobject.rs"]).build();`; do not define a QML module.
- [ ] Verify with `cargo fmt -- --check`, `rtk cargo test --locked`, and `rtk cargo check --locked`; commit `feat: expose settings animations through CXX-Qt`.

### Task 7: CMake and composition-root integration

**Files:** Create `Settings/cmake/RustBackend.cmake`. Modify `Settings/CMakeLists.txt`, `Settings/core/CMakeLists.txt`, `Settings/core/SettingsController.hpp`, `Settings/core/SettingsController.cpp`, and static structure checks. Delete the superseded four files under `Settings/services/animation`.

- [ ] Add a failing static assertion requiring the Rust manifest/CMake helper/generated-header include and rejecting the four deleted production paths.
- [ ] In `RustBackend.cmake`, use official CXX-Qt-CMake tag `0.10.0` via `find_package(CxxQt QUIET)` plus FetchContent fallback, then call `cxx_qt_import_crate` with `MANIFEST_PATH`, `CRATES astrea_settings_backend`, `LOCKED`, `QT_MODULES Qt6::Core Qt6::Network`, and explicit `QMAKE`.
- [ ] Resolve `Qt6::qmake`/`Qt::qmake`, otherwise `qmake6` then `qmake`; execute `-query QT_VERSION` and fail when missing or below Qt 6.8. Pass the verified executable to CXX-Qt.
- [ ] Import the crate before `add_subdirectory(core)`, keep generated/Cargo artifacts in the active CMake build tree, remove the old four sources, link `astrea_settings_backend` to `astrea-settings-core`, and include the generated header directory transitively.
- [ ] Change only the C++ include to the generated `.cxxqt.h`; retain `SettingsController.animations` and its type/name.
- [ ] Verify with `cmake --preset debug` and `cmake --build build/debug --target astrea-settings-core --parallel`; commit `build: integrate Rust settings animation backend`.

### Task 8: Qt test migration and lifetime coverage

**Files:** Modify `Settings/tests/unit/SettingsAnimationControllerTest.cpp` and `Settings/tests/CMakeLists.txt`.

- [ ] Remove the direct client include/tests; retain controller tests and the in-process control server. Rust protocol/discovery/client tests replace cases that no longer need Qt.
- [ ] Add `generatedQObjectDoesNotBlockDuringTimeout`, `staleCompletionDoesNotMutateAReusedController`, and `destroyingControllerWithOutstandingRequestIsSafe` using delayed responses, event-loop markers, scoped destruction, and operation-token assertions.
- [ ] Build/run `settings-animation-controller-test` and verify authoritative snapshots, planned/unavailable rejection, server rejection availability, event-loop responsiveness, and pending-speed folding.
- [ ] Commit `test: cover Rust-backed settings animation QObject`.

### Task 9: CI, static checks, and documentation

**Files:** Modify `tools/ci/run-rust-gate.sh`, `Settings/tests/static/SettingsStructureTest.cmake`, `Settings/README.md`, `Settings/docs/ARCHITECTURE.md`, `Settings/docs/MIGRATION_NOTES.md`, `Settings/docs/STRUCTURE.md`, and `Settings/docs/TESTING.md`.

- [ ] Extend the Rust gate to run locked fmt, clippy `--all-targets -- -D warnings`, and tests for both Spotlight and Settings manifests; retain Spotlight behavior.
- [ ] Assert the Rust backend/CMake files exist, the generated target is referenced, and the superseded C++ implementation is not in production or tests.
- [ ] Replace false “no Typhon IPC boundary” claims with the public Astrea control-socket boundary used by Animations. Document exactly: QML/Qt Quick presentation, CXX-Qt thin QObject, Rust backend/domain/system logic, and C++ composition/Qt-specific code during incremental migration. Keep Compositor preview-only and other migrations deferred.
- [ ] Run `git diff --check` and `tools/ci/run-rust-gate.sh`; commit `docs: document incremental Rust settings backend migration`.

### Task 10: Full verification and deliberate review

- [ ] Run `tools/ci/run-rust-gate.sh`.
- [ ] Run `cmake --preset debug` and `cmake --build build/debug --parallel`.
- [ ] Run focused Settings CTest tests, full `ctest --test-dir build/debug --output-on-failure`, and `tools/ci/run-qml-gate.sh`.
- [ ] Run ASan and UBSan configure/build/CTest presets when available; record exact failures rather than treating unavailable checks as success.
- [ ] Review worker→CXX-Qt→QObject lifetime, every external-data bound, lstat/ownership/modes, panic paths, stale tokens, GUI-thread I/O, debounce boundedness, Qt conversion/cloning, old C++ removal, CMake/CI/static/docs assumptions.
- [ ] Run `git diff --check`, `git status --short`, and `git diff --stat`; commit verified review fixes. Final report must list files, CXX-Qt version/integration, preserved behavior, tests, every command/result, sanitizer status, necessary remaining C++, and deferred work.
