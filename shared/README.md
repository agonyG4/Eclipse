# Astrea Shared Targets

The shared foundation is split by runtime ownership:

- `astrea-shared-core` contains compositor-independent icons, desktop-entry
  catalogue, launcher helpers, and the `Astrea.Shared` QML module;
- `astrea-shared-layer-shell` contains `LayerShellHelper` and owns the optional
  LayerShellQt link and compile-time availability flag.

Settings links only the core target and its QML plugin. Dock, Spotlight, and
AltTab link the layer-shell target because they configure Layer Shell surfaces.
The core target has no LayerShellQt headers, types, or transitive link
dependency and remains buildable when LayerShellQt is unavailable.
