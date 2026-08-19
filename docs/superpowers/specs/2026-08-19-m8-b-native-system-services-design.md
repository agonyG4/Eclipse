# M8-B Native System Services Core — Design

## Approval context

This design implements the attached M8-B milestone brief. The brief is the
design authority and explicitly removes the need for an additional interactive
clarification round.

## Goal

Replace the legacy command/process-backed desktop service paths with one
native, typed, asynchronous system-services layer. The first consumers are the
native shell runtime and the TopBar status surface. The layer must remain
usable when PipeWire, NetworkManager, or BlueZ is absent or restarts.

## Architecture

`shared/system` is a separate library. Each service owns a replaceable backend
through a small typed interface and is the Qt-thread authority for its
properties, models, timers, and lifecycle:

- `AudioService` and `AudioOutputModel`, backed in production by
  `PipeWireAudioBackend`.
- `NetworkService` and `WifiNetworkModel`, backed in production by
  `NetworkManagerBackend`.
- `BluetoothService` and `BluetoothDeviceModel`, backed in production by
  `BluezBackend`.
- `SystemServiceState` is shared by all three services and exposes
  `Stopped`, `Starting`, `Ready`, `Unavailable`, and `Degraded` without
  stringly-typed state in QML.

Native backends use PipeWire's C API or Qt D-Bus. They never invoke shell
commands, parse human-readable CLI output, or mutate QML objects from native
callbacks. Backend events are immutable typed snapshots delivered to the
service on its owning Qt thread. Start and stop are idempotent, restartable,
and teardown detaches listeners, timers, subscriptions, and pending work.

The services expose only QML-safe properties, signals, models, and actions.
Fake backends implement the same interfaces for deterministic unit tests;
production services do not special-case test data.

## Runtime ownership and lifecycle

`ShellRuntime` constructs exactly one instance of each service during
`initialize()`. `start()` starts them in a deterministic order and `stop()`
stops them in reverse order before the runtime is reusable. Missing daemons are
reported as an unavailable/degraded service, not an initialization failure.

`AstreaShellApplication` adds read-only `system.audio`,
` system.network`, and `system.bluetooth` health objects to its existing
status JSON. No credentials, network secrets, or device-private data are
included.

## TopBar integration

`BarSurfaceManager` and `BarSurfaceBundle` receive non-owning service pointers
from the runtime. Each output surface gets the same runtime service instances
through QML initial properties. `StatusSurface.qml` remains a compact native
surface containing the existing clock plus:

- `NetworkIndicator` — disconnected/Wi-Fi/wired state and connection label.
- `BluetoothIndicator` — availability, power, connection count, and a bounded
  scan pulse.
- `VolumeIndicator` — cubic UI volume scale, mute state, and wheel adjustment
  through `AudioService::adjustVolume()`.

The indicators contain presentation only. They do not access D-Bus, PipeWire,
processes, filesystem counters, or legacy status JSON. Existing TopBar
geometry, palette, popup ownership, and multi-output lifecycle stay unchanged.

## Model contracts

The three models are stable `QAbstractListModel` contracts with explicit roles:

- Audio: node id, stable name/description/nick, media class, default, virtual.
- Wi-Fi: SSID, strength, active, secured, frequency, BSSID.
- Bluetooth: stable id/path/address, name, paired, trusted, connected,
  discovered, icon, RSSI, and battery percentage.

Models sort deterministically and avoid reset notifications when the semantic
content has not changed. Network rows are deduplicated by SSID; Bluetooth
rows are reconciled by object path; audio default selection follows the native
metadata default rather than registry order.

## Native backend boundaries

PipeWire uses a managed thread loop and registry/metadata/node listeners.
Volume sent to the wire remains linear; UI percentages use the specified
cubic mapping and clamp to 0–150. PipeWire callbacks copy data and queue it
to the service thread; teardown synchronizes listener removal before object
destruction.

NetworkManager and BlueZ use Qt's asynchronous system-bus APIs, service
ownership watchers, ObjectManager signals, and PropertiesChanged updates.
Network traffic counters are sampled by a service-owned timer from kernel
statistics paths; they are not a substitute for NetworkManager state. Wi-Fi
scan requests are rate-limited/coalesced. Bluetooth scan ownership is a
reference-counted owner set, and pairing/agent/password flows are explicitly
out of scope.

## Build and verification

`astrea-shared-core` remains free of system-service dependencies. The new
`astrea-shared-system` target links Qt Core/DBus and PipeWire and is consumed
by the shell runtime. CI installs the PipeWire development package. Tests
cover conversion helpers, model ordering/deduplication/no-op updates,
service lifecycle/restart/error recovery, QML indicator contracts, runtime
ownership, and the legacy-token guard over all relevant production roots.

Live qualification is read-only and best-effort. If a desktop daemon is not
available in the test environment, the report records the skipped observation
and the deterministic unavailable-state evidence instead of mutating the
session.

## Non-goals

This milestone does not add a system tray, Control Center, full settings
panels, OSDs, MPRIS, Wi-Fi password flows, Bluetooth pairing/agent flows, or
settings migration. It does not alter Paper, Dock, Spotlight, AltTab, or
existing TopBar behavior outside the new indicators and injected services.
