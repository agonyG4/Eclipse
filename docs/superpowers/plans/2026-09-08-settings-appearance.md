# Settings Appearance Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a backward-compatible Appearance page with Automatic/Light/Dark and Default/Transparent/Frosted choices, backed by a shared effective-theme resolver and covered by deterministic controller and QML tests.

**Architecture:** Extend `shared/theme/ThemeController` with persisted `themePreference` and a private effective-mode resolver driven by `QStyleHints::colorSchemeChanged`. Keep `themeMode` binary and keep all existing QML consumers on that property. Add a native hidden `appearance` child route and a local card-based QML page that calls the existing controller and save API.

**Tech Stack:** C++20, Qt 6.6 Settings compatibility floor, Qt Gui `QStyleHints`, Qt Quick/QML, CMake/CTest, Qt Test, existing Eclipse Settings theme/form components.

## Global Constraints

- Work directly on the existing Eclipse `main` checkout; do not create a branch, worktree, or temporary build directory.
- Reuse `/home/agony/GitHub/Eclipse/build`; build parallelism is `4`.
- Preserve unrelated existing working-tree edits in Dock/Slider files.
- `themeMode` remains `0` dark and `1` light; Automatic is stored only in `themePreference`.
- `themePreference` accepts only `"auto"`, `"light"`, and `"dark"`; Unknown platform color scheme falls back to dark.
- Missing/new `shellStyle` defaults to `1`; valid stored `0` and `2` retain their current meanings; invalid values resolve to `1`.
- No Qt 6.8-only color-scheme setter APIs, scheduler, geolocation, compositor blur, daemon, IPC, or unrelated settings redesign.
- QML owns presentation/interaction only; C++ owns persistence and platform access.
- Use `apply_patch` for source edits and `rtk` for shell/build/test output where available.

---

### Task 1: Add deterministic ThemeController behavior with TDD

**Files:**
- Modify: `Settings/tests/unit/ThemeControllerTest.cpp`
- Modify: `shared/theme/ThemeController.hpp`
- Modify: `shared/theme/ThemeController.cpp`

**Interfaces:**
- Produces `QString ThemeController::themePreference() const` and `void ThemeController::setThemePreference(const QString&)` with `themePreferenceChanged()`.
- Preserves `themeMode()`, `setThemeMode(int)`, `shellStyle()`, `setShellStyle(int)`, `applyConfig()`, `reload()`, and `save()`.
- Adds a narrow injectable `std::function<Qt::ColorScheme()>` provider for deterministic tests; the normal constructor reads `QGuiApplication::styleHints()` and connects `colorSchemeChanged`.

- [ ] **Step 1: Add the controller test declarations and fixtures.**

Extend `ThemeControllerTest` with test slots named:

```cpp
void missingConfigurationUsesAutomaticAndDefaultStyle();
void legacyLightConfigurationMigratesInMemory();
void legacyDarkConfigurationMigratesInMemory();
void validShellStylesRemainCompatible();
void invalidShellStyleUsesDefault();
void saveWritesCanonicalAndLegacyThemeKeys();
void legacySetThemeModeSelectsExplicitPreference();
void automaticPlatformChangeUpdatesEffectiveModeOnly();
```

Add small helpers that write a `QVariantMap`/JSON fixture to a `QTemporaryDir`,
read the saved JSON object, and construct a controller with a mutable color
scheme provider. Keep the missing-configuration test using no provider so a
headless test has the deterministic Unknown-to-dark behavior.

- [ ] **Step 2: Run the focused unit test to verify the new expectations fail.**

Run:

```bash
rtk run "cmake --build build --target theme-controller-test --parallel 4"
rtk ctest --test-dir build -R '^theme-controller-test$' --output-on-failure
```

Expected: the existing target either fails to compile because the new test
methods/properties do not exist or fails the new assertions; no production
implementation has been added yet.

- [ ] **Step 3: Add the preference and effective-mode API.**

