# Astrea Settings Architecture

`astrea-settings` is a native Qt 6 application and a normal frameless Wayland
toplevel. C++ owns lifecycle and application-facing state. QML owns the
source-preserved Astrea Settings presentation and interaction.

## Runtime Structure

```text
QGuiApplication
  -> SettingsApplication
      -> SettingsController
          -> SettingsNavigationModel
      -> SettingsTranslationController
      -> ThemeController
      -> AstreaIconTheme / AstreaIconProvider
      -> QQmlApplicationEngine
          -> Main.qml
              -> Sidebar
              -> Loader
                  -> pages/system/Compositor.qml
```

`Main.qml` maps the selected navigation ID `compositor` to the registered
Compositor page source. The Loader is inactive for routes that do not yet have
a page. Leaving Compositor destroys the page; selecting it again creates fresh
page-local preview values.

## Native Ownership

`app/` owns application metadata, icon initialization, QML engine startup,
context-property registration, fatal QML warning reporting, and the startup
check that exactly one root object was created.

`SettingsController` owns navigation selection and filtering, display-only
account metadata, AccountsService avatar resolution, icon URL resolution, and
native membership detection for the `wheel` and `sudo` groups. Lookup failure
is treated as no administrative membership.

`SettingsNavigationModel` owns the catalogue, stable IDs, row roles, filtering,
and selected-row state. Its current catalogue order is:

```text
System, Software Update, Internet, Bluetooth, Audio, Components, Services,
Compositor, spacer, Performance, Appearance, More Settings
```

`SettingsTranslationController` loads the bundled English messages and exposes
translation lookup to QML. `ThemeController` owns the existing shell/theme
configuration boundary used by the approved presentation.

## QML Ownership

`qml/components/` owns the window shell, profile, sidebar, navigation item, and
page-agnostic form controls. The registered module contains 35 QML files,
including the five singleton sources `Tokens`, `Apps`, `Shell`, `State`, and
`Theme`.

`qml/pages/system/Compositor.qml` is the first real page route. It uses only
the existing Settings controls and Theme tokens. Every compositor preview value
is local to that page. The page has no backend object and does not access
configuration files, QSettings, JSON state, environment configuration, IPC,
sockets, processes, shell commands, compositor protocols, or ThemeController.

## Explicit Exclusions

This target has no Quickshell import, LayerShellQt dependency, Hyprland
integration, Typhon-private protocol, compositor backend, or command
execution. No current navigation row is collapsible. Performance, Appearance,
and More Settings are normal selectable `group` rows using the same NavItem
composition as the other rows.
