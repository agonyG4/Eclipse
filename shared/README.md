# Astrea Shared Targets

The shared foundation is split by runtime ownership:

- `astrea-shared-core` contains compositor-independent icons, desktop-entry
  catalogue, launcher helpers, and the `Astrea.Shared` QML module;
- `astrea-shared-layer-shell` contains `LayerShellHelper` and owns the
  LayerShellQt/Wayland links, the compositor protocol probe, and the
  compile-time capability flag.

Settings links only the core target and its QML plugin. Dock, Spotlight, and
AltTab link the layer-shell target because they configure Layer Shell surfaces.
The core target has no LayerShellQt headers, types, or transitive link
dependency. Production unified-shell builds require LayerShellQt; the
dependency-free helper stub is available only when the explicit
`ASTREA_ENABLE_LAYER_SHELL=OFF` development/test mode is selected.

astrea-shared-system is a separate native-service target. Audio and Network
remain on their existing C++ adapters. Bluetooth is implemented in incremental
phases:

- Phase 1 is the Rust BlueZ/state-machine migration. The reusable
  `shared/backend` crate owns BlueZ D-Bus communication, Bluetooth object
  tracking, operations, deadlines, generation handling, and state policy.
- Phase 2 adds an application-specific BlueZ Agent1, Pair/CancelPairing,
  authoritative Trust/Untrust, and authoritative Forget Device operations. The
  typed Qt compatibility contract exposes the state and bounded user-response
  operations required by a future Settings page. It does not change the
  existing Bluetooth popup UX: the current Shell and Bar behavior, including
  non-clickable unpaired rows, remains unchanged.
- Phase 3 will build the Settings Bluetooth UI on this shared contract. The
  Settings Bluetooth page does not exist in Phase 2.

`BluetoothService` remains a thin Qt compatibility facade, and
`BluetoothDeviceModel` remains C++ so the current Shell and Bar contract stays
stable. The core target remains free of PipeWire and D-Bus dependencies.
ShellRuntime owns one instance of each service and injects those instances
into the TopBar through BarSurfaceBundle initial properties; QML contains
presentation and input forwarding only.
