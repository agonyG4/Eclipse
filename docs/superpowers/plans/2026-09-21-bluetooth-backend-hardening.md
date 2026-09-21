# Bluetooth Backend Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden the approved Rust Bluetooth backend so scan ownership is lossless, system D-Bus recovery is bounded and cancellable, old BlueZ work is retired immediately, Qt health notifications are complete, and device connect/disconnect operations use their modeled identities.

**Architecture:** Keep the current Shell/Bar QML → thin C++ facade → temporary C++ device model → CXX-Qt Rust engine → Rust policy core → zbus BlueZ client architecture. Move scan-owner transport to a coalesced desired-state mailbox, implement a transport-injected worker lifecycle around fresh zbus connections, and keep all policy/generation checks in `BluetoothCore` and `DiscoveryState`.

**Tech Stack:** Rust 2024, async-channel, async-io, futures/smol, zbus 5, CXX-Qt 0.10, Qt 6.8, QSignalSpy, CMake/CTest.

## Global Constraints

- Do not add pairing, Agent1, a Settings Bluetooth page, or additional Bluetooth features.
- Do not reintroduce `BluezBackend`, `BluezDiscoveryState`, or `BluezObjectStore`.
- Do not move D-Bus calls to C++; do not use JSON across the Qt/Rust boundary; do not migrate `BluetoothDeviceModel` in this pass.
- Rust `DiscoveryState` remains authoritative for discovery policy and the local Astrea discovery lease is released best-effort on shutdown.
- All compilation, tests, benchmarks, generated build artifacts, and temporary build output use `/mnt/Aether/Desktop/GitHub`.
- Before every compile/build command, print and verify the effective output directory is under `/mnt/Aether/Desktop/GitHub`, never `/home/agony/GitHub/...`.
- Preserve unrelated existing worktree changes and stage only files belonging to this hardening pass plus the already-approved migration files required by the final backend build.

## File Map

- Modify `shared/backend/src/bluetooth/discovery.rs`: apply coalesced owner sets without weakening lease policy.
- Modify `shared/backend/src/bluetooth/operations.rs`: add pure per-device connect/disconnect pending-operation identity state.
- Modify `shared/backend/src/bluetooth/engine.rs`: track device operations, retire them on authoritative convergence and session/BlueZ changes, and expose generation-safe completion helpers.
- Modify `shared/backend/src/bluetooth/bluez/client.rs`: add coalesced scan-owner wake transport, a testable bus transport abstraction, reconnect lifecycle, stream outcome handling, and old-generation task cancellation.
- Modify `shared/backend/tests/*.rs` only when needed to expose test seams and add focused pure/worker tests.
- Modify `shared/system/bluetooth/BluetoothService.cpp`: aggregate health-field notification changes exactly once per snapshot.
- Modify `shared/system/tests/SystemServicesTest.cpp`: add QSignalSpy regressions for all `healthJson()` fields and adapter-name-only behavior.

---

### Task 1: Add pure Connect/Disconnect operation identity state

**Files:**
- Modify: `shared/backend/src/bluetooth/operations.rs`
- Modify: `shared/backend/src/bluetooth/engine.rs`
- Test: `shared/backend/tests/bluetooth_operations.rs`
- Test: `shared/backend/tests/bluetooth_engine.rs`

**Interfaces:**
- Produce `PendingDeviceOperation { operation_id, object_path, target_connected, session_generation, bluez_generation }` and an operation-state helper that can begin, converge, complete, clear by session, and clear by BlueZ generation.
- `BluetoothCore::connect_device` records the latest operation for the path before returning `CoreAction::Connect`.
- `BluetoothCore::connect_reply(session_generation, bluez_generation, operation_id, object_path, target_connected, success, error)` ignores stale identities and only degrades the service for the current operation before authoritative convergence.

- [ ] **Step 1: Write the failing pure tests.** Add tests for Connect replaced by Disconnect, Disconnect replaced by Connect, stale failure after authoritative convergence, same-path stale completion, independent operations on two paths, and clearing all pending operations on session and BlueZ generation changes. Assert no public connected state is changed by begin or method success.
- [ ] **Step 2: Run the focused tests and observe the identity failures.**

```bash
build_root=/mnt/Aether/Desktop/GitHub/Eclipse-target
printf 'CARGO_TARGET_DIR=%s\n' "$build_root"
case "$build_root" in /mnt/Aether/Desktop/GitHub/*) ;; *) exit 1 ;; esac
CARGO_TARGET_DIR="$build_root" cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_operations bluetooth_engine
```

