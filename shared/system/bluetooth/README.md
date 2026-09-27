# Shared Bluetooth Architecture

The shared Bluetooth backend keeps BlueZ policy and D-Bus ownership in Rust:

```text
Shell / Bar QML
    -> BluetoothService
    -> thin C++ Qt facade
    -> BluetoothDeviceModel
    -> RustBluetoothEngine (CXX-Qt)
    -> BluetoothCore
    -> BluezClient
    -> system D-Bus / BlueZ
```

## Phases

- Phase 1 is the Rust BlueZ/state-machine migration. It owns the object store,
  discovery and power policy, Connect/Disconnect operations, generations,
  reconnect behavior, and worker ownership.
- Phase 2 adds an application-specific `org.bluez.Agent1` at
  `/org/astrea/bluetooth/agent`, lazy `AgentManager1.RegisterAgent` with the
  `KeyboardDisplay` capability, Pair/CancelPairing, authoritative Trust and
  Untrust, and authoritative Forget Device. The Agent is never made the
  system-wide default agent.
- Phase 3 is the Settings Bluetooth page under `Settings/qml/pages/system`.
  It consumes the typed state and bounded response operations from Phase 2.

## Display identity

Bluetooth operations continue to use the BlueZ object path, address,
generation, request identity, and the existing Rust state-machine ownership.
Names and device kinds are presentation metadata; they never identify an
operation, authorize a request, or change pairing behavior.

BlueZ `Alias` and `Name` are retained as raw presentation evidence in the Rust
device projection. Astrea resolves that evidence into a concrete
`display_name` and a semantic `BluetoothDeviceKind`. The Qt device model keeps
its `name` role for the resolved concrete display name and adds a `deviceKind`
role. Rust owns semantic classification, `BluetoothDeviceModel` transports the
kind, and QML presentation surfaces own user-facing fallback copy. Settings
localizes its fallbacks, while the Shell Bar uses matching English copy in its
existing QML presentation. Raw aliases, remote names, addresses, and object
paths are not sent to QML as the primary display name.

Resolution is conservative. A non-empty Alias that differs from a non-empty
Name is treated as a user alias and always wins, including when it resembles a
technical token. When Alias and Name are equal, the value is treated as the
ordinary remote name; when Alias is absent, Name is the candidate. Empty and
generic labels, MAC addresses, BlueZ `dev_` address tokens, and the current
device's own path or address are unresolved. A narrow `LE_` or `BLE_` prefix is
removed only when the remainder is a useful name, so names such as
`LE_WH-1000XM3` become `WH-1000XM3` while `LE_4A8C19FE` stays unresolved.
Model-like names, punctuation, capitalization, and ordinary Unicode are
preserved. The resolver trims and sanitizes control characters for one-line
display while retaining the raw BlueZ values internally.

`BluetoothDeviceKind` comes from exact BlueZ `Icon` values. Technical object
identity is separate from presentation identity: Bluetooth operations remain
keyed by object path, while Settings and the Shell Bar present resolved names
or kind-based fallbacks. Classification does not affect pairing,
authorization, Trust, cancellation, deadlines, generations, or stale-response
handling. The resolver does not probe devices, read GATT services, spawn
metadata lookup processes, or consult a product database. Unknown and
unnamed devices remain visible and pairable.

## Agent and pairing ownership

The Agent object is exported on the same system-bus connection as the BlueZ
client. Registration is qualified by the system-bus connection generation and
the current BlueZ unique-owner generation. An owner replacement, connection
loss, or service stop invalidates registration and cancels pending prompts.

`AgentBroker` authenticates every Agent callback against the current BlueZ
unique owner and generation. It supports one interactive prompt at a time,
bounded prompt waiting, monotonic request IDs, typed PIN/passkey/
authorization state, coalesced display-only updates, and a dedicated bounded
response channel. Agent methods remain concurrently dispatchable so BlueZ can
call `Cancel` while another method is waiting for the future Settings UI.

Pairing is a dedicated Rust operation with a 120-second overall deadline and a
60-second interactive prompt deadline. `Device1.Paired` remains authoritative;
the backend never optimistically marks a device paired. User rejection,
cancellation, authentication failure, and timeout are pairing/device results,
not global Bluetooth service-health failures.

Trust and Untrust wait for authoritative `Device1.Trusted` convergence. Forget
uses the selected adapter's `Adapter1.RemoveDevice` and waits for authoritative
device disappearance. These operations are generation- and identity-qualified
and participate in per-device scheduling without globally serializing unrelated
devices.

The C++ layer only maps typed state, exposes Qt properties and invokables, and
emits notify signals. It does not implement BlueZ calls, policy, deadlines,
generation checks, validation, or stale-response handling. The existing Bar
popup behavior is unchanged: paired rows retain Connect/Disconnect behavior,
unpaired rows remain non-clickable, and scan, power, RSSI, and battery
projection remain as before.