In `ThemeController.hpp`, add the `themePreference` Q_PROPERTY, getter/setter,
signal, a provider alias/constructor overload that keeps existing call sites
valid, and private helpers for normalized preferences, platform resolution,
effective-mode updates, and the color-scheme change callback. Keep `shellStyle`
initialized to `1`.

In `ThemeController.cpp`, implement these rules:

```cpp
static constexpr auto kAuto = "auto";
static constexpr auto kLight = "light";
static constexpr auto kDark = "dark";

// explicit light -> 1, explicit dark -> 0,
// automatic -> QStyleHints::colorScheme(), Unknown -> 0
```

`setThemePreference()` changes the preference and recomputes the effective mode.
`setThemeMode()` remains an explicit legacy setter: it normalizes to `0/1`,
selects `dark/light`, and emits both property notifications when needed. The
private effective-mode setter must not rewrite `themePreference` when Automatic
resolves differently. Connect the available `QStyleHints::colorSchemeChanged`
signal to that private recomputation path without using Qt 6.8 setters.

- [ ] **Step 4: Implement configuration migration and canonical save.**

In `applyConfig()` use this precedence:

1. Valid `theme_preference` (`auto`, `light`, or `dark`).
2. Legacy `theme` exactly `light` or `dark`, case-insensitive.
3. Valid legacy `theme_mode` `0` or `1`.
4. Otherwise leave the default/current preference unchanged.

Parse `shell_style` only when it is `0`, `1`, or `2`; map any other supplied
value to `1`, while leaving unrelated keys on their existing compatibility
paths. `save()` must write `theme_preference`, effective `theme`, effective
`theme_mode`, `shell_style`, `accent`, `icon_style`, `icon_theme`, and
`audio_osd_style`.

- [ ] **Step 5: Run the focused unit tests to verify they pass.**

Run the same build and CTest commands from Step 2. Expected: all
`theme-controller-test` cases pass, including the injected-provider test that
changes from Dark to Light while `themePreference()` remains `"auto"`.

- [ ] **Step 6: Commit only the controller/test files.**

```bash
git add shared/theme/ThemeController.hpp shared/theme/ThemeController.cpp Settings/tests/unit/ThemeControllerTest.cpp
git commit -m "feat(settings): add automatic theme preference"
```

Do not stage existing unrelated working-tree edits.

### Task 2: Add the Appearance destination, state projection, registration, and translations

