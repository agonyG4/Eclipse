# AltTab Architecture

## Process boundary

Alt+Tab is a resident native Qt 6 process. Typhon owns physical shortcut
matching and dispatches lifecycle events to the daemon. Thin CLI invocations
remain an IPC compatibility path.

```
Typhon astrea-shell shortcut ────────────────→ AltTabApplication
CLI compatibility caller → local IPC (astrea-alt-tab-v1) ───┘
                                                ↓
                                         AltTabApplication
                                                |
                                ┌───────────────┼────────────────┬──────────────┐
                                ▼               ▼                ▼              ▼
                         AltTabController  WindowSource   AppIdentityResolver  TyphonShortcutClient
                                |           (Interface)    (async icon/name)
                                ▼               ▼
                         AltTabWindowModel   HyprlandWindowSource
                         (QAbstractListModel) (HyprlandSocket IPC)
                                |
                                ▼
                          QML (presentation only)
                                |
                                ▼
                           required LayerShellQt overlay (astrea-alt-tab)
```

## Directory structure

```
AltTab/
  CMakeLists.txt         - Build system
  app/                   - Process bootstrap, CLI routing, dependency wiring
  core/                  - Controller, model, state machine, window info
  services/              - Identity resolution, config watching
  platform/
    hyprland/            - Hyprland socket transport and window source
    ipc/                 - Resident IPC server (astrea-alt-tab-v1)
    icons/               - XDG icon theme resolution and image provider
    wayland/             - LayerShell surface configuration
    runtime/             - XDG and environment-based paths
  qml/                   - Presentation-only QML
  tests/                 - Native unit/integration tests
  packaging/             - systemd unit and Hyprland config snippet
  docs/                  - Architecture and integration documentation
```

## Ownership rules

- `AltTabApplication`: Owns the app lifecycle, commands, IPC, QML engine, and wiring
- `AltTabController`: Owns the state machine (Hidden/Opening/Open/Committing/Closing),
  selection, model coordination, commit/cancel behavior
- `AltTabWindowModel`: Owns QAbstractListModel boundary, stable keyed updates
- `WindowSource`: Abstract compositor interface, isolate Hyprland details
- `AppIdentityResolver`: Maps window metadata to display name, icon name/path
- QML: Renders state and forwards user intent only

The production unified shell requires LayerShellQt and the Qt Wayland platform;
there is no ordinary-window fallback when the overlay cannot be configured.

Window icons use the shared `AstreaAppIcon` and `AstreaIconProvider` pipeline.
The delegate keeps a maximum logical source extent of 84 pixels for both
selected and unselected states. Selection changes the visible 72-to-84 pixel
presentation animation, while the provider request remains based on the
effective DPR and does not reload just because selection changed.

Named representation selection is delegated to `QIcon::pixmap()`; the shared
provider does not reconstruct theme `Scale`, threshold, scalable-directory, or
inheritance rules from `availableSizes()`. The same shared theme boundary
preserves user `.icons`, XDG, Flatpak, and existing Qt search roots with
deterministic deduplication.
