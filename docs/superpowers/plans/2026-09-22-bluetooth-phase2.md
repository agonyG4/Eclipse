# Bluetooth Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a secure, application-specific BlueZ Agent1, pairing/cancellation, authoritative Trust/Untrust, and Forget Device to the shared Rust Bluetooth backend while preserving the existing Shell/Bar Qt contract.

**Architecture:** Keep `BluetoothCore` authoritative for object-store eligibility, generations, operation identity, convergence, and snapshots. Add typed `agent.rs` and `pairing.rs` domains, extend the existing bounded worker with a connection-owned Agent object and dedicated response/state wake lanes, and extend the thin CXX-Qt/C++ facade without adding BlueZ policy to C++.

**Tech Stack:** Rust 2024, zbus 5.19 server/proxy APIs, async-channel, futures/smol, CXX-Qt 0.10, Qt 6.8, CMake/Ninja, Qt Test, deterministic fake transport/session seams.

## Global Constraints

- Build artifacts, Cargo `target/`, CMake build directories, caches, and temporary compilation output must be under `/mnt/Aether/Desktop/GitHub`.
- Rust production policy, input validation, deadlines, generation checks, D-Bus registration, Agent1 behavior, Trust/Forget semantics, and stale-response rules remain in Rust.
- The Agent object path is `/org/astrea/bluetooth/agent`, capability is `KeyboardDisplay`, and `RequestDefaultAgent` is never called.
- Agent1 interactive prompts have a 60-second deadline; overall Pair has a 120-second deadline.
- PIN text is 1–16 characters; numeric passkeys are `0..=999999` and display as six zero-padded digits at the Qt boundary.
- No mutex guard is held across `.await`; no polling, unbounded queue, per-prompt thread, or global operation serialization is allowed.
- Pair, Trust, and Forget complete only through authoritative ObjectStore projection/convergence; no optimistic `paired`, `trusted`, or device removal state is published.
- Normal pairing/user-authentication outcomes do not transition global Bluetooth health to `Degraded`.
- Existing Bar QML behavior is unchanged: paired rows retain Connect/Disconnect, unpaired rows remain non-clickable, and scan/power/RSSI/battery projection remains unchanged.
- Never log PINs, passkeys, confirmation codes, or place them in `healthJson()` or ordinary diagnostics.

---

### Task 1: Add typed operation and pairing domain tests first

**Files:**
- Create: `shared/backend/src/bluetooth/pairing.rs`
- Modify: `shared/backend/src/bluetooth/operations.rs`
- Modify: `shared/backend/src/bluetooth/mod.rs`
- Create: `shared/backend/tests/bluetooth_pairing.rs`
- Modify: `shared/backend/tests/bluetooth_operations.rs`

**Interfaces:**
- Consumes: current `OperationIds`, `PendingDeviceOperation`, `BluetoothDevice`, `BluetoothCore` generation rules, and ObjectStore projection shape.
- Produces: typed `PairingState`, `PairingSnapshot`, `PendingTrustOperation`, `PendingForgetOperation`, operation result enums, and test fixtures that `BluetoothCore` methods will use.

- [ ] **Step 1: Write failing Pair/Trust/Forget state tests.**

  Add tests for one active Pair session, operation identity matching, Pair method success waiting for `Paired`, authoritative convergence beating late method failure, cancellation retiring the identity, 120-second expiry, Trust/Untrust convergence, late Trust failure after convergence, Forget waiting for disappearance, late Forget failure after removal, and generation retirement. Assert typed fields and result enums; do not assert generic error strings.

