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
- Phase 3 will add the Settings Bluetooth UI. Phase 2 exposes its typed state
  and bounded response operations, but does not create that page.

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
