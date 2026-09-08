# Settings Appearance Page Design

**Status:** Approved for implementation

## Goal

Add a production-quality `Customization -> Appearance` page to the Eclipse
Settings application. The page exposes two independent settings: an effective
theme preference (`Automatic`, `Light`, `Dark`) and interface surface style
(`Default`, `Transparent`, `Frosted`). Existing settings routes, QML theme
consumers, persisted keys, and visual conventions remain compatible.

## Architecture

`shared/theme/ThemeController` remains the single authority for theme state.
The existing `themeMode` property stays a binary effective rendering value:
`0` for dark and `1` for light. A new persisted `themePreference` property
stores the user intent as `"auto"`, `"light"`, or `"dark"`.

Explicit preferences resolve directly. Automatic resolves from
`QGuiApplication::styleHints()->colorScheme()` and falls back deterministically
to dark for `Unknown`; the controller listens to
`QStyleHints::colorSchemeChanged` and updates only the effective mode while
leaving `themePreference` as `"auto"`. A narrowly scoped injectable color-scheme
provider is used by controller tests so they do not depend on the host theme.
Legacy `setThemeMode()` calls remain supported and intentionally select the
matching explicit preference.

Configuration loading gives `theme_preference` precedence, then migrates valid
legacy `theme`, then valid legacy `theme_mode`, all in memory. Missing or
invalid `shell_style` resolves to `1` (Default); valid `0`, `1`, and `2` keep
their existing meanings. Saving writes `theme_preference`, effective `theme`,
effective `theme_mode`, `shell_style`, and all existing unrelated theme keys.

`Settings/qml/theme/State.qml` and `Settings/qml/components/Theme.qml` project
the new preference but do not resolve it. Existing `Apps.qml`, `Shell.qml`,
Bar, Shell menus, and other consumers continue using binary `themeMode`.

## Navigation and page structure

The native navigation catalog adds a hidden child destination:

- ID: `appearance`
- Parent: `customization`
- Label: `Appearance`
- Subtitle: `Light, dark, and interface style`
- Icon: the existing theme icon
- Page: `qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Appearance.qml`

Customization's child order becomes `Appearance`, `Wallpaper`, `Dock`, while
the sidebar remains flat and unchanged. The page uses the existing
`Form.ScrollPage`, `Form.SectionHeader`, `Form.FormCard`, content margins, max
width, theme tokens, typography, and animation timings.

The page has two sections. Each section contains three compact cards in a
responsive horizontal layout. Cards have an Eclipse-style preview, translated
label, accent-backed selected border/indicator, hover feedback, keyboard focus,
and click/Return/Space activation. Appearance previews use explicit local light
and dark palette values; Automatic visibly combines both palettes. Interface
style previews vary surface opacity/detail without implementing compositor blur.
Changing one section never changes the other. Every selection updates the
controller and calls `save()` immediately. Stable object names identify the
page, cards, and sections for offscreen tests.

## Test strategy

Controller tests are written first for missing defaults, legacy migration,
interface-style compatibility, canonical persistence, explicit legacy setters,
and automatic platform changes through the injected provider. The focused
controller test remains deterministic and does not read the host color scheme.

The QML smoke test is extended to assert the three Customization children and
Appearance route, all six cards, initial selection projection, each activation
path, and an empty QML warning list. The structural source test checks the new
QML registration, route/page source, and updated registration count. Existing
Settings QML lint and CTest verification continue to use the checkout's
existing `build/` directory with bounded parallelism.

## Files in scope

- Modify `shared/theme/ThemeController.hpp/.cpp`.
- Modify `Settings/qml/theme/State.qml`, `Settings/qml/components/Theme.qml`,
  `Settings/qml/CMakeLists.txt`, and the navigation catalog.
- Create `Settings/qml/pages/appearance/Appearance.qml`.
- Modify `Settings/assets/i18n/en_US.json`.
- Extend `Settings/tests/unit/ThemeControllerTest.cpp`,
  `Settings/tests/integration/SettingsQmlSmokeTest.cpp`, and
  `Settings/tests/static/SettingsStructureTest.cmake`.

No wallpaper, Dock, compositor, daemon, IPC, scheduling, geolocation, accent,
icon-theme, or global color-system behavior is included.