- [ ] **Step 2: Run the focused tests and confirm the missing-domain failure.**

  Run:

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_pairing bluetooth_operations
  ```

  Expected: compile/test failure because the new typed state and methods do not exist.

- [ ] **Step 3: Implement the minimal typed operation state.**

  Add constants `PAIRING_TIMEOUT = 120s` and `AGENT_PROMPT_TIMEOUT = 60s` in the focused modules. Define `PairingState` with `Idle`, `EnsuringAgent`, `Pairing`, and `AwaitingAuthoritativePaired` behavior plus operation ID, pairing epoch, session/BlueZ generations, path, deadline, and device display name. Define exact completion enums for Pair, Trust, and Forget. Keep secrets out of all `Debug`/diagnostic representations.

- [ ] **Step 4: Implement authoritative transition methods only.**

  Add methods equivalent to `begin_pair`, `agent_registered`, `pair_method_reply`, `paired_changed`, `cancel`, `expire`, `begin_trust`, `trusted_changed`, `begin_forget`, `device_removed`, and generation retirement. A method success returns an awaiting state; only the ObjectStore event retires a pending Pair/Trust/Forget operation. A matching late failure after convergence returns `Ignored`.

- [ ] **Step 5: Run the focused tests to green and commit the domain slice.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_pairing bluetooth_operations
  git add shared/backend/src/bluetooth/pairing.rs shared/backend/src/bluetooth/operations.rs shared/backend/src/bluetooth/mod.rs shared/backend/tests/bluetooth_pairing.rs shared/backend/tests/bluetooth_operations.rs
  git commit -m "feat(bluetooth): add typed Phase 2 operation state"
  ```

### Task 2: Build the AgentBroker and server-side Agent1 with pure tests

**Files:**
- Create: `shared/backend/src/bluetooth/agent.rs`
- Create: `shared/backend/tests/bluetooth_agent.rs`
- Modify: `shared/backend/src/bluetooth/mod.rs`

**Interfaces:**
- Consumes: typed pairing epoch/path data from Task 1 and zbus 5 `#[zbus::interface]`/`DBusError` APIs.
- Produces: `AgentBroker`, typed `AgentPromptKind`/prompt data, `AgentResponse`, custom `AgentError`, `Agent1` interface implementation, broker view/wake handle, and exact request-ID response methods.

- [ ] **Step 1: Write failing broker tests for every prompt and lifecycle rule.**

  Cover PIN, passkey, confirmation, authorization, service authorization, display PIN, repeated display-passkey coalescing and entered-count updates, accept/reject, stale request IDs, timeout, BlueZ Cancel, Release, service stop, system-bus loss, owner replacement, unauthorized sender, stale owner, second interactive request rejection, and secret-free debug formatting. Use short injected deadlines in tests while keeping the production deadline 60 seconds.

- [ ] **Step 2: Run the broker tests to verify the expected failure.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_agent
  ```

- [ ] **Step 3: Implement typed broker state and the bounded response primitive.**

  Store one `PendingInteractive` containing request ID, epoch, path, prompt kind/data, a bounded response sender, and deadline. Store one coalesced `DisplayPrompt`. Use a mutex only for state transitions; copy the receiver/sender out, release the guard, and await response-or-timeout. Use a bounded one-slot state mailbox/wake sender for worker publication.

- [ ] **Step 4: Implement authorization and all Agent1 methods.**

  Define a custom `#[derive(zbus::DBusError)]` error with BlueZ names `org.bluez.Error.Rejected` and `org.bluez.Error.Canceled`. Implement `Release`, `RequestPinCode`, `DisplayPinCode`, `RequestPasskey`, `DisplayPasskey`, `RequestConfirmation`, `RequestAuthorization`, `AuthorizeService`, and `Cancel` with `#[zbus(header)] Header<'_>`. Reject any sender that does not equal the current authoritative owner/session/generation. Return immediately for display methods. Map user reject to Rejected, all cancellation paths to Canceled, and validation errors to a non-secret rejection/cancellation result.

- [ ] **Step 5: Implement typed input validation and response methods.**

  Validate PIN length before sending the D-Bus response. Parse passkeys as decimal numeric input and reject values outside `0..=999999`. Require request ID equality for every response and make stale responses no-ops. Keep passkeys numeric in Rust; the bridge helper formats them only for Qt presentation.

