# Bluetooth Phase 1 Correctness Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct worker generation ordering, per-device D-Bus serialization, queue-admission identity, owner lookup classification, and production worker destruction without expanding Bluetooth scope.

**Architecture:** Keep `BluetoothCore` authoritative for generation, operation identity, and public snapshots. Add worker-owned scheduling state around the existing bounded action queue and task set, preserve per-device concurrency isolation, and keep fake transport/session seams in the worker module for deterministic integration tests.

**Tech Stack:** Rust 2024, async-channel, futures/smol, zbus 5, CXX-Qt bridge, Cargo/CMake test gates.

## Global Constraints

- Do not add pairing, Agent1, Trust, Forget Device, a Settings Bluetooth page, new user-visible Bluetooth functionality, or migrate `BluetoothDeviceModel`.
- Build artifacts, Cargo target output, CMake builds, caches, and temporary compilation output must be under `/mnt/Aether/Desktop/GitHub`.
- Preserve bounded command/action queues, reconnect delays, signal recovery, scan-owner mailbox behavior, health notifications, generation validation, and the removal of the C++ BlueZ authority.
- Do not join the production Bluetooth worker from QObject destruction; deterministic test harnesses may join.

---

### Task 1: Add red worker regressions and test seams

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs` test module
- Modify: `shared/backend/src/bluetooth/engine.rs` tests only if an operation-state assertion cannot be expressed through the worker

**Interfaces:**
- Consumes: current `WorkerHarness`, `FakeTransport`, `FakeSession`, `CoreAction`, and `CoreSnapshot`.
- Produces: fake owner-change signals, controllable Connect/Disconnect completion gates, per-session dispatch logs, and failing tests for the required worker behavior.

- [ ] **Step 1: Extend the fake session seam.**

  Replace the one-shot signal deque with a bounded signal channel that can receive `NameOwnerChanged` messages after startup. Add a fake owner lookup result, a managed-object fixture containing one powered adapter and configurable paired devices, and an operation tracker that records object path/target/operation ID, active count, maximum active count, and completion senders.

- [ ] **Step 2: Add the failing generation-order tests.**

  Add tests that start with owner `:1.42`, hold an old Connect future, send `NameOwnerChanged(:1.42 -> :1.84)`, and assert the replacement Probe is dispatched and reaches `Ready` without completing the old future. Add disappearance (`:1.84 -> ""`) with no Probe and subsequent appearance with a fresh Probe reaching `Ready`.

- [ ] **Step 3: Add failing per-device scheduling tests.**

  Add worker tests for Connect→Disconnect, Disconnect→Connect, three-operation latest-intent coalescing, concurrent different devices, stale old completion, and authoritative `Connected` convergence. Assert dispatch logs and active/max-active counts, never only core completion return values.

- [ ] **Step 4: Add failing queue and owner-lifecycle tests.**

  Add a queue-full Connect test that creates a stale replacement pair and verifies stale rejection cannot settle the newer identity, plus a current rejection test for the bounded-queue error. Add owner lookup tests for `NameHasNoOwner`, lookup failure, success, and failure-then-retry. Add a structural assertion that the production Drop implementation contains no `JoinHandle::join` call while the harness still joins.

- [ ] **Step 5: Run the focused tests and verify they fail for the current defects.**

  Run:

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth::bluez::client::worker_lifecycle_tests
  ```

  Expected: the new tests fail because the current worker clears the replacement Probe, dispatches same-device calls concurrently, erases owner lookup errors, and joins production workers.

### Task 2: Implement generation ownership, per-device scheduling, and queue settlement

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Modify: `shared/backend/src/bluetooth/engine.rs` only if the exact operation-specific completion API needs a focused internal helper

**Interfaces:**
- Consumes: the red fake-transport tests and existing `BluetoothCore::connect_reply`/`owner_changed` APIs.
- Produces: `retire_in_flight_tasks`, dispatch eligibility/coalescing helpers, operation-specific Connect queue failure, and retained replacement Probe behavior.

