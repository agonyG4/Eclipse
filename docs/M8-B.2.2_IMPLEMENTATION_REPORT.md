# M8-B.2.2 — Final Async Operation Identity and Discovery Lease Closure

## Scope and source baseline

This closure was implemented in the Eclipse repository at source baseline
`0bee6af fix(system): close native services correctness gaps` on 2026-08-20.
The accepted M8-B.2 and M8-B.2.1 architecture was preserved:

```text
NetworkService   -> NetworkManagerBackend -> NetworkManagerState / NetworkScanState
BluetoothService -> BluezBackend          -> BluezObjectStore / BluezDiscoveryState
AudioService     -> PipeWireAudioBackend  -> PipeWireAudioState
```

Production changes are limited to native NetworkManager and BlueZ async
operation routing plus their shared tests. PipeWire architecture and behavior
were preserved. No Bar/QML, TopBar, popup, workspace, System Tray, or Control
Center files were changed.

M8-A.3.1 was not started. M8-C was not started.

## BlueZ discovery lease closure

`BluezDiscoveryState::operationFinished(false, false)` now retains
`StopPending -> Held`. A failed StopDiscovery therefore cannot make Eclipse
forget a lease whose release was not confirmed. A successful stop still moves
`StopPending -> None` without waiting for the global `Adapter1.Discovering`
property to become false; another client may still own discovery.

Discovery retries are now state-derived and shared by acquisition and release:

```text
demand && lease == None  -> StartDiscovery
!demand && lease == Held -> StopDiscovery
otherwise                -> no request
```

The single existing single-shot retry timer uses bounded delays of 500 ms, 1 s,
2 s, and 5 s maximum. A failed or timed-out StopDiscovery schedules release
retry while the lease remains Held. If demand returns before that retry, the
desired state is already satisfied, so reconciliation cancels the retry and
does not issue StartDiscovery or another StopDiscovery. Service stop,
daemon/adapter loss, and owner changes remain lifecycle cancellation points.

Every StartDiscovery and StopDiscovery attempt receives a fresh monotonically
increasing request id. Completion is accepted only when daemon generation,
current request id, and expected operation kind all match. Timeout retires the
request id before state recovery, so a late reply from Stop #1 cannot settle
Stop #2 or alter its retry/timer state.

## NetworkManager RequestScan identity

`NetworkScanState` now stores a per-request id. `NetworkManagerBackend` assigns
a fresh id to each actual RequestScan D-Bus call and captures daemon
generation, selected device path, and request id in its method callback.
Stale callbacks are no-ops when any of those identities or the request phase no
longer match.

Timeout, device replacement, daemon restart, cooldown completion, and failed
method replies retire the current request id. LastScan remains the authority
for actual scan completion; RequestScan method success only advances the state
to `WaitingForLastScan`.

## Bluetooth power identity

Bluetooth power requests now use a separate monotonically increasing request id
from the discovery ids. `BluezBackend::setPowered()` returns that id in its
typed operation result. `BluetoothService` accepts only the current pending
power request, so an old ON result cannot settle a replacement OFF request.

Authoritative `Adapter1.Powered` convergence remains the public-state
authority. Property convergence retires the request identity before a late
method reply can arrive. Timeout, adapter loss, daemon loss, service stop, and
replacement requests all invalidate the old identity.

## Wi-Fi radio power identity

The existing equivalent race also existed in NetworkManager: Wi-Fi power
completion used the shared string `"wifi-enabled"` without an operation id.
The network backend callback is now typed as
`NetworkOperationResult{kind, requestId, success, error}`. Each real
`setWifiEnabled()` call receives a fresh id, and `NetworkService` waits for
authoritative `WirelessEnabled` snapshot convergence. A stale method result
cannot clear or overwrite a newer pending request.

## Regression tests

`SystemServicesTest` now reports 48 QTest framework totals: 46 logical feature
tests after excluding `initTestCase()` and `cleanupTestCase()`.