- [ ] **Step 6: Run broker and formatting checks, then commit.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_agent
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo fmt --manifest-path shared/backend/Cargo.toml -- --check
  git add shared/backend/src/bluetooth/agent.rs shared/backend/src/bluetooth/mod.rs shared/backend/tests/bluetooth_agent.rs
  git commit -m "feat(bluetooth): add application-specific Agent1 broker"
  ```

### Task 3: Extend BluetoothCore snapshots, actions, eligibility, and convergence

**Files:**
- Modify: `shared/backend/src/bluetooth/engine.rs`
- Modify: `shared/backend/src/bluetooth/object_store.rs` only if an authoritative disappearance helper is needed
- Modify: `shared/backend/src/bluetooth/device.rs` only if adapter membership needs a focused helper
- Modify: `shared/backend/tests/bluetooth_engine.rs`
- Modify: `shared/backend/tests/bluetooth_pairing.rs`

**Interfaces:**
- Consumes: Task 1 typed operation state and Task 2 `AgentPromptView`.
- Produces: new `CoreSnapshot` fields, `CoreAction::{RegisterAgent, Pair, CancelPairing, SetTrusted, RemoveDevice}`, `pair_device`, `cancel_pairing`, `set_device_trusted`, `forget_device`, agent-view application, and authoritative event completion.

- [ ] **Step 1: Add failing core policy tests.**

  Add tests for unpaired selected-adapter Pair eligibility, already-paired/unknown/non-selected-adapter rejection, one active Pair session, RegisterAgent-before-Pair action creation, Pair method success waiting for `Paired`, pairing-specific method failure, cancel action generation, overall timeout CancelPairing, generation replacement retirement, Trust/Untrust eligibility/convergence, unknown/unpaired Trust rejection, Forget selected-adapter action, authoritative disappearance completion, terminal Forget conflict rejection, and queue identities.

- [ ] **Step 2: Run core tests to confirm missing action/state APIs.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_engine bluetooth_pairing
  ```

- [ ] **Step 3: Extend snapshot and action types without changing existing fields.**

  Add typed Rust snapshot fields for pairing and Agent prompt state. Add typed action payloads carrying operation ID, epoch, session/BlueZ generation, owner, adapter/device path, and target. Add pairing-specific error state separate from `CoreSnapshot.error`; do not call `fail()` for normal Pair or Agent rejection.

- [ ] **Step 4: Implement authoritative eligibility and lifecycle transitions.**

  Validate service, owner, selected adapter, power, device presence, adapter membership, pairing/trust state, and conflict state from the ObjectStore projection. Start one Pair session only. Retire/clear Pair, Trust, Forget, and broker-view state on stop, connection loss, owner replacement, and adapter replacement. Keep the existing Connect eligibility and Bar projection unchanged.

- [ ] **Step 5: Integrate property/removal convergence.**

  In the authoritative projection path, match `Paired`, `Trusted`, and device disappearance against exact operation identities and generations. A late method result must not regress a completed operation. Forget remains active after method success until `InterfacesRemoved` or the authoritative projection no longer contains the device.

- [ ] **Step 6: Run focused core tests to green and commit.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth_engine bluetooth_pairing
  git add shared/backend/src/bluetooth/engine.rs shared/backend/src/bluetooth/object_store.rs shared/backend/src/bluetooth/device.rs shared/backend/tests/bluetooth_engine.rs shared/backend/tests/bluetooth_pairing.rs
  git commit -m "feat(bluetooth): add pairing trust and forget policy"
  ```

### Task 4: Add typed BlueZ proxies and connection-owned Agent registration

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/proxies.rs`
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Modify: `shared/backend/src/bluetooth/bluez/mod.rs`
- Modify: `shared/backend/tests/bluetooth_agent.rs`

**Interfaces:**
- Consumes: Task 2 Agent interface and Task 3 CoreAction variants.
- Produces: typed `AgentManager1Proxy`, Pair/CancelPairing/Trusted Device1 methods, Adapter1 RemoveDevice, connection-owned export/register/unregister helpers, and testable operation ordering.

- [ ] **Step 1: Extend proxy declarations with compile-first tests.**

  Add proxy compile/use tests for `RegisterAgent`, `UnregisterAgent`, `Pair`, `CancelPairing`, `set_trusted`, and `RemoveDevice` using typed zbus proxy methods. Do not add `RequestDefaultAgent`.