- [ ] **Step 1: Split generation cleanup.**

  Track in-flight Connect/Disconnect paths separately from `FuturesUnordered`. Keep `retire_generation_work` for lifecycle/connection-loss cleanup, but make the post-`handle_signal` generation path retire only old futures and in-flight markers. Keep the existing queued-action clear immediately before replacement actions are enqueued.

- [ ] **Step 2: Coalesce and serialize device actions.**

  On queue admission, remove older queued Connect actions for the same path before retaining the newest action. At dispatch, skip a Connect action whose path is already in flight while allowing other paths to dispatch. Mark a path in flight before calling `BusSession::execute`; remove it after every terminal task result, including stale results.

- [ ] **Step 3: Settle queue overflow by exact identity.**

  Change `fail_action` for `CoreAction::Connect` to call `core.connect_reply` with the action’s session generation, BlueZ generation, operation ID, object path, target, `false`, and the existing bounded-queue error. Do not call generic `operation_failed` for Connect.

- [ ] **Step 4: Remove production joining.**

  In `BluetoothWorker::drop`, try to send shutdown and take/drop the handle without joining. Leave `WorkerHarness::shutdown` as the explicit test-only join path.

- [ ] **Step 5: Run the focused tests to green.**

  Run:

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth::bluez::client::worker_lifecycle_tests
  ```

  Expected: all generation, scheduling, queue, stale-completion, and lifecycle tests pass.

### Task 3: Implement explicit owner lookup classification and recovery

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`

**Interfaces:**
- Consumes: owner lookup fake tests and zbus’s typed `NameHasNoOwner`/method error forms.
- Produces: `OwnerLookupError`, `BusSession::owner -> Result<Option<String>, OwnerLookupError>`, and connection retry behavior that treats only a genuine absent owner as healthy.

- [ ] **Step 1: Add explicit owner lookup error handling.**

  Preserve the existing `BusStreams::new` before owner query ordering. Map zbus `NameHasNoOwner` to `Ok(None)`, map timeout/transport/unexpected errors to `OwnerLookupError`, and propagate lookup errors out of `connect_attempt` so the worker enters `Disconnected` with normal backoff.

- [ ] **Step 2: Update fake transport behavior.**

  Make fake sessions return `Ok(Some(owner))`, `Ok(None)`, or `Err(OwnerLookupError)` and keep signal subscriptions available in all successful connections.

- [ ] **Step 3: Run owner-focused tests.**

  Run:

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth::bluez::client::worker_lifecycle_tests::owner
  ```

  Expected: no-owner remains connected without repeated attempts; failures retry; success probes and reaches `Ready`.

### Task 4: Format, lint, run Rust/Qt gates, build, and verify

**Files:**
- Modify: no source files unless verification exposes a defect

**Interfaces:**
- Consumes: completed Rust implementation and existing Eclipse build/test configuration.
- Produces: fresh verification evidence and a logical git commit containing only this pass’s source/tests/docs.

- [ ] **Step 1: Verify Aether output paths before every build.**

  Print and inspect `/mnt/Aether/Desktop/GitHub/Eclipse-target` and the CMake build directory before invoking Cargo or CMake. Stop if any effective output path resolves under `/home/agony/GitHub/Eclipse`.

- [ ] **Step 2: Run the required Rust checks.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo fmt --manifest-path shared/backend/Cargo.toml -- --check
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo clippy --locked --manifest-path shared/backend/Cargo.toml --all-targets -- -D warnings
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml
  tools/ci/run-rust-gate.sh
  tools/ci/run-qml-gate.sh
  ```

- [ ] **Step 3: Run supported CMake build/tests and sanitizers.**

  Configure/build under `/mnt/Aether/Desktop/GitHub/Eclipse-build`, then run `system-services-test`, `bar-qml-smoke-test`, `shell-runtime-test`, CTest, and the supported ASan/UBSan configurations. Run CI helper/workflow-policy tests if build or CI files changed.

- [ ] **Step 4: Review the final diff and commit.**

  Reinspect generation ordering, Probe ownership, queued/in-flight cleanup, same-device dispatch, latest intent, queue overflow, owner classification, reconnect, Drop/Qt blocking, scan mailbox, health notification compatibility, and the absence of C++ BlueZ authority. Commit only the logical Bluetooth correctness changes and their approved design/plan docs.
