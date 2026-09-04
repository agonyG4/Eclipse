# Settings Structure

## Directory Tree

```text
Settings/
├── CMakeLists.txt
├── AGENTS.md
├── app/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── SettingsApplication.cpp
│   └── SettingsApplication.hpp
├── core/
│   ├── CMakeLists.txt
│   ├── SettingsController.cpp
│   ├── SettingsController.hpp
│   └── navigation/
│       ├── SettingsNavigationCatalog.cpp
│       ├── SettingsNavigationCatalog.hpp
│       ├── SettingsNavigationEntry.hpp
│       ├── SettingsNavigationModel.cpp
│       └── SettingsNavigationModel.hpp
├── services/
│   ├── assets/
│   ├── dock/
│   ├── i18n/
│   ├── profile/
│   └── wallpaper/
├── platform/linux/
├── qml/
│   ├── CMakeLists.txt
│   ├── Main.qml
│   ├── components/
│   ├── pages/
│   └── theme/
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   ├── integration/
│   └── static/
├── docs/
├── assets/
├── packaging/
└── cmake/
```

## Target Graph

```text
astrea-settings-core  -> PUBLIC Qt6::Core, astrea-shared-dock;
                         PRIVATE Qt6::Network; direct Paper protocol include
astrea-settings-ui    -> Qt6::Core, Core5Compat, Gui, Qml, Quick, QuickControls2
astrea-settings       -> astrea-settings-core, astrea-settings-ui,
                          astrea-settings-uiplugin,
                          astrea-shared-core, astrea-shared-coreplugin, Qt app libraries
Settings unit tests   -> astrea-settings-core, Qt6::Test
Settings QML tests    -> astrea-settings-core, astrea-settings-ui,
                          astrea-settings-uiplugin, shared targets, Qt6::Test
```

## Dependency Direction

```text
app -> core -> services -> platform/linux
qml -> context properties supplied by app
tests -> public production targets -> explicit fake boundaries
shared -> compositor-independent utilities only
```

The core target deliberately has no Qt QML, Qt Quick, Quick Controls,
LayerShellQt, or compositor dependency. The application owns shared UI-facing
dependencies directly; the core does not obtain them transitively.

## Composition and Route Flow

`SettingsApplication` constructs the Linux detector, profile provider and value,
navigation catalogue and model, icon resolver, theme controller, translation
controller, and QML engine. It registers `SettingsController`,
`ThemeController`, and `I18n` as context properties and registers the shared
icon provider as `astrea-icon`.

The catalogue provides ordered descriptors. The model expands sections and
selects only routable Page/Child entries by stable ID. `SettingsController`
selects the first routable descriptor at startup, and its
`selectedPageSource` reads the selected descriptor's optional URL. `Main.qml`
supplies that URL to one `Loader`; an empty URL is never selected and therefore
does not produce an empty selected page.

## Adding a Visual-Only Page

1. Add the QML source under the established `qml/pages/` tree.
2. Register it in the single `astrea-settings-ui` module list.
3. Add one stable descriptor to `SettingsNavigationCatalog` with its exact QML
   module URL.
4. Add native model/controller route tests and a source-policy test if needed.

Do not add a route condition to `Main.qml` and do not add a numeric page index.

## Adding a Native Backend

1. Define a focused service interface with an explicit value or callback boundary.
2. Put Linux, compositor, IPC, DBus, or other raw system access in the matching
   `platform/` implementation.
3. Inject the service at `SettingsApplication`, then expose only a stable
   controller API to QML.
4. Keep QML responsible for presentation and interaction only.
5. Add unit tests against the service boundary and integration coverage for the
   QML-facing behavior without invoking private shell protocols.
