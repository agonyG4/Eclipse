# Settings Agent Rules

- Work directly on `main`; do not create a branch, worktree, or temporary branch.
- Preserve the source-approved legacy Settings design and current visuals.
- QML owns presentation and interaction. Rust Appearance owns mutation
  semantics for `theme_preference`, `accent`, and `icon_appearance`; Rust Icons
  owns installed icon themes and `system_icon_theme`. Both use the shared Rust
  `ThemeConfigStore` for `theme.json` transactions through CXX-Qt boundaries.
- C++ owns application composition and the live ThemeController
  reader/watcher/projection. ThemeController temporarily writes only remaining
  legacy fields (`shell_style`, `icon_style`, `icon_theme`, and
  `audio_osd_style`); it must preserve all Rust-owned Appearance and icon-theme
  fields plus unknown configuration keys. `shell_style` is the transitional
  Visual Effects setting until Phase 2.
- QML owns presentation and interaction.
- Do not put shell commands, filesystem access, process access, IPC, or DBus in QML.
- Do not add Quickshell, LayerShellQt, Hyprland, or Typhon-private runtime dependencies.
- New pages use stable IDs and native route descriptors, never numeric page indexes.
- The sidebar is flat and exposes exactly the catalogue's sidebar-visible rows;
  nested destinations use stable `parentId` metadata and are reached through a
  generic Hub page.
- `SettingsController` owns current destination, sidebar ancestor highlight,
  selected page source, and bounded session-only Back/Forward history. Do not
  reintroduce model-owned selection or expansion state.
- Keep unavailable hubs visible as disabled catalogue entries; do not create
  placeholder pages or hardcode the first route in QML.
- Search/filter UI is deferred until the navigation contract is stable.
- Future page backends belong in focused service and platform boundaries.
- Customization routes are ordered Appearance, Visual Effects, Icons,
  Wallpaper, Dock, and Animations with stable IDs `appearance`,
  `visual-effects`, `icons`, `wallpaper`, `dock`, and `animations`.
- Phase 2 migrates the material/effects control plane. Continuous material
  sliders, advanced overrides, compositor blur controls, shaders, refraction,
  noise, and Typhon effects integration are not Phase 1 capabilities.
- Tests link reusable production targets instead of compiling production sources again.
- Do not include stale build artifacts in source archives; use `tools/create-source-archive`.
- Proceed independently and ask only about real blockers or conflicting requirements.