Expected: failure because per-device pending identity and generation-safe completion are not implemented.
- [ ] **Step 3: Implement the minimal state.** Use a `BTreeMap<String, PendingDeviceOperation>`, allocate a fresh monotonic ID for each request, replace only the same path, check all five identity fields on completion, clear on target convergence, and clear from `start_generation`, `stop_generation`, and `owner_changed`. Do not mutate `BluetoothDevice.connected` optimistically.
- [ ] **Step 4: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_operations bluetooth_engine` and confirm green.**
- [ ] **Step 5: Commit with `git add shared/backend/src/bluetooth/operations.rs shared/backend/src/bluetooth/engine.rs shared/backend/tests/bluetooth_operations.rs shared/backend/tests/bluetooth_engine.rs && git commit -m "fix: make Bluetooth device operations generation-safe"`.**

### Task 2: Make scan-owner transport lossless under ordinary-queue saturation

**Files:**
- Modify: `shared/backend/src/bluetooth/discovery.rs`
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Test: `shared/backend/tests/bluetooth_discovery.rs`
- Add/modify: `shared/backend/tests/bluetooth_worker.rs`

**Interfaces:**
- Produce `DiscoveryState::replace_owners(BTreeSet<String>)` (or equivalent) that applies a complete desired owner set and returns only the next policy action.
- `BluetoothWorker::request_scan(session_generation, owner) -> bool` and `BluetoothWorker::release_scan(session_generation, owner)` update a mutex-protected coalesced mailbox before attempting a one-slot wake send.
- `WorkerControl::ScanOwnersChanged` is only a wake; it never carries owner data.

- [ ] **Step 1: Write failing saturation tests.** Fill all `COMMAND_CAPACITY` ordinary slots, request two owners, release one and then the other, service only the coalesced scan wake, and assert `DiscoveryState::owners()` ends empty and a held local lease emits `Stop`. Assert releasing an absent owner is harmless.
- [ ] **Step 2: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_discovery bluetooth_worker`; confirm the current lossy release behavior fails the new worker test.**
- [ ] **Step 3: Implement the mailbox.** Remove `RequestScan` and `ReleaseScan` from `WorkerCommand`. Add `ScanOwnerMailbox { session_generation, owners }` behind `Arc<Mutex<_>>`, a bounded one-slot scan-control channel, and a handler that reads the latest state and applies it to `DiscoveryState`. Recover poisoned mutexes; never wait for capacity on the caller thread. Clear the mailbox on service stop, but retain `DiscoveryState` demand across BlueZ owner loss/recovery.
- [ ] **Step 4: Run the focused tests and `rg -n "blocking_send|send\(" shared/backend/src/bluetooth/bluez/client.rs`; expect green tests and no blocking caller send.**
- [ ] **Step 5: Commit with `git add shared/backend/src/bluetooth/discovery.rs shared/backend/src/bluetooth/bluez/client.rs shared/backend/tests/bluetooth_discovery.rs shared/backend/tests/bluetooth_worker.rs && git commit -m "fix: make Bluetooth scan ownership lossless"`.**

### Task 3: Introduce a transport-injected recoverable D-Bus lifecycle

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Modify: `shared/backend/src/bluetooth/bluez/mod.rs` if the seam is split out
- Test: `shared/backend/tests/bluetooth_worker.rs`

**Interfaces:**
- Produce private `BusTransport`/`BusConnection` abstractions with boxed futures for connect, owner lookup, managed-object probe, BlueZ operations, and signal outcomes.
- Production connect creates `Connection::system()`, `BusStreams::new(&connection)`, and `BusDaemonProxy::new(&connection)` together for every attempt.
- Signal delivery explicitly represents `Some(Ok(message))`, `Some(Err(error))`, and terminal `None`.
- Retry delays are 500 ms, 1 s, 2 s, and 5 s maximum; success resets the retry index; shutdown and stop cancel connect/backoff waits.

- [ ] **Step 1: Write fake-transport tests first.** Script initial connection failure then success/probe, signal error then recovery, signal `None` then recovery, no attempts while stopped, and immediate shutdown during backoff. Assert fresh fake connection construction and an unavailable snapshot while disconnected.
- [ ] **Step 2: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_worker`; confirm the one-shot worker fails these expectations.**
- [ ] **Step 3: Replace the one-shot split with a loop that keeps `BluetoothCore` and desired scan state alive.** In stopped state wait only on lifecycle/scan/ordinary/shutdown notifications. In disconnected running state add the bounded reconnect deadline. On success create fresh streams/proxy state, resolve `org.bluez`, call `owner_changed`, and enqueue a fresh probe. On setup failure, stream `Err`, or stream `None`, drop the connection/session futures, call `owner_changed(session, None)` to invalidate BlueZ state while retaining demand, publish unavailable/degraded state, and schedule retry.
- [ ] **Step 4: Route scan wakes and ordinary commands through every lifecycle state; use the complete connect identity for task results; stop clears tasks and pending device operations while shutdown retains best-effort `stop_discovery_action` on the current connection.**
- [ ] **Step 5: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_worker bluetooth_engine bluetooth_discovery` and inspect `client.rs` to verify explicit `Ok`/`Err`/`None`, bounded timers, and no busy-spin path.**
- [ ] **Step 6: Commit with `git add shared/backend/src/bluetooth/bluez/client.rs shared/backend/src/bluetooth/bluez/mod.rs shared/backend/tests/bluetooth_worker.rs && git commit -m "fix: recover Bluetooth worker D-Bus connections"`.**

