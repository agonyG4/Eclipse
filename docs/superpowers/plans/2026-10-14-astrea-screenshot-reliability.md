# Astrea Screenshot Shortcut Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep screenshot shortcuts usable after asynchronous failures and report shortcut/capture health accurately.

**Architecture:** Add bounded, token-validated screenshot deadlines and a cancellable Wayland UI barrier. Version the shortcut outcome protocol so authorization rejection is distinct from ownership loss, then recover each displaced registration at most once per connection generation. Extend the existing shell status JSON additively.

**Tech Stack:** Rust, Qt 6/C++, QML, Wayland protocol XML, Qt Test, Cargo tests.

**Spec:** `docs/superpowers/specs/2026-10-14-astrea-screenshot-reliability-design.md`

## Global Constraints

- Preserve physical screenshot shortcut mappings and user-visible capture behavior.
- Keep one owner per `astrea-shell` shortcut name and preserve existing shell authorization.
- Use a 5-second UI synchronization deadline and a 15-second compositor capture deadline; do not time out interactive selection.
- Retry ownership loss at most once per registration per connection generation; never retry authorization rejection.
- Use the existing Aether build directories; never place build output in a checkout or system SSD.
- Preserve all pre-existing working-tree edits and do not restart the running compositor or shell.

## Review Focus

- Inline completion from `requestCapture()` must not rearm a timer or overwrite success/failure; cover synchronous ready and failure.
- A same-connection late result must not complete a newer screenshot; assert operation-token mismatch is ignored.
- A queued barrier task after cancel must not start synchronization for a newer operation; test cancellation followed by a new synchronize call.
- Authorization rejection must not trigger a takeover retry; test v2 `rejected` and v1 ambiguous cancellation behavior.
- Repeated owner replacement must not create unbounded takeover; exercise real fake-server exclusive ownership and verify the per-generation retry cap.
- Reconnection must restore all seven registrations exactly once while unrelated Alt+Tab and Spotlight events remain routable.

---

### Task 1: Screenshot watchdog, cancellation, and capture correlation

**Files:**
- Modify `Shell/screenshot/ScreenshotController.hpp/.cpp`
- Modify `shared/platform/typhon/TyphonScreenCaptureClient.hpp/.cpp`
- Test `Shell/tests/ScreenshotControllerTest.cpp`
- Test `shared/tests/TyphonScreenCaptureProtocolIntegrationTest.cpp`

**Interfaces:**
- `TyphonScreenCaptureClient::requestCapture(std::uint64_t operationToken)` stores the token for its active resource.
- `ready` and `failed` signals retain connection generation and add the operation token.
- `ScreenshotController` exposes phase/intent/pending/deadline/failure/rejection accessors for status JSON.

- [ ] Add tests for a never-completing capture timing out, returning to `Idle`, allowing a second capture, and ignoring a prior operation token.
- [ ] Add tests for a synchronous request failure leaving no active timer and for frozen/live selection remaining untimed.
- [ ] Run `screenshot-controller-test` and `typhon-screen-capture-protocol-integration-test` to observe expected failures.
- [ ] Implement 5-second UI and 15-second capture watchdog stages, disarm on success/cancel/failure, cancel capture resources, flush destruction, and record failure stage/reason.
- [ ] Add operation-token checks after synchronous signals and preserve persistence, clipboard, thumbnail, and preview order.
- [ ] Re-run both targets and verify the test cases pass.

### Task 2: Cancellable screenshot UI barrier

**Files:**
- Modify `Shell/screenshot/ScreenshotUiBarrier.hpp/.cpp`
- Test `Shell/tests/ScreenshotUiBarrierTest.cpp`
- Test `Shell/tests/ScreenshotControllerTest.cpp`

**Interfaces:**
- `ScreenshotUiBarrier::cancel()` is virtual, idempotent, and invalidates any pending completion.
- Queued and Wayland callback work is accepted only for its captured barrier generation and callback identity.

