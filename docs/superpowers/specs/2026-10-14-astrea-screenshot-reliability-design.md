# Astrea Screenshot Shortcut Reliability Design

## Verified implementation facts

- Typhon maps `KEY_SYSRQ`, `Super+KEY_SYSRQ`, and `Super+Shift+KEY_SYSRQ` to the existing `screenshot_quick`, `screenshot_region_frozen`, and `screenshot_region_live` shell shortcut names.
- Typhon keeps one live owner per `astrea-shell` shortcut name. A newer authorized registration replaces the previous owner and sends `cancelled`.
- The current shortcut protocol also uses `cancelled` when registration is rejected by authorization. A client cannot distinguish rejection from ownership loss.
- Eclipse registers seven shell shortcuts. A canceled registration is destroyed without recovery or per-entry health. The client can remain `Ready` while an individual shortcut is missing.
- `ScreenshotController` rejects starts outside `Idle` and has no deadline for barrier synchronization or compositor capture.
- `ScreenshotWaylandUiBarrier` owns one completion, queued work, and an optional `wl_callback`, but exposes no cancellation operation.
- The screen capture client destroys its protocol resource on cancellation. Typhon removes the matching pending compositor capture when the resource is destroyed. Eclipse currently does not flush that destroy request immediately.
- Capture completion signals identify the shared Wayland connection generation, not the screenshot operation. A stale response from the same generation can match a later waiting operation.
- Capture request failure can be emitted synchronously from `requestCapture()`. Existing phase checks avoid replacing an inline failure with the boolean rejection path.
- Successful persistence, clipboard copy, thumbnail, preview, frozen selection, and live selection are already implemented and must retain their ordering and behavior.

These facts identify independent liveness gaps. They do not prove which gap caused the October 14 incident; prior successful saves do not distinguish a stuck operation from a missing shortcut owner.

## Invariants

1. Preserve all physical key mappings and screenshot intent behavior.
2. Preserve single ownership per shell shortcut name and Astrea shell authorization.
3. A capture operation is pending only in `PreparingCapture`, `WaitingCapture`, or `WaitingLiveCapture`; frozen and live user selection has no ordinary-operation deadline.
4. A failed or canceled operation invalidates its token, cancels any barrier and active capture resource, and returns the controller to `Idle` exactly once.
5. UI screenshot surfaces remain hidden and Wayland-synchronized before a desktop capture is requested.
6. Older timer, barrier, capture, or shortcut events cannot mutate a newer operation or connection generation.
7. Retry ownership loss at most once per shortcut per shared connection generation. Never retry an authorization rejection. Reconnect restores the complete seven-entry set once.
8. Existing status JSON fields remain intact; new health data is additive.

## Architecture

The screenshot controller uses one single-shot `QTimer`, operation-token validation, and separate deadlines: 5 seconds for UI synchronization and 15 seconds for compositor capture. Timeout records a stage and reason, cancels outstanding asynchronous work, and resets to `Idle`. Interactive selection phases stop the timer. Capture completion carries both connection generation and operation token so same-connection late responses are rejected. State updates are guarded after synchronous signal emissions.

`ScreenshotUiBarrier` gains an idempotent `cancel()` method. The Wayland implementation advances a generation on cancel, invalidates queued synchronization closures, destroys an outstanding callback while the display proxy is valid, and drops the pending completion. Callback identity and generation must both match before completion.

The shortcut protocol advances to version 2 and adds `rejected(serial)` for authorization failures. Version 1 behavior remains supported; because its cancellation event is ambiguous, a version 1 client reports cancellation but does not retry. A version 2 client marks rejected entries as non-recoverable and uses a single delayed, generation-bound recovery attempt only after ownership loss. Exhausted or canceled entries remain visible in health status. Transport reconnection discards old objects and registers all seven entries once.

The shell status endpoint adds screenshot phase, intent, pending state, timeout remaining, client readiness, last failure stage/reason, rejected request count, shortcut connection state/count, and per-shortcut health. Dispatcher logging records screenshot shortcut acceptance or active-operation rejection without changing Alt+Tab or Spotlight routing.

## Considered approaches

1. **Diagnostics only:** Smallest patch, but a stuck controller and missing shortcut remain stuck.
2. **Screenshot watchdog and barrier cancellation only:** Recovers captures, but a canceled shortcut still cannot reach the controller.
3. **Bounded screenshot recovery plus versioned shortcut outcome:** Recommended. It addresses both independent liveness gaps, distinguishes authentication from ownership loss, and retains the existing ownership policy and user-facing behavior.

## Validation scope

Qt tests cover timeout, retrying a capture after timeout, stale completion rejection, cancellation, synchronous failure, interactive selection, persistence, and dismissal ordering. Wayland protocol integration tests exercise authorization rejection, exclusive-owner takeover, bounded retry, reconnection, and unaffected Alt+Tab/Spotlight entries. The requested Eclipse targets and relevant Typhon Rust tests are run using the approved Aether build directories. Validation must not restart the running compositor or shell.
