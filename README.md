# Eclipse
AstreaOS Shell

Native components:

- `Spotlight/` provides the native Qt Quick launcher/search surface.
- `AltTab/` provides native window switching and identity resolution.
- `Dock/` provides the resident Qt Quick pinned-application Dock foundation.
- `shared/` provides the supervised application launcher, desktop-entry
  catalog, icon provider, and Layer Shell helper.

Production `astrea-shell` builds require Qt 6.8+, LayerShellQt 6.4.5+ and its
`LayerShellQt::Interface` CMake target, plus Wayland client development files.
Non-standard
installations are supplied with normal CMake package search paths, for
example `-DCMAKE_PREFIX_PATH=/path/to/layer-shell-qt-prefix` or
`-DLayerShellQt_DIR=/path/to/lib/cmake/LayerShellQt`. The explicit
`-DASTREA_ENABLE_LAYER_SHELL=OFF` mode is reserved for isolated development
and tests; Dock, AltTab, and Spotlight never fall back to ordinary Qt windows
at runtime.

Dock Stage 1 is always-visible when enabled, bottom-centered, and Layer Shell
work-area reserving. It displays configured pins and launches through
`astrea-launch`. Typhon does not currently expose a public window-management
protocol, so running, active, and minimized Dock state is intentionally not
reported or simulated.