### Task 4: Cancel old-generation task futures immediately

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Test: `shared/backend/tests/bluetooth_worker.rs`

- [ ] **Step 1: Add a failing test** with `TASK_LIMIT` old-generation fake tasks that never complete; trigger `NameOwnerChanged`, assert all old futures are dropped, and assert the replacement generation's probe begins immediately.
- [ ] **Step 2: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_worker` and confirm old tasks occupy the limit.**
- [ ] **Step 3: Clear queued actions and replace `FuturesUnordered` with a new empty set on BlueZ generation replacement, connection loss, and session stop/restart before enqueueing the new probe. Keep all result generation checks as a second safety barrier.**
- [ ] **Step 4: Run `CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_worker bluetooth_engine` and confirm green.**
- [ ] **Step 5: Commit with `git add shared/backend/src/bluetooth/bluez/client.rs shared/backend/tests/bluetooth_worker.rs && git commit -m "fix: cancel stale BlueZ generation tasks"`.**

### Task 5: Complete Qt healthChanged semantics

**Files:**
- Modify: `shared/system/bluetooth/BluetoothService.cpp`
- Test: `shared/system/tests/SystemServicesTest.cpp`

- [ ] **Step 1: Add QSignalSpy tests before changing production code.** Apply snapshots that change one `healthJson()` field at a time and assert one `healthChanged` notification for state, available, ready, error, adapter availability, powered, scanning, and connected count. Apply an adapter-name-only change and assert `adapterChanged` without `healthChanged`.
- [ ] **Step 2: Run the existing Aether CMake `system-services-test` target after printing and verifying `/mnt/Aether/Desktop/GitHub/Eclipse-build`; confirm the new field assertions fail for the current two-field predicate.**
- [ ] **Step 3: Include all eight `healthJson()` fields in one pre-assignment predicate. Keep field-specific notifications, and prevent `setState`/`setErrorString` from causing a duplicate aggregate emission for the same snapshot.**
- [ ] **Step 4: Run `ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R system-services-test --output-on-failure` and confirm green.**
- [ ] **Step 5: Commit with `git add shared/system/bluetooth/BluetoothService.cpp shared/system/tests/SystemServicesTest.cpp && git commit -m "fix: notify Bluetooth health fields completely"`.**

### Task 6: Full verification and final architecture review

**Files:**
- Modify only if a focused verification finding requires a correction in the files above.

- [ ] **Step 1: Verify target output and run Rust checks.**

```bash
build_root=/mnt/Aether/Desktop/GitHub/Eclipse-target
printf 'CARGO_TARGET_DIR=%s\n' "$build_root"
case "$build_root" in /mnt/Aether/Desktop/GitHub/*) ;; *) exit 1 ;; esac
cargo fmt --manifest-path shared/backend/Cargo.toml -- --check
CARGO_TARGET_DIR="$build_root" cargo clippy --locked --manifest-path shared/backend/Cargo.toml --all-targets -- -D warnings
CARGO_TARGET_DIR="$build_root" cargo test --locked --manifest-path shared/backend/Cargo.toml
```

- [ ] **Step 2: Run `tools/ci/run-rust-gate.sh` and `tools/ci/run-qml-gate.sh`, verifying any build output override before invocation.**
- [ ] **Step 3: Read repository CMake instructions/scripts, print and verify a supported `-B /mnt/Aether/Desktop/GitHub/Eclipse-build...` directory, configure/build there, then run `system-services-test`, `bar-qml-smoke-test`, `shell-runtime-test`, and complete CTest.**
- [ ] **Step 4: Run supported ASan and UBSan configurations in separate verified Aether build directories.**
- [ ] **Step 5: Run CI helper/policy tests after any build/workflow change.**
- [ ] **Step 6: Inspect final diff/source for saturated scan release, non-blocking Qt calls, reconnect/backoff and stream outcomes, generation cancellation, operation identity, health coverage, worker destruction, QML compatibility, and absence of old C++ BlueZ authority; stage only the completed logical hardening/migration files and commit.**
