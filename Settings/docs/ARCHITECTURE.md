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

`astrea-settings-ui` is the only `Astrea.Settings 1.0` QML module and registers
40 QML files. The application and QML integration tests consume that same
target and generated plugin. The application additionally links the existing
shared core and QML plugin for compositor-independent shared utilities.

The QML component taxonomy separates reusable interaction from page composition:
`components/controls/` owns interactive primitives, while `components/form/`
owns Settings-specific structural/layout composition. A semantic Qt Quick
Controls type is the preferred base for a new interactive primitive; replace
its visual delegates and retain the framework's pointer, keyboard, touch,
focus, range, and RTL behavior instead of implementing a second input state
machine with `MouseArea`.

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

`SettingsNavigationCatalog` owns the authoritative descriptor catalogue:
stable IDs, visible metadata, Page/Hub/Spacer kind, parent relationships, and
optional `pageSource`. The model exposes only sidebar-visible rows while also
providing native lookup, child, ancestor, and first-destination helpers. A
destination is navigable only when it is enabled and has a valid route; a Hub
also requires at least one navigable child. `SettingsController` owns the
current destination, sidebar highlight, selected route, and session history.

`Main.qml` passes the native URL directly to a `Loader`. The catalogue is the
single source of truth for row order and page routing. There is no numeric page
index and no QML route-ID condition. The current routable descriptors are:

```text
qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml
```

Rows without implemented pages remain visible but cannot be selected. The first
navigable sidebar destination is supplied by the catalogue and is currently
Compositor. The sidebar is flat: `Customization` remains highlighted while its
nested Wallpaper or Dock destination is open. Hub pages render their children
from controller metadata, and the controller maintains a bounded 64-entry
Back/Forward history without duplicate current entries. Leaving Compositor
destroys the page and recreates its local preview state when selected again.

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
under the Customization hub and its preview is presentation-only; it does not
import resident Dock QML or implement a second schema. Performance and More
Settings are visible but unavailable hubs until they receive navigable children.
