# Astrea Settings Architecture

`astrea-settings` is a native Qt 6 application and a normal frameless Wayland
toplevel. C++ owns lifecycle and system-facing state. QML owns the approved
Astrea Settings presentation and interaction.

## Composition Root

`SettingsApplication` is the only composition root:

```text
AdminGroupDetector
  -> SettingsUserProfileProvider
      -> SettingsUserProfile
SettingsNavigationCatalog
  -> SettingsNavigationModel
SettingsIconResolver
  -> SettingsController
ThemeController
SettingsTranslationController
SettingsDockController -> shared DockConfigStore
QQmlApplicationEngine
```

The application registers the stable context properties `SettingsController`,
`ThemeController`, and `I18n`, plus the existing `astrea-icon` image provider.
The provider is owned by the QML engine and is not duplicated as a context
property. It also owns metadata, QML startup, fatal warning reporting, and
exactly-one-root validation.

## Target Boundaries

`astrea-settings-core` is a reusable static native library. Its public link
interface is `Qt6::Core` and `astrea-shared-dock`; `Qt6::Network` is private.
The core contains the controller, navigation, services, and Linux account
implementation, and includes the header-only Paper protocol directly. It does
not link Qt QML, Qt Quick, Quick Controls, LayerShellQt, or a compositor
library.

`astrea-settings-ui` is the only `Astrea.Settings 1.0` QML module. The
application and QML integration tests consume that same target and generated
plugin. The application additionally links the existing shared core and QML
plugin for compositor-independent shared utilities.

Unit tests link `astrea-settings-core`. Integration tests link both reusable
production targets. No test target lists a production `.cpp` file owned by the
core library.

## Dependency Direction

```text
app -> core -> services -> platform/linux
app -> qml context properties
qml -> presentation and interaction
tests -> production targets and explicit fakes
shared -> compositor-independent shared utilities
```

QML has no filesystem, process, IPC, DBus, or compositor API. Platform access
is implemented in focused C++ services and platform classes.

## Navigation and Routing

`SettingsNavigationCatalog` owns the ordered descriptors, stable IDs, visible
metadata, Page/Section/Child/Spacer kind, section relationships, and optional
`pageSource`. The model copies that catalogue and owns selection and section
expansion state. A row is selectable only when it is enabled, is a Page or
Child, and has a non-empty `pageSource`. `SettingsController` exposes
`selectedPageSource`, derived from the selected descriptor.

`Main.qml` passes the native URL directly to a `Loader`. The catalogue is the
single source of truth for row order and page routing. There is no numeric page
index and no QML route-ID condition. The current routable descriptors are:

```text
qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml
```

Rows without implemented pages remain visible with an empty URL but cannot be
selected. The first routable descriptor selects Compositor at startup. Sections
toggle expansion by stable ID and never become selected; Wallpaper and Dock are
children of Appearance, and a selected child remains visible while its section
is collapsed. Leaving Compositor destroys the page and recreates its local
preview state when selected again.

## Native Ownership

`SettingsController` is the stable QML facade. It delegates navigation to
`SettingsNavigationModel`, profile values to an immutable `SettingsUserProfile`,
resource URL construction to `SettingsIconResolver`, and Dock personalization to
the focused `SettingsDockController`. The latter exposes typed validated
properties, debounced atomic writes, bounded errors, and a file watcher for
external replacement. It depends only on the shared compositor-independent
Dock configuration boundary; it does not reach into Shell, Typhon, or
LayerShellQt.

`SettingsUserProfileProvider` resolves the current username, the readable
AccountsService avatar path, and administrative membership. `AdminGroupDetector`
owns libc/NSS enumeration; `AdministrativeGroupPolicy` recognizes exactly
`wheel` and `sudo`.

`ThemeController` and `SettingsTranslationController` retain their existing
public QML names and semantics, but live under their service ownership paths.

## Exclusions

Settings has no Quickshell import, LayerShellQt dependency, Hyprland command,
Typhon-private protocol, compositor backend, IPC boundary, persistence for the
Compositor preview, or shell command execution. The Dock page is a native route
under the Appearance section and its preview is presentation-only; it does not
import resident Dock QML or implement a second schema. Performance, Appearance,
and More Settings are non-selectable sections.