New or extended production-routing/state coverage includes:

- failed StopDiscovery preserving the Held lease;
- bounded release retry after StopDiscovery failure;
- release retry cancellation when demand returns;
- late StopDiscovery reply rejection across a timeout and replacement stop;
- successful release while global discovery remains true;
- late RequestScan rejection across timeout/retry and Wi-Fi device switch;
- distinct scan request ids across actual requests;
- stale Bluetooth power result rejection across replacement requests;
- power-property convergence retiring a request before its late method result;
- timed-out Bluetooth power result rejection;
- stale Wi-Fi power result rejection across replacement requests.

The tests drive `BluetoothService`, `BluezDiscoveryState`, `NetworkService`, and
`NetworkScanState` with fake transport callbacks. They do not rely only on
pre-completed public snapshots.

## Build and test matrix

Existing build directories and incremental caches were reused:

| Configuration | `system-services-test` build | `system-services-test` |
|---|---:|---:|
| Debug | passed | 48/48 |
| Release | passed | 48/48 |
| Clang | passed | 48/48 |
| ASan | passed | 48/48, clean |
| UBSan | passed | 48/48, clean |
| no-Typhon | passed | 48/48 |
| no-layer-shell | passed | 48/48 |

The fresh Release CTest run registered 63 tests: 61 passed and 2 failed.
The failures were unrelated to this closure and came from pre-existing dirty
worktree changes:

- `settings-navigation-model-test`: current Settings catalogue has 13 rows
  while the test expects 12, and the route is non-empty;
- `shell-unified-runtime-integration-test`: `QLocalServer::listen: Name error`.

The changed `system-services-test`, `bar-core-test`, `bar-qml-smoke-test`, and
`bar-qml-legacy-guard` passed in that CTest run. No CTest test was Not Run.

## Live qualification

Only read-only qualification was performed; no live scan, Bluetooth pairing,
connection transition, active-network disconnect, audio default change,
volume change, or daemon restart was forced.

- NetworkManager: active.
- bluetooth: active.
- user PipeWire: active.
- user WirePlumber: active.
- NetworkManager `PrimaryConnection`: `/org/freedesktop/NetworkManager/ActiveConnection/2`.
- BlueZ `/org/bluez/hci0`: `Powered=true`, `Discovering=false`.

The live discovery lease and Wi-Fi scan paths were not forced because doing so
would alter the active desktop session. Deterministic production state-machine
tests cover the operation-token races and release semantics.

## Manual audits and limitations

The final production search found no `wpctl`, `pactl`, `pw-cli`, `pw-dump`,
`pw-mon`, `nmcli`, `iw`, `iwctl`, `bluetoothctl`, `python`, `python3`,
Quickshell, `astrea-statusd`, or legacy JSON fallback under `shared/system`.
There is no remaining string-only Wi-Fi power completion, no discovery
completion accepted by kind/generation alone, and no RequestScan callback
without a per-request identity check.

The deterministic tests do not replace a full live D-Bus integration harness.
They do not force real AP transitions, real Bluetooth ownership races against
another client, or live radio toggles. Pairing, Agent1, Wi-Fi SecretAgent,
saved-network editing, VPN, PipeWire feature work, MPRIS, notifications,
System Tray, Control Center, M8-A.3.1, and M8-C remain out of scope.

The codebase-memory coverage check still marks several Qt/D-Bus files as
parse-partial, including parts of the operated service/backend files. Those
ranges were read directly from the source and the final build/tests were run
against the working tree; graph coverage is therefore treated as best-effort,
not as proof of source completeness.

## Protocol references

Implementation assumptions remain aligned with the official
[BlueZ Adapter API](https://bluez.readthedocs.io/en/latest/adapter-api/),
[NetworkManager D-Bus API](https://networkmanager.dev/docs/api/latest/), and
[D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager).
