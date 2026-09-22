# Bluetooth Phase 2 Design

## Scope

Phase 2 extends the existing shared Rust Bluetooth backend with production
pairing, an application-specific BlueZ `Agent1`, authoritative Trust/Untrust,
and authoritative Forget Device operations. It exposes the typed state and
commands needed by a future Settings Bluetooth page while preserving the
current Shell/Bar contract, including the temporary C++ `BluetoothDeviceModel`
and the non-clickable unpaired rows in `BluetoothPopup.qml`.

The existing architecture remains authoritative:

```text
Shell / Bar QML
        |
BluetoothService
thin C++ Qt facade
        |
BluetoothDeviceModel
temporary C++ model
        |
RustBluetoothEngine
CXX-Qt
        |
BluetoothCore
Rust policy/state
        |
BluezClient / Agent1 server
Rust / zbus
        |
system D-Bus / BlueZ
```

No BlueZ policy moves into C++, no JSON crosses the C++/Rust boundary, and no
Settings-specific Bluetooth implementation is introduced.

## Approach

The recommended approach is a focused extension of the Phase 1 state machine
and worker seam:

* `agent.rs` owns typed prompts, request IDs, the bounded response primitive,
  sender authentication, and the concurrent zbus `Agent1` implementation.
* `pairing.rs` owns Pair, CancelPairing, Trust/Untrust, and Forget operation
  identities, deadlines, convergence rules, and result/error state.
* `BluetoothCore` remains the sole authority for object-store eligibility,
  session/BlueZ generation acceptance, snapshots, and operation retirement.
* `BluetoothWorker` owns the system-bus connection, Agent object lifetime,
  lazy registration, action dispatch, separate cancellation/response lanes,
  and per-device scheduling. Pair method futures remain asynchronous and do
  not block unrelated devices; CancelPairing is a dedicated control action.
* The CXX-Qt bridge maps typed Rust snapshot fields into stable Qt properties
  and invokes typed operations. `BluetoothService` forwards those operations
  and emits field-specific notify signals without owning policy.

The alternatives of implementing Agent1 or timeout policy in C++, or adding a
second Settings backend, would duplicate generation and authority rules and
are rejected.

## Agent1 and AgentBroker

The worker exports `/org/astrea/bluetooth/agent` on the same zbus `Connection`
used for BlueZ operations and registers it lazily with
`org.bluez.AgentManager1.RegisterAgent(path, "KeyboardDisplay")`. It never
calls `RequestDefaultAgent`. Registration is tracked by both system connection
session generation and BlueZ unique-owner generation. A replacement connection
or owner invalidates registration and cancels prompts; a future Pair registers
again before invoking `Device1.Pair`.

`AgentBroker` stores a bounded, typed state machine with one interactive
request and one coalesced display state. Interactive requests carry a monotonic
request ID, pairing epoch, device path, and a 60-second deadline. The prompt
variants are `PinCodeInput`, `PasskeyInput`, `PasskeyConfirmation`,
`Authorization`, `ServiceAuthorization`, `DisplayPinCode`, and
`DisplayPasskey`, with typed numeric/text fields rather than JSON or QVariant.
Display requests return immediately and repeated DisplayPasskey calls update
the same display state. A second interactive request is rejected without
replacing the active request.

Each Agent1 method receives its zbus method-call header and compares the sender
with the currently authoritative BlueZ unique owner for the current session and
BlueZ generations. Unauthorized and stale-owner calls are rejected without
publishing a prompt. `Cancel`, `Release`, service stop, D-Bus loss, owner
replacement, and Pair cancellation resolve waiting requests as canceled.
Explicit user rejection resolves as `org.bluez.Error.Rejected`; cancellation
resolves as `org.bluez.Error.Canceled`. A custom zbus `DBusError` preserves
these BlueZ error names. No mutex guard is held across an await, so Cancel and
Release remain dispatchable while an interactive method waits.

## Pairing and device mutations