- [ ] Test canceling a queued synchronization, repeated cancellation, and starting a newer synchronization before old queued work runs.
- [ ] Test controller timeout/cancel clears a pending fake barrier and does not let its old completion affect a later operation.
- [ ] Run `screenshot-ui-barrier-test` and `screenshot-controller-test` to observe expected failures.
- [ ] Implement generation-aware queued work and destroy the pending `wl_callback` on the Qt/Wayland owner thread when its display remains valid.
- [ ] Re-run both targets; verify surface hiding and sync-before-capture order remain covered.

### Task 3: Versioned shortcut rejection and bounded ownership recovery

**Files:**
- Modify Typhon `protocols/astrea-shortcuts-v1.xml`, `src/compositor/protocols/shortcuts.rs`, and `src/compositor/tests/astrea_shortcuts.rs`
- Modify Eclipse `shared/platform/typhon/protocols/astrea-shortcuts-v1.xml`, `shared/platform/typhon/TyphonShortcutClient.hpp/.cpp`, and `shared/tests/TyphonShortcutProtocolIntegrationTest.cpp`
- Modify `shared/tests/TyphonShortcutProtocolServerCompat.hpp` as needed for the v2 event

**Interfaces:**
- Shortcut manager/resource protocol version becomes 2; v2 adds `rejected(serial)` while preserving v1 events.
- Typhon sends v2 `rejected` for authorization failure and `cancelled` for replacement. Version 1 retains compatible legacy signaling.
- `TyphonShortcutClient::registrationHealth()` exposes per-entry registered, recovery-pending, canceled, or rejected state.

- [ ] Extend the fake Wayland compositor to model one exclusive live owner per namespace/name; add authorization rejection and replacement tests.
- [ ] Assert v1 cancellation is reported as ambiguous with no retry; assert v2 rejection is not retried.
- [ ] Assert a v2 displaced entry is retried once after delay, repeated cancellation exhausts its budget, and other entries remain routable.
- [ ] Run `typhon-shortcut-protocol-integration-test` and Typhon shortcut protocol tests to observe expected failures.
- [ ] Implement protocol v2 rejection, per-entry health, one delayed retry per generation, generation-safe teardown, and full-set registration on transport reconnect.
- [ ] Re-run protocol tests; verify reconnect restores seven entries once and Alt+Tab/Spotlight still dispatch.

### Task 4: Runtime diagnostics and status introspection

**Files:**
- Modify `Shell/platform/shortcut/ShellShortcutDispatcher.cpp/.hpp`
- Modify `Shell/runtime/ShellRuntime.cpp/.hpp` only if state signal wiring is needed
- Modify `Shell/app/AstreaShellApplication.cpp`
- Test `Shell/tests/ShellShortcutDispatcherTest.cpp`
- Test `Shell/tests/ShellUnifiedRuntimeIntegrationTest.cpp`

**Interfaces:**
- Existing `statusJson()` fields stay unchanged; additive `screenshot` and `shortcuts` objects report controller and per-registration health.
- Screenshot dispatch records accepted/rejected input and active-operation rejection counts.

- [ ] Add tests for screenshot accepted/rejected diagnostics and status fields with active/pending and degraded/recovered states.
- [ ] Run `shell-shortcut-dispatcher-test` and `shell-unified-runtime-integration-test` to observe expected failures.
- [ ] Implement additive status JSON and lightweight, stage-specific transition diagnostics; preserve existing Alt+Tab and Spotlight mapping.
- [ ] Re-run both targets and review status output for schema compatibility.

### Task 5: Adversarial review and requested validation

**Files:**
- Review all changed files in both repositories; no unrelated changes.

- [ ] Verify timer phase behavior, stale token/generation guards, callback destruction, protocol flush, one-retry cap, authorization distinction, and state reset ordering.
- [ ] Print and verify each effective Aether build directory before building.
- [ ] Confirm existing build-tree binaries and running executable paths before validation; do not restart or terminate the active shell/compositor.
- [ ] Run all six named Eclipse test targets and relevant Typhon Rust tests; record exact commands and all failures.
- [ ] Review final diffs against the pre-existing dirty Typhon files and commit only task-owned changes on `main`.
