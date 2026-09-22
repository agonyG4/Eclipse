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
├── backend/
│   ├── Cargo.toml
│   ├── Cargo.lock
│   ├── build.rs
│   └── src/
│       ├── animation/       # typed state and thin CXX-Qt QObject
│       ├── appearance/      # Appearance mutations and bounded worker
│       ├── icons/           # installed icon-theme domain and QObject
│       ├── theme_config/    # shared theme.json transaction store
│       └── typhon/          # typed protocol, discovery, and transport
├── qml/
│   ├── CMakeLists.txt
│   ├── Main.qml
│   ├── components/
│   │   ├── controls/       # reusable interactive primitives
│   │   ├── feedback/
│   │   ├── form/           # Settings-specific structural/layout composition
│   │   ├── menu/
│   │   ├── navigation/
│   │   └── typography/
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
                         PUBLIC astrea_settings_backend;
                         PRIVATE Qt6::Network;
                         direct Paper protocol include
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
astrea_settings_backend -> CXX-Qt generated QObjects and Rust domain backends
```

The core target deliberately has no Qt QML, Qt Quick, Quick Controls,
LayerShellQt, or compositor dependency. The application owns shared UI-facing
dependencies directly; the core does not obtain them transitively.

Within QML, `components/controls/` owns reusable interactive primitives such as
`Slider`, `ToggleSwitch`, `SelectButton`, and `SearchField`. `components/form/`
owns structural Settings composition such as `FormCard`, `SettingRow`, and
`ScrollPage`. Prefer deriving from a Qt Quick Controls semantic control and
replacing its visual delegates when one exists; do not recreate pointer,
keyboard, or touch interaction with a raw `MouseArea`.

## Composition and Route Flow

`SettingsApplication` constructs the Linux detector, profile provider and value,
navigation catalogue and model, icon resolver, theme controller, translation
controller, and QML engine. It registers `SettingsController`,
`ThemeController`, and `I18n` as context properties and registers the shared
icon provider as `astrea-icon`.

The catalogue provides ordered descriptors for the flat sidebar and the full
nested destination graph. The model exposes only sidebar-visible rows and
provides native child and ancestor lookup. `SettingsController` selects the
first navigable destination at startup, derives the sidebar highlight for
nested routes, and owns the bounded Back/Forward session history. `Main.qml`
supplies its `selectedPageSource` to one authoritative `Loader`; an empty URL is
never selected and therefore does not produce an empty page.

Customization children appear in this native catalogue order: Appearance,
Visual Effects, Icons, Wallpaper, Dock, and Animations. Their stable route IDs
are `appearance`, `visual-effects`, `icons`, `wallpaper`, `dock`, and
`animations`; nested routes remain hidden from the sidebar.

## Theme Configuration Ownership

QML owns presentation and interaction. `SettingsController.appearance` exposes
the CXX-Qt `SettingsAppearanceController`, whose Rust backend owns mutations of
`theme_preference`, `accent`, and `icon_appearance`. `SettingsController.icons`
exposes `SettingsIconsController`, whose Rust domain owns installed icon-theme
selection and `system_icon_theme`. Both use the shared Rust
`ThemeConfigStore` transaction primitive for `theme.json`.

The C++ `ThemeController` remains the live Qt reader, filesystem watcher, and
projection consumed by QML, Shell, Dock, and Bar. It temporarily writes only
unmigrated legacy fields (`shell_style`, `icon_style`, `icon_theme`, and
`audio_osd_style`). Visual Effects is the transitional UI for `shell_style`
until Phase 2 migrates the material/effects control plane. Phase 2 behavior is
not implemented by this architecture phase.

## Adding a Visual-Only Page

1. Add the QML source under the established `qml/pages/` tree.
2. Register it in the single `astrea-settings-ui` module list.
3. Add one stable descriptor to `SettingsNavigationCatalog` with its exact QML
   module URL. For a nested destination, set `parentId` and
   `sidebarVisible=false`; expose its parent as a navigable Hub only when it
   has a real child.
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
   QML-facing behavior without invoking private shell protocols. For a Rust
   backend, keep domain and protocol cases Qt-independent and add one Qt test
   for the generated QObject's queued, non-blocking boundary.
