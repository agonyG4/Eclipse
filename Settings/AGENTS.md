# Settings Agent Rules

- Work directly on `main`; do not create a branch, worktree, or temporary branch.
- Preserve the source-approved legacy Settings design and current visuals.
- C++ owns system access, persistence, models, services, and backends.
- QML owns presentation and interaction.
- Do not put shell commands, filesystem access, process access, IPC, or DBus in QML.
- Do not add Quickshell, LayerShellQt, Hyprland, or Typhon-private runtime dependencies.
- New pages use stable IDs and native route descriptors, never numeric page indexes.
- Future page backends belong in focused service and platform boundaries.
- Tests link reusable production targets instead of compiling production sources again.
- Do not include stale build artifacts in source archives; use `tools/create-source-archive`.
- Proceed independently and ask only about real blockers or conflicting requirements.
