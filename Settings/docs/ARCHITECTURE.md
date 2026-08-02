# Astrea Settings Foundation Architecture

`astrea-settings` is a native Qt 6 application and a normal Wayland toplevel.
C++ owns startup and navigation state, while QML owns presentation. The
application has no concrete settings page or system backend.

## Runtime structure

```text
QGuiApplication
  -> SettingsApplication
      -> SettingsController
          -> SettingsNavigationModel
      -> AstreaIconTheme / AstreaIconProvider
      -> QQmlApplicationEngine
          -> Main.qml
              -> AppShell
                  -> WindowTitleBar
                  -> SettingsSidebar
                  -> EmptyContent
```

## Controller contract

`SettingsController` is registered as the `SettingsController` QML context
property. Its public properties are:

| Property | Type | Semantics |
| --- | --- | --- |
| `navigationModel` | `SettingsNavigationModel *` | Constant model used by the sidebar. |
| `selectedSectionId` | `QString` | Stable selected navigation ID; initially `system`. |
| `selectedSectionTitle` | `QString` | Title for the selected stable ID. |
| `filterText` | `QString` | Trimmed search text currently applied to the model. |
| `userName` | `QString` | `USER`, then `LOGNAME`, then `User`. |
| `avatarUrl` | `QUrl` | Local AccountsService icon only when it is a readable file. |
| `pagesAvailable` | `bool` | Always `false` until a separately designed page boundary exists. |

Its invokables are:

| Invokable | Result | Semantics |
| --- | --- | --- |
| `selectSection(id)` | `bool` | Selects a known enabled item; rejects unknown and spacer IDs without changing state. |
| `setFilterText(text)` | `void` | Applies case-insensitive filtering against ID, title, and subtitle. |
| `clearFilter()` | `void` | Restores the complete catalogue and clears `filterText`. |

The model exposes `entryId`, `title`, `subtitle`, `iconName`, `kind`,
`entryEnabled`, and `selected` roles. Its catalogue contains the stable IDs
`system`, `software-update`, `internet`, `bluetooth`, `audio`, `performance`,
`appearance`, and `more-settings`, plus one non-selectable spacer between
`audio` and `performance`.

## Ownership boundaries

- `app/` owns metadata, icon initialization, QML engine startup, context-property registration, and fatal root validation.
- `core/` owns only navigation, filtering, and display-only account metadata.
- `qml/theme/` contains static tokens and in-memory dark/light palette values; it does not persist or detect theme state.
- `qml/components/` owns window composition and navigation presentation.
- `qml/ui/` contains page-agnostic controls for future designs.

Navigation IDs are not page URLs. No navigation role contains a URL or page
loading policy, and the placeholder content has no `Loader`.

## Explicit exclusions

This target contains no settings pages, persistence, service or system
backends, configuration mutation, shell commands, `Process`, Quickshell,
LayerShellQt, Hyprland protocol, or Typhon-private protocol. Future pages must
receive typed backend objects through independently reviewed boundaries rather
than extending this foundation with command execution.
