# Bluetooth Phase 1 Correctness Pass

## Scope

This pass hardens the existing Phase 1 Rust Bluetooth backend only. It does not
add pairing, Agent1, Trust, Forget Device, a Settings Bluetooth page, new
user-visible Bluetooth behavior, or migrate `BluetoothDeviceModel`.

## Design

The worker will own generation replacement ordering. `NameOwnerChanged` will
discard queued actions belonging to the old generation before `BluetoothCore`
creates the replacement action, and the post-signal cleanup will discard only
old in-flight futures and their per-device in-flight markers. The replacement
Probe therefore remains queued and is dispatched by the normal worker loop.
Lifecycle and connection-loss cleanup will continue to clear both layers.

The worker will also own a small per-device Connect/Disconnect scheduler. A
device path may have one active D-Bus method future. New actions for that path
replace older queued intent; a replacement waits until the active method
future returns. Dispatch eligibility is checked independently per path, so
different devices remain concurrent. Core operation identities and
authoritative `Device1.Connected` state remain the source of truth for public
state and completion acceptance.

Queue admission will settle a rejected Connect action through
`BluetoothCore::connect_reply` with its exact operation identity. A stale
rejected action is ignored, while a rejected current action is removed and
reports the existing bounded-queue error. The queue remains bounded and never
waits for capacity.

Owner discovery will return `Result<Option<String>, OwnerLookupError>`. A typed
`NameHasNoOwner` response becomes `Ok(None)` after signal subscriptions are
installed. Timeouts, transport failures, and other lookup failures reject the
connection attempt and use the existing bounded reconnect backoff. The
subscription-before-lookup ordering remains unchanged.

Production `BluetoothWorker::drop` will signal shutdown and relinquish the
`JoinHandle` without joining from the QObject destruction path. Shutdown still
wakes all worker waits and the deterministic test harness retains an explicit
join for tests.

## Regression coverage

Worker-level fake-transport tests will cover replacement Probe retention and
BlueZ disappearance/return, generation task retirement, same-device
serialization and latest-intent coalescing, cross-device concurrency, stale
completion handling, authoritative convergence, exact queue-overflow
settlement, owner lookup classification/retry, and the no-join production
lifecycle structure. Existing core, Rust, Qt, CMake, gate, sanitizer, and
workflow-policy tests remain in scope for verification.
