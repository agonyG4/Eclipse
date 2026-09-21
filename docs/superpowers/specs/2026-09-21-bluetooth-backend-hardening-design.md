# Shared Rust Bluetooth Backend Hardening Design

## Goal

Make the approved Rust Bluetooth backend a lossless, recoverable, generation-safe reference system-backend architecture while preserving the current Shell/Bar Qt and QML contracts.

## Scope

This pass covers scan-owner transport, recoverable system D-Bus connection lifecycle, BlueZ-generation task retirement, complete Bluetooth health notifications, and Connect/Disconnect operation identity. It does not add pairing, Agent1, a Settings Bluetooth page, new Bluetooth features, or a C++ Bluetooth model migration.

## Architecture

The Qt facade and temporary C++ `BluetoothDeviceModel` remain compatibility layers over the CXX-Qt `RustBluetoothEngine`, `BluetoothCore`, and `BluezClient`. BlueZ policy and all D-Bus calls remain in Rust; no JSON crosses the Qt/Rust boundary.

### Scan ownership

The worker owns a bounded ordinary command queue for operations that can report submission failure. Scan ownership is transported separately as a coalesced desired state: a mutex-protected session-scoped owner set plus a bounded one-slot wake notification. Every request/release updates the desired set before attempting the wake. The worker reads the latest set and applies it to the authoritative Rust `DiscoveryState`, preserving independent owners and making release durable even when the ordinary queue is full. Stop clears the session's desired owners; reconnects preserve active local demand until an explicit service stop.

### D-Bus lifecycle

The worker uses an explicit stopped/disconnected/connecting/connected lifecycle. It never reconnects while the requested service session is stopped. While running, failed connection setup and terminated/error signal streams transition to disconnected, publish an unavailable snapshot, and retry after bounded delays of 500 ms, 1 s, 2 s, and 5 s maximum. Shutdown and stop notifications are selected alongside reconnect waits so they cancel immediately. Each successful attempt creates a fresh connection, signal match streams, and daemon proxy; each transition invalidates the prior session's tasks and re-probes `GetManagedObjects` after re-resolving the BlueZ owner. Local discovery demand remains in core state and can resume on the new generation.

The production transport wraps zbus. Tests use a deterministic transport/connection abstraction that can script connection failures, stream errors/end, backoff timers, and task completion without a live system bus.

### Generation and task safety

BlueZ owner changes increment the BlueZ generation, clear queued old-generation actions, and drop old-generation in-flight futures before enqueuing the replacement probe. Every task result remains checked against session and BlueZ generation, so stale results cannot publish or mutate replacement state.

### Connect/Disconnect identity

`BluetoothCore` tracks the latest pending operation per device object path. Each operation carries `operation_id`, object path, target connected state, session generation, and BlueZ generation. Replacing an operation on the same device retires the older identity; different device paths remain independent. Matching authoritative `Device1.Connected` state retires the pending operation before any method completion, and late success/failure is ignored. Session stop/restart and BlueZ owner generation changes clear all pending device operations without optimistic public-state changes.

### Qt health notifications

`BluetoothService::applySnapshot` computes one health-change predicate over exactly the fields represented by `healthJson`: state, available, ready, error, adapter availability, powered, scanning, and connected count. It emits `healthChanged` once when any of those values changes, while preserving field-specific notifications. Adapter display-name-only changes remain an `adapterChanged` concern and do not require a health notification.

## Verification

Focused Rust tests cover saturated scan-owner release, multiple owners, connection failure/recovery, connection loss/recovery, signal error/end, stopped reconnect suppression, backoff cancellation, immediate replacement-generation probing, and all Connect/Disconnect identity cases. Qt tests use `QSignalSpy` for every health field class and verify adapter-name-only behavior. Existing Rust, Qt compatibility, CI gates, CMake tests, and supported ASan/UBSan configurations remain required. All build and test output is directed under `/mnt/Aether/Desktop/GitHub`.
