# Settings Agent Rules

- Work directly on `main`; do not create a branch, worktree, or temporary branch.
- Preserve the source-approved legacy Settings design and current visuals.
- C++ owns application composition and remaining unmigrated system access,
  persistence, models, services, and backends; migrated backends such as
  Animations/Typhon use Rust behind the thin CXX-Qt boundary.
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
- Tests link reusable production targets instead of compiling production sources again.
- Do not include stale build artifacts in source archives; use `tools/create-source-archive`.
- Proceed independently and ask only about real blockers or conflicting requirements.
