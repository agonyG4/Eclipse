# Astrea Settings Foundation

`astrea-settings` is Eclipse's native Qt 6 Settings application. It preserves
the approved Astrea Settings presentation while keeping lifecycle, navigation,
theme configuration, translations, account metadata, and startup validation in
focused native boundaries.

## Build

From the Eclipse checkout:

```bash
cmake -S . -B build-settings -G Ninja \
  -DASTREA_BUILD_TESTS=ON \
  -DASTREA_SETTINGS_BUILD_TESTS=ON
cmake --build build-settings --parallel
ctest --test-dir build-settings --output-on-failure
```

The executable is `Settings/app/astrea-settings` in the build tree and installs
as `bin/astrea-settings`. The desktop entry installs under
`share/applications`.

## Current Scope

The native application includes:

- a normal frameless Qt Wayland window with native window actions;
- the source-preserved legacy glass shell, profile composition, and sidebar;
- catalogue-owned navigation with filtering and stable selection;
- thirteen navigation rows including the non-selectable spacer;
- native theme configuration, translations, icon resolution, and user-profile
  services; Linux libc/NSS administrative-group detection recognizes only
  `wheel` and `sudo`;
- two real page routes: `Compositor`, immediately after `Services`, and the
  native `Wallpaper` route under Appearance;
- reusable form controls and one reusable `Astrea.Settings` QML module.

The Compositor page is a visual-only preview. Its toggles and selectors use
page-local QML properties. Those values are destroyed when the page is left or
the application closes; they are never persisted, applied, or sent to a
backend.

The Wallpaper page is a native route backed by
`SettingsWallpaperController` and the Paper wallpaper service. Paper owns
catalog validation, managed files, metadata, persistence, and authoritative
selection; QML owns only presentation and native file/name dialogs.

The Compositor preview intentionally has no compositor integration. Settings
does not use shell commands, Quickshell, LayerShellQt, Hyprland, or Typhon.
Existing ThemeController configuration is separate from the Compositor preview
state.

Settings links `astrea-shared-core` and its QML plugin only for
compositor-independent shared utilities. Layer Shell code is isolated in
`astrea-shared-layer-shell`, which is linked only by Dock, Spotlight, and
AltTab. Administrative-group detection does not spawn subprocesses.

Use `tools/create-source-archive` for source handoff. It archives committed Git
content with an `Eclipse/` prefix and never zips the checkout filesystem.

See `AGENTS.md` for non-negotiable contribution rules, `docs/STRUCTURE.md` for
ownership and extension points, `docs/COMPONENT_CATALOG.md` for registered QML
types, `docs/MIGRATION_NOTES.md` for source-preserving decisions, and
`docs/TESTING.md` for verification procedures.