Pair is a dedicated state machine with one backend-wide active session. Its
identity includes operation ID, pairing epoch, session generation, BlueZ
generation, device path, and a 120-second overall deadline. Eligibility is
checked against the authoritative selected adapter and object projection:
service and owner are active, the adapter exists and is powered, the device is
under that adapter and currently present, the device is unpaired, and no
conflicting session or Forget operation exists.

The action sequence is `RegisterAgent` then `Pair`. A successful Pair method
does not complete the operation until authoritative `Device1.Paired == true`
is observed. A late Pair failure after convergence is ignored. Method failure
before convergence produces pairing-specific error state and does not degrade
global Bluetooth health. Timeout retires the identity, cancels the broker,
issues best-effort `CancelPairing`, and ignores the stale Pair result.

Trust/Untrust uses the typed `Device1.Trusted` property setter and waits for
authoritative convergence to the requested value. It requires a current,
selected-adapter, paired device. Same-device trust intent may coalesce to the
latest operation, but it cannot race Forget.

Forget uses `Adapter1.RemoveDevice(device_path)`. It requires an authoritative
selected-adapter device, remains pending after a successful method call until
the object disappears, and completes on `InterfacesRemoved`/authoritative
projection disappearance. A late method failure after disappearance is
ignored. Forget is terminal for that device until completion or failure and
blocks new Pair, Connect, and Trust requests for the path.

All operations carry finite deadlines and exact identities. Queue saturation
settles the affected identity immediately with a bounded operation result;
ordinary user responses use the broker-owned bounded response primitive rather
than the ordinary Bluetooth command queue.

## Qt-facing contract

The Rust snapshot adds:

```text
pairing
pairingDevicePath
pairingDeviceName
pairingError
agentRequestActive
agentRequestId
agentRequestKind
agentDevicePath
agentDeviceName
agentPasskey
agentEntered
agentServiceUuid
```

The bridge exposes typed invokables equivalent to:

```text
pairDevice(objectPath)
cancelPairing()
setDeviceTrusted(objectPath, trusted)
forgetDevice(objectPath)
submitAgentText(requestId, text)
confirmAgentRequest(requestId, accepted)
rejectAgentRequest(requestId)
```

Every response includes the exact request ID. Stale IDs are ignored. Numeric
passkeys remain numeric in Rust and are formatted as six digits only at the
Qt-facing presentation boundary. PIN input is limited to BlueZ's 1-16
character contract and passkeys to `0..=999999` before resolving an Agent1
method.

The existing properties, `healthJson()` fields, power/discovery projection,
Connect/Disconnect behavior, and device-model ordering remain unchanged.
Authentication secrets are never logged or placed in `healthJson()` or routine
diagnostics.

## Testing and verification

Pure Rust tests cover broker prompts, response races, timeouts, authentication,
generation invalidation, Pair convergence/cancellation, Trust convergence,
Forget removal, eligibility, and stale identities. The existing fake
transport/session seam is extended to record Agent export, registration, Pair,
CancelPairing, per-device ordering, cross-device concurrency, and owner
replacement without host Bluetooth hardware. A deterministic zbus peer test is
used where method headers and server dispatch must be verified.

Qt tests pin new properties, notify signals, stale response handling, command
forwarding, unchanged `healthJson()`, and unchanged Bar-facing behavior. The
existing Bar QML is not modified to exercise pairing.

The repository verification sequence remains the mandated Rust fmt/clippy/test
commands, Rust and QML CI gates, supported CMake tests and CTest, ASan/UBSan,
and workflow-policy tests where applicable. Every compilation output and
temporary build directory is explicitly placed below
`/mnt/Aether/Desktop/GitHub`.

## Roadmap boundary

* **Phase 1:** Rust BlueZ/state-machine migration and lifecycle correctness.
* **Phase 2:** application-specific Agent1, Pair/CancelPairing, Trust/Untrust,
  and Forget Device. This phase exposes the API required by a future Settings
  page but does not change the existing Bluetooth popup UX.
* **Phase 3:** Settings Bluetooth UI.

The Settings Bluetooth page does not exist as part of Phase 2.
