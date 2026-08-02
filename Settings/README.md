# Astrea Settings Foundation

`astrea-settings` is Eclipse's native Qt 6 Settings application. It preserves
the approved Astrea Settings presentation while moving lifecycle, navigation,
theme configuration, translations, account metadata, and startup validation to
native C++ boundaries.

## Build

From the Eclipse checkout:

```bash
cmake -S . -B build-settings -G Ninja \
  -DASTREA_BUILD_TESTS=ON \
  -DASTREA_SETTINGS_BUILD_TESTS=ON
cmake --build build-settings --parallel
ctest --test-dir build-settings --output-on-failure
```

The executable is `Settings/astrea-settings` in the build tree and installs as
`bin/astrea-settings`. The desktop entry installs under
`share/applications`.

## Current Scope

The native application includes:

- a normal frameless Qt Wayland window with native window actions;
- the source-preserved legacy glass shell, profile composition, and sidebar;
- model-owned navigation with filtering and stable selection;
- twelve navigation rows including the non-selectable spacer;
- native theme configuration, translations, icon resolution, and
  wheel/sudo administrative-group detection;
- one real page route, `Compositor`, immediately after `Services`;
- reusable form controls and a complete registered QML module.

The Compositor page is a visual-only preview. Its toggles and selectors use
page-local QML properties. Those values are destroyed when the page is left or
the application closes; they are never persisted, applied, or sent to a
backend.

The application intentionally has no compositor integration, IPC, private
protocol, service backend, shell command, Quickshell, LayerShellQt, Hyprland,
or Typhon runtime dependency. Existing ThemeController configuration is
separate from the Compositor preview state.

See `docs/ARCHITECTURE.md` for ownership and routing, `docs/MIGRATION_NOTES.md`
for source-preserving migration decisions, and `docs/TESTING.md` for
verification procedures.