**Files:**
- Modify: `Settings/core/navigation/SettingsNavigationCatalog.cpp`
- Modify: `Settings/qml/theme/State.qml`
- Modify: `Settings/qml/components/Theme.qml`
- Modify: `Settings/qml/CMakeLists.txt`
- Modify: `Settings/assets/i18n/en_US.json`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`

**Interfaces:**
- Produces an enabled-but-sidebar-hidden `appearance` destination under `customization` before `wallpaper` and `dock`.
- Exposes `themePreference` through `State.qml` and `Theme.qml` without resolving it in QML.
- Registers `pages/appearance/Appearance.qml` once in the existing module.

- [ ] **Step 1: Add structural assertions before changing production wiring.**

Extend `SettingsStructureTest.cmake` so it requires the Appearance page in the
production source list, checks the `appearance` route/page source tokens in the
catalog, and expects 41 registered QML files. Add a dedicated Appearance source
check for the page object name, six stable card object names, and the two
section headers once the page exists.

- [ ] **Step 2: Run the structural test to verify it fails for the missing route/page.**

Run:

```bash
rtk run "cmake --build build --target settings-structure-test --parallel 4"
rtk ctest --test-dir build -R '^settings-structure-test$' --output-on-failure
```

Expected: FAIL because the registered count, source list, and Appearance page
are not present yet.

- [ ] **Step 3: Add the native destination in the required order.**

Insert an `appearance` `makeEntry()` immediately before `wallpaper` in
`SettingsNavigationCatalog.cpp` with:

```cpp
QStringLiteral("appearance")
QStringLiteral("Appearance")
QStringLiteral("settings.nav.appearance")
QStringLiteral("Light, dark, and interface style")
QStringLiteral("theme")
QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Appearance.qml"))
SettingsNavigationEntry::Kind::Page, true, false,
QStringLiteral("customization")
QStringLiteral("settings.nav.appearance.subtitle")
```

Do not change sidebar visibility or add a new top-level item.

- [ ] **Step 4: Project the preference and register the page.**

Add a writable `themePreference` property and two-way signal synchronization
to `Settings/qml/theme/State.qml` and `Settings/qml/components/Theme.qml`,
matching the existing `themeMode` projection. Add
`pages/appearance/Appearance.qml` to the authoritative QML list so the route
uses the generated `Astrea.Settings` module.

- [ ] **Step 5: Add the required English keys.**

Add these keys using the existing naming convention:

```json
"settings.nav.appearance": "Appearance",
"settings.nav.appearance.subtitle": "Light, dark, and interface style",
"apps.settings.pages.appearance.text.appearance": "APPEARANCE",
"apps.settings.pages.appearance.text.interface_style": "INTERFACE STYLE",
"apps.settings.pages.appearance.option.automatic": "Automatic",
"apps.settings.pages.appearance.option.light": "Light",
"apps.settings.pages.appearance.option.dark": "Dark",
"apps.settings.pages.appearance.option.default": "Default",
"apps.settings.pages.appearance.option.transparent": "Transparent",
"apps.settings.pages.appearance.option.frosted": "Frosted"
```

- [ ] **Step 6: Run the structural test after wiring the route.**

Run the commands from Step 2. Expected: it remains red until the page file is
added in Task 3, then passes with 41 registered files and the new route.

- [ ] **Step 7: Commit only navigation/projection/registration/translation changes.**

```bash
git add Settings/core/navigation/SettingsNavigationCatalog.cpp Settings/qml/theme/State.qml Settings/qml/components/Theme.qml Settings/qml/CMakeLists.txt Settings/assets/i18n/en_US.json Settings/tests/static/SettingsStructureTest.cmake
git commit -m "feat(settings): register appearance destination"
```

### Task 3: Build the Appearance page and extend QML smoke coverage with TDD

**Files:**
- Create: `Settings/qml/pages/appearance/Appearance.qml`
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`

**Interfaces:**
- Provides page object name `appearancePage` and scroll object name
  `appearanceScrollPage`.
- Provides card object names `appearanceOption-auto`,
  `appearanceOption-light`, `appearanceOption-dark`,
  `interfaceStyleOption-default`, `interfaceStyleOption-transparent`, and
  `interfaceStyleOption-frosted`.
- Selecting an Appearance card calls the matching `ThemeController` preference
  setter and `ThemeController.save()`; selecting an Interface Style card calls
  `setShellStyle(1/0/2)` and `save()` without touching the other setting.

- [ ] **Step 1: Add failing QML smoke declarations and assertions.**

Add test slots for `loadsAppearanceRouteFromHubOffscreen()` and
`appearanceChoicesUpdateController()`. Update
`loadsCustomizationHubOffscreen()` to require the `appearance` row first,
exactly three `hubNavigationRow-*` rows, and three current destination
children. The new route test should navigate to `customization`, invoke the
Appearance row, assert `appearancePage`/`appearanceScrollPage`, all six card
objects, and an empty `QQmlApplicationEngine::warnings` list. The selection test
should construct the controller with a missing config, assert the initial
`auto`/Default selections, invoke each card's QML `activate` function, and
compare the C++ properties after each activation.

- [ ] **Step 2: Run the focused QML smoke test to verify it fails.**

Run:

```bash
rtk run "cmake --build build --target settings-qml-smoke-test --parallel 4"
rtk ctest --test-dir build -R '^settings-qml-smoke-test$' --output-on-failure
```

Expected: FAIL because the hub has only Wallpaper and Dock and the route/page
does not exist.

- [ ] **Step 3: Implement the page shell and two sections.**