- [ ] **Step 2: Run the proxy tests to catch API/signature mismatches.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth::bluez
  ```

- [ ] **Step 3: Implement connection-owned Agent registration.**

  Extend `BusSession` with an async registration method that exports `Agent1` at `/org/astrea/bluetooth/agent` on its own `Connection`, then calls typed `RegisterAgent(path, "KeyboardDisplay")`. Track registration in the worker by session and BlueZ generations. Add best-effort `UnregisterAgent` on normal shutdown and never wait for it after connection loss.

- [ ] **Step 4: Add owner/generation invalidation.**

  Update the broker authority before publishing a new owner snapshot. On owner disappearance/replacement or connection loss, invalidate registration and cancel broker waits. Ensure the old object interface is retained only by the old connection and no interface reference escapes into the replacement generation.

- [ ] **Step 5: Extend fake transport ordering tests and commit.**

  Record `AgentExported`, `RegisterAgent`, `Pair`, `CancelPairing`, `SetTrusted`, and `RemoveDevice` events. Assert Agent export precedes registration, registration precedes Pair, no default-agent request occurs, replacement owners cancel old work, and the replacement owner registers again.

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth::bluez
  git add shared/backend/src/bluetooth/bluez/proxies.rs shared/backend/src/bluetooth/bluez/client.rs shared/backend/src/bluetooth/bluez/mod.rs shared/backend/tests/bluetooth_agent.rs
  git commit -m "feat(bluetooth): register Agent1 per BlueZ generation"
  ```

### Task 5: Extend worker commands, scheduler, deadlines, and fake session seam

**Files:**
- Modify: `shared/backend/src/bluetooth/bluez/client.rs`
- Modify: `shared/backend/src/bluetooth/engine.rs` only for focused worker completion helpers
- Modify: `shared/backend/tests/bluetooth_engine.rs`
- Modify: `shared/backend/tests/bluetooth_pairing.rs`
- Modify: `shared/backend/tests/bluetooth_agent.rs`

**Interfaces:**
- Consumes: Task 3 CoreAction and Task 4 BusSession/registration methods.
- Produces: bounded command APIs for Pair/CancelPairing/Trust/Forget, direct broker response APIs, separate control-lane cancellation, Pair/Trust/Forget task dispatch, per-device conflict scheduler, queue saturation settlement, and deadline-driven best-effort cancellation.

- [ ] **Step 1: Add failing worker seam tests.**

  Extend the existing fake transport/session to test Agent export/register/Pair ordering, Pair and CancelPairing overlap, Pair timeout and best-effort CancelPairing, Trust convergence, Forget removal and late failure, same-device conflicting mutation serialization, unrelated-device concurrency, queue saturation identity settlement, owner replacement cancellation, and re-registration after reconnect.

- [ ] **Step 2: Implement bounded command and response lanes.**

  Keep ordinary commands finite and non-blocking. Add a dedicated bounded response mailbox for `submitAgentText`, confirmation, and rejection, and a one-slot Agent state wake mailbox. A response updates broker state directly and never waits for ordinary command capacity.

- [ ] **Step 3: Implement operation dispatch and completion handling.**

  Add `make_task` cases with 120-second Pair deadline and 3-second bounded deadlines for Trust/Forget/registration. Use typed proxies. Dispatch CancelPairing outside the Pair path-in-flight marker so it can run concurrently. Resolve method results through exact operation IDs and let Core authoritative events retire the pending operation.

- [ ] **Step 4: Extend scheduler conflict rules.**

  Track per-device active ordinary mutations. Block Connect/Disconnect/Trust/Forget/Pair conflicts for the same path as required, coalesce only latest Trust intent, make Forget terminal, and leave different paths concurrent. Do not count CancelPairing as a conflicting ordinary operation. When the action queue is full, settle only the affected current identity with an operation result and preserve newer identities.

- [ ] **Step 5: Implement timer handling and shutdown.**

  Include Pair, Trust, Forget, and broker deadlines in the worker timer set. On Pair expiry, retire the operation, cancel the broker prompt, enqueue best-effort CancelPairing without waiting, and ignore the eventual Pair result. On stop/connection loss, wake and cancel broker requests, retire tasks, and let the worker exit without joining from QObject destruction.

