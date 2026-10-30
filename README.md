# Eclipse
AstreaOS desktop components

Eclipse contains the native desktop shell and the Settings application. Its
components are organized around feature ownership and shared runtime
foundations:

- `Shell/` composes the resident session, native surfaces, IPC, and screenshot
  surface lifecycle.
- `Bar/`, `Dock/`, `Spotlight/`, and `AltTab/` provide the top bar, pinned
  applications, launcher, and window switching.
- `Settings/` provides the system settings UI and its Rust and Qt service
  integrations.
- `Paper/` owns wallpaper catalog and surface behavior.
- `shared/` provides cross-component application, platform, system, theme, and
  Typhon SDK foundations.

See [the component structure guide](docs/COMPONENT_STRUCTURE.md), the
[shared SDK architecture](shared/docs/SDK_ARCHITECTURE_V1.md), and the
[theme package guide](docs/THEME_PACKAGES.md) for ownership and extension
contracts.

Production `astrea-shell` builds require Qt 6.8+, LayerShellQt 6.4.5+ and its
`LayerShellQt::Interface` CMake target, plus Wayland client development files.
Non-standard installations are supplied with normal CMake package search
paths, for example `-DCMAKE_PREFIX_PATH=/path/to/layer-shell-qt-prefix` or
`-DLayerShellQt_DIR=/path/to/lib/cmake/LayerShellQt`. The explicit
`-DASTREA_ENABLE_LAYER_SHELL=OFF` mode is reserved for isolated development
and tests; Dock, AltTab, and Spotlight never fall back to ordinary Qt windows
at runtime.

Dock Stage 1 is always-visible when enabled, bottom-centered, and Layer Shell
work-area reserving. It displays configured pins and launches through
`astrea-launch`. Typhon provides compositor window and workspace protocols;
Dock intentionally does not simulate running, active, or minimized state.