Create an `Item` root with `objectName: "appearancePage"`, one direct
`Form.ScrollPage` with the existing content margins/max width, and two direct
`Form.SectionHeader` + `Form.FormCard` groups. Use `GridLayout` with three
columns at the existing Settings minimum width and a responsive fallback when
the available content is narrower. Keep section spacing compact and avoid
hard-coded coordinates tied to the 1050 px default window.

- [ ] **Step 4: Implement local preview cards and interaction.**

Use a small local `FocusScope`/card delegate with explicit properties for the
choice ID, label, selection state, and preview kind. Draw the preview with
local light/dark backing colors, a small shell/window composition, and simple
opacity differences for Default/Transparent/Frosted. Do not alter production
theme tokens. Add an accent border and visible check/selection indicator,
hover color behavior, `activeFocusOnTab`, and `Keys` handling for Return/Space.
The click handler must call the same `activate()` function so tests and users
exercise one behavior path. Use `I18n.tr()` for every visible label and section
header.

- [ ] **Step 5: Implement controller bindings and immediate persistence.**

Drive selected state from `Theme.themePreference` and `Theme.shellStyle`.
Implement the six activation handlers as:

```qml
Theme.themePreference = "auto";  Theme.save()
Theme.themePreference = "light"; Theme.save()
Theme.themePreference = "dark";  Theme.save()
Theme.shellStyle = 1;             Theme.save()
Theme.shellStyle = 0;             Theme.save()
Theme.shellStyle = 2;             Theme.save()
```

Use `Theme`/`State` projection rather than duplicating resolution logic in the
page, and never change the other setting as a side effect.

- [ ] **Step 6: Run focused QML smoke tests and QML lint.**

Run:

```bash
rtk run "cmake --build build --target settings-qml-smoke-test astrea-settings-ui_qmllint --parallel 4"
rtk ctest --test-dir build -R '^(settings-qml-smoke-test|settings-structure-test)$' --output-on-failure
```

Expected: the hub contains exactly three child rows, all six cards activate,
the route has no warnings, and the lint target reports no new warnings/errors.

- [ ] **Step 7: Commit only the Appearance page and QML integration tests.**

```bash
git add Settings/qml/pages/appearance/Appearance.qml Settings/tests/integration/SettingsQmlSmokeTest.cpp
git commit -m "feat(settings): add appearance settings page"
```

### Task 4: Full verification and handoff

**Files:**
- No new source files; inspect all task files and the final working tree.

- [ ] **Step 1: Build the existing configured checkout.**

```bash
rtk run "cmake --build build --parallel 4"
```

- [ ] **Step 2: Run the focused tests again.**

```bash
rtk ctest --test-dir build -R '^(theme-controller-test|settings-qml-smoke-test|settings-structure-test)$' --output-on-failure
```

- [ ] **Step 3: Run the complete available CTest suite.**

```bash
rtk ctest --test-dir build --output-on-failure
```

- [ ] **Step 4: Run the documented Settings lint and source audits.**

```bash
rtk run "cmake --build build --target astrea-settings-ui_qmllint --parallel 4"
rtk rg -n 'Form\.ToggleSwitch|Form\.SelectButton|Form\.SearchField' Settings/qml
rtk rg -n 'components/form/ToggleSwitch\.qml|components/form/SelectButton\.qml|components/form/SearchField\.qml' Settings
rtk rg -n '^[[:space:]]*Slider[[:space:]]*\{' Settings/qml/pages/appearance/Dock.qml
rtk rg -n 'Controls\.Slider|detentValue|writeConfig|writePersonalization' Settings/qml/pages/appearance/Dock.qml Settings/services/dock
git diff --check
```

The first three current-source searches must be empty; the Dock audit must
remain compatible with the existing unrelated worktree edits.

- [ ] **Step 5: Review the final diff and report exact evidence.**

Use `git diff main...HEAD` plus the remaining working-tree diff without
claiming unrelated user edits. Report changed files, migration precedence,
defaults, Automatic semantics, focused/full test results, the reused build
directory, lint registration/lint counts, and any unavailable verification.