- [ ] **Step 6: Run all focused Rust tests to green and commit.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml bluetooth
  git add shared/backend/src/bluetooth/bluez/client.rs shared/backend/src/bluetooth/engine.rs shared/backend/tests/bluetooth_engine.rs shared/backend/tests/bluetooth_pairing.rs shared/backend/tests/bluetooth_agent.rs
  git commit -m "feat(bluetooth): schedule Phase 2 BlueZ operations safely"
  ```

### Task 6: Extend CXX-Qt and the thin C++ Qt facade

**Files:**
- Modify: `shared/backend/src/bluetooth/bridge.rs`
- Modify: `shared/system/bluetooth/BluetoothBackend.hpp`
- Modify: `shared/system/bluetooth/RustBluetoothBackend.hpp`
- Modify: `shared/system/bluetooth/RustBluetoothBackend.cpp`
- Modify: `shared/system/bluetooth/BluetoothService.hpp`
- Modify: `shared/system/bluetooth/BluetoothService.cpp`
- Modify: `shared/system/tests/SystemServicesTest.cpp`

**Interfaces:**
- Consumes: Task 3 snapshot/action fields and Task 5 worker command methods.
- Produces: stable Qt properties, a typed Agent prompt enum, invokables for Pair/CancelPairing/Trust/Forget/responses, notify signals, fake-backend forwarding coverage, and unchanged health JSON.

- [ ] **Step 1: Add failing Qt contract tests.**

  Extend `FakeBluetoothBackend` with counters/last values for Pair, CancelPairing, Trust, Forget, Agent text/confirm/reject. Add QSignalSpy tests for each new state class and a stale response test. Assert `healthJson()` has the same existing keys and excludes secrets. Keep all existing facade tests unchanged.

- [ ] **Step 2: Add typed CXX-Qt snapshot properties and invokables.**

  Add scalar/string properties named `pairing`, `pairingDevicePath`, `pairingDeviceName`, `pairingError`, `agentRequestActive`, `agentRequestId`, `agentRequestKind`, `agentDevicePath`, `agentDeviceName`, `agentPasskey`, `agentEntered`, and `agentServiceUuid`. Use a stable Qt enum or documented integer for request kind. Add invokables with exact request ID arguments and no JSON command payloads.

- [ ] **Step 3: Map Rust state and commands through RustBluetoothBackend.**

  Map the bridge’s typed values into `BluetoothSnapshot`, forward commands directly to `RustBluetoothEngine`, format six-digit passkeys only for Qt presentation, and keep `BluetoothDeviceModel` untouched. Do not add BlueZ calls, timers, generation checks, or secret diagnostics to C++.

- [ ] **Step 4: Update BluetoothService notify behavior.**

  Compute field-specific changes for pairing and Agent state, emit the corresponding Qt signals once per effective change, and keep `healthChanged` driven only by the pre-existing `healthJson()` fields. Clear pairing/Agent state on stop without changing Bar properties.

- [ ] **Step 5: Run Qt unit tests and commit the compatibility slice.**

  Build only through an Aether path verified before the command, then run `system-services-test`. Commit:

  ```bash
  git add shared/backend/src/bluetooth/bridge.rs shared/system/bluetooth/BluetoothBackend.hpp shared/system/bluetooth/RustBluetoothBackend.hpp shared/system/bluetooth/RustBluetoothBackend.cpp shared/system/bluetooth/BluetoothService.hpp shared/system/bluetooth/BluetoothService.cpp shared/system/tests/SystemServicesTest.cpp
  git commit -m "feat(bluetooth): expose Phase 2 Qt contract"
  ```

### Task 7: Documentation, Bar compatibility review, and graph refresh

**Files:**
- Modify: `shared/README.md`
- Modify: `docs/superpowers/specs/2026-09-22-bluetooth-phase2-design.md` only if implementation decisions materially differ
- Do not modify: `Bar/qml/BluetoothPopup.qml`, `Bar/qml/components/BluetoothIndicator.qml`, `shared/system/bluetooth/BluetoothDeviceModel.*`

**Interfaces:**
- Consumes: completed Rust/Qt implementation and existing Phase 1 roadmap language.
- Produces: English architecture documentation describing Phase 1/2/3 and a repository graph refreshed after implementation.

- [ ] **Step 1: Update shared architecture documentation.**

  Replace the current “pairing is future” wording with the Phase 2 boundary: shared Rust owns Agent1, Pair/CancelPairing, Trust/Untrust, and Forget; the API is future-Settings-ready; the existing popup UX is unchanged; Phase 3 is the Settings page and is not implemented.

- [ ] **Step 2: Run literal compatibility checks.**

  Verify `BluetoothPopup.qml` and `BluetoothIndicator.qml` are byte-for-byte unchanged relative to the Phase 2 base and that no new code references `RequestDefaultAgent`, PIN/passkey logging, `healthJson()` secret fields, or arbitrary path acceptance. Run the existing Bar QML smoke test in the CMake gate.

- [ ] **Step 3: Refresh Codebase MCP and inspect changed blast radius.**

  Run the graph index update for `/home/agony/GitHub/Eclipse`, query the new Agent/Pair/Trust/Forget symbols, trace the Qt operations to Rust worker actions, and call `check_index_coverage` for every changed source/test path. Read any newly reported partial ranges directly before relying on graph claims.

### Task 8: Full verification, sanitizer runs, final review, and commit

**Files:**
- Modify: source/tests/docs only if a fresh verification failure identifies a concrete defect

**Interfaces:**
- Consumes: all completed Phase 2 source, tests, and documentation.
- Produces: fresh verification evidence and a final logical commit containing the implementation.

- [ ] **Step 1: Verify output locations before each build.**

  Use explicit paths and print them:

  ```bash
  rust_target=/mnt/Aether/Desktop/GitHub/Eclipse-target
  cmake_build=/mnt/Aether/Desktop/GitHub/Eclipse-build
  case "$rust_target" in /mnt/Aether/Desktop/GitHub/*) ;; *) exit 1 ;; esac
  case "$cmake_build" in /mnt/Aether/Desktop/GitHub/*) ;; *) exit 1 ;; esac
  realpath -m "$rust_target" "$cmake_build"
  ```

  Stop if any effective Cargo target or CMake binary directory resolves under `/home/agony/GitHub/Eclipse`.

- [ ] **Step 2: Run the required Rust and QML gates with Aether output.**

  ```bash
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo fmt --manifest-path shared/backend/Cargo.toml -- --check
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo clippy --locked --manifest-path shared/backend/Cargo.toml --all-targets -- -D warnings
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --locked --manifest-path shared/backend/Cargo.toml
  CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target tools/ci/run-rust-gate.sh
  ASTREA_CMAKE_BUILD_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-qml-gate-build tools/ci/run-qml-gate.sh
  ```

  Confirm each command exits zero; do not infer gate success from partial output.

- [ ] **Step 3: Configure and build CMake outside the checkout.**

  Use direct supported options so preset `binaryDir` cannot place output in the repository:

  ```bash
  cmake -S /home/agony/GitHub/Eclipse -B /mnt/Aether/Desktop/GitHub/Eclipse-build -G Ninja \
    -DBUILD_TESTING=ON -DASTREA_BUILD_TESTS=ON \
    -DASTREA_ENABLE_LAYER_SHELL=ON -DASTREA_ENABLE_TYPHON_BACKEND=ON \
    -DASTREA_ENABLE_ASAN=OFF -DASTREA_ENABLE_UBSAN=OFF \
    -DASTREA_ENABLE_SANITIZERS=OFF -DCMAKE_BUILD_TYPE=Debug
  cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build
  ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build --output-on-failure
  /mnt/Aether/Desktop/GitHub/Eclipse-build/shared/system/system-services-test
  /mnt/Aether/Desktop/GitHub/Eclipse-build/Shell/bar-qml-smoke-test
  /mnt/Aether/Desktop/GitHub/Eclipse-build/Shell/shell-runtime-test
  ```

  Resolve target paths with `cmake --build ... --target help` if the generated target layout differs; run the named targets themselves, not an unverified guessed path.

- [ ] **Step 4: Run supported ASan and UBSan configurations under Aether.**

  Configure separate directories with the same source and test options, set one sanitizer cache variable at a time, build, and run CTest. Record any environment-based skips explicitly.

- [ ] **Step 5: Run CI helper/workflow-policy tests if build files changed.**

  Use the repository’s documented helper test command discovered with `rg -n "workflow|policy|ci.*test" tools/ci tools/tests .github`; do not claim success if it was not run.

- [ ] **Step 6: Perform the final security and compatibility checklist.**

  Review Agent object lifetime, per-generation registration, sender authentication, concurrent Cancel/Release, Pair/CancelPairing overlap, deadlines, authoritative convergence, Trust/Forget removal, per-device ordering, queue saturation, reconnect/BlueZ restart, Qt destruction, stale QML response IDs, secret logging, no default-agent takeover, unchanged Bar QML, and no Settings page.

- [ ] **Step 7: Inspect diff, graph impact, and commit the completed logical change.**

  Run `git diff --check`, `git status --short`, and `git diff --stat`, confirm no build artifacts are in the checkout, then commit all remaining Phase 2 implementation/tests/docs changes with a focused message.
