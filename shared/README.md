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

astrea-shared-system is a separate native-service target. It owns the
PipeWire, NetworkManager D-Bus, and BlueZ D-Bus adapters plus the typed
Audio, Network, and Bluetooth Qt services and models. The core target remains
free of PipeWire and D-Bus dependencies. ShellRuntime owns one instance of
each service and injects those instances into the TopBar through
BarSurfaceBundle initial properties; QML contains presentation and input
forwarding only.
