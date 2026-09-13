# System-wide App Icon Appearance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a persisted `default`/`monochrome`/`tinted` app-icon presentation preference that updates the shared Astrea Shell icon renderer and is exposed with deterministic previews in Settings Appearance.

**Architecture:** Extend the existing shared `ThemeController` and its watched JSON state with canonical string `iconAppearance` values. Extend the existing `AstreaAppIcon` rounded `ShaderEffect` so one fragment shader handles rounding plus luminance-based presentation; keep provider/source-theme resolution unchanged. Add the Settings proxy and Appearance cards with explicit controller mutation and real `Shared.AstreaAppIcon` preview overrides.

**Tech Stack:** C++20, Qt 6.6+, Qt Quick/QML, Qt Quick ShaderEffect, QtTest, CMake/CTest.

## Global Constraints

- Work directly on `main` in `/home/agony/GitHub/Eclipse`; do not create a branch or worktree because `Settings/AGENTS.md` requires direct work and the user requires the existing build folder.
- Reuse `/home/agony/GitHub/Eclipse/build`; compile only with `cmake --build build --parallel 4`.
- Use `rtk` for shell inspection, diffs, builds, tests, and searches.
- Do not use subagents.
- Do not modify `AstreaIconTheme`, `AstreaIconProvider`, DockAppDelegate, SpotlightResultsList, or AltTabWindowDelegate for presentation logic.
- Do not repurpose or migrate legacy `iconStyle`, `iconTheme`, `icon_style`, or `icon_theme`.
- Persist only canonical lowercase `default`, `monochrome`, or `tinted` under JSON key `icon_appearance`.
- Do not add Dark, Clear, layered icon assets, a new controller, a daemon, IPC, DBus, polling, MultiEffect, ColorOverlay, ShaderEffectSource, CPU recoloring, or provider cache invalidation.
- Keep Default with `iconRadius <= 0` on the direct Image fast path; use one combined ShaderEffect for rounding and non-default presentation.
- Preserve source URL, DPR, resolution-aware target sizing, async loading, caching, fallbacks, alpha, and existing rounded behavior.

## File Map

- Modify `shared/theme/ThemeController.hpp/.cpp`: canonical property, normalization, reload reset, persistence, and signals.
- Modify `shared/qml/AstreaAppIcon.qml`: optional global controller consumption, preview overrides, effective mode/tint properties, and one combined shader activation path.
- Modify `shared/qml/shaders/rounded_icon.frag`: add luminance/grayscale/tint uniforms while retaining the existing rounded coverage math.
- Modify `shared/tests/AstreaAppIconQmlTest.cpp`: controller-free, override, activation, source-quality, and deterministic pixel tests.
- Modify `Settings/qml/theme/State.qml` and `Settings/qml/components/Theme.qml`: controller-backed projection and mutation.
- Modify `Settings/qml/pages/appearance/Appearance.qml`: `APP ICONS` cards and production-renderer previews using existing colorful Settings SVG assets.
- Modify `Settings/assets/i18n/en_US.json`: Appearance section and option translations.
- Modify `Settings/tests/unit/ThemeControllerTest.cpp`: controller contract regressions.
- Modify `Settings/tests/integration/SettingsQmlSmokeTest.cpp`: Settings selection, watcher, projection, preview, and warning regressions.
- Modify `Settings/tests/static/SettingsStructureTest.cmake`: stable Appearance object names/translation invariants.
- Modify `Settings/docs/ARCHITECTURE.md`: distinguish global app-icon presentation, legacy Settings icon fields, and XDG source-theme resolution.
- Do not change CMake unless the existing `Astrea.Shared` import or shader resource registration requires it; if changed, keep the existing QML module and single shader resource target.

---

### Task 1: Add failing ThemeController contract tests

**Files:**
- Modify: `Settings/tests/unit/ThemeControllerTest.cpp`
- Read-only reference: `shared/theme/ThemeController.hpp`, `shared/theme/ThemeController.cpp`

**Interfaces:**
- Tests will require `QString ThemeController::iconAppearance() const`, `void setIconAppearance(const QString &)`, and `iconAppearanceChanged()`.
- Tests will use the existing `writeConfig()`/`readConfig()` helpers and watcher timing conventions.

- [x] **Step 1: Add focused test slots before production code**

Add slots named `iconAppearanceDefaultsToDefault`, `iconAppearanceNormalizesCanonicalValues`, `iconAppearanceRejectsInvalidValues`, `iconAppearanceResetsOnCompleteReplacement`, `iconAppearancePersistsWithoutChangingLegacyFields`, `legacyIconFieldsDoNotMigrateToAppearance`, and `iconAppearancePreservesUnrelatedThemeState`.

Use assertions equivalent to:

```cpp
ThemeController controller(missingPath);
QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));

controller.setIconAppearance(QStringLiteral("TiNtEd"));
QCOMPARE(controller.iconAppearance(), QStringLiteral("tinted"));
controller.setIconAppearance(QStringLiteral("unsupported"));
QCOMPARE(controller.iconAppearance(), QStringLiteral("default"));

controller.applyConfig({
    {QStringLiteral("icon_appearance"), QStringLiteral("monochrome")},
    {QStringLiteral("icon_style"), 1},
    {QStringLiteral("icon_theme"), QStringLiteral("dark")},
});
controller.save();
const QJsonObject saved = readConfig(path);
QCOMPARE(saved.value(QStringLiteral("icon_appearance")).toString(),
         QStringLiteral("monochrome"));
QCOMPARE(saved.value(QStringLiteral("icon_style")).toInt(), 1);
QCOMPARE(saved.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("dark"));
```

Also write a complete external replacement without `icon_appearance` after selecting `tinted` and assert it resets to `default`, and assert changing `iconAppearance` leaves `themePreference`, `shellStyle`, and `accentHex` unchanged. A config containing only `icon_style: 0` and `icon_theme: "dark"` must still report `default`.

- [x] **Step 2: Run the unit test to verify the expected RED state**

Run:

```bash
./build/Settings/tests/theme-controller-test
```

Expected: compilation or test failure because the new property and behavior do not yet exist. Fix only test syntax/setup errors; do not add production code before the missing behavior is the reason for failure.

---

### Task 2: Implement canonical ThemeController state and persistence

**Files:**
- Modify: `shared/theme/ThemeController.hpp`
- Modify: `shared/theme/ThemeController.cpp`

**Interfaces:**
- Produce `Q_PROPERTY(QString iconAppearance READ iconAppearance WRITE setIconAppearance NOTIFY iconAppearanceChanged)`.
- Produce canonical values `default`, `monochrome`, `tinted` and `icon_appearance` JSON persistence.

- [x] **Step 1: Add the property, signal, member, and normalization helpers**

Add the property beside the existing icon fields, getter/setter declarations, `iconAppearanceChanged()`, `m_iconAppearance = QStringLiteral("default")`, and private helpers:

```cpp
static QString normalizedIconAppearance(const QString &value);
static bool isValidIconAppearance(const QString &value);
```

Normalize with `trimmed().toLower()`, accept only the three canonical strings, and return `default` for empty or invalid input.

- [x] **Step 2: Reset the preference during every config application**

In `applyConfig`, call `setIconAppearance(config.value(QStringLiteral("icon_appearance")).toString())` even when the key is absent. This makes complete external replacements reset stale state instead of preserving the previous value. Do not read `icon_style` or `icon_theme` as a fallback.

- [x] **Step 3: Persist the canonical field without changing existing fields**

Add `{QStringLiteral("icon_appearance"), m_iconAppearance}` to the existing save object. Leave `theme_preference`, `theme`, `theme_mode`, `shell_style`, `accent`, `icon_style`, `icon_theme`, and `audio_osd_style` semantics and output intact.

- [x] **Step 4: Run the controller tests to verify GREEN**

Run:

```bash
cmake --build build --target theme-controller-test --parallel 4
./build/Settings/tests/theme-controller-test
```

Expected: all ThemeController tests pass, including the new canonicalization and replacement-reset cases.

---

### Task 3: Add failing shared AstreaAppIcon presentation tests

**Files:**
- Modify: `shared/tests/AstreaAppIconQmlTest.cpp`

**Interfaces:**
- Tests will require `effectiveAppearance`, `effectiveTintColor`, `appearanceOverride`, `tintColorOverride`, and `hasTintColorOverride`.
- Tests will inspect the existing `roundedIconEffect` object as the one combined presentation shader.

- [x] **Step 1: Add standalone contract tests before QML implementation**

Add tests for:

```cpp
void defaultAppearanceKeepsDirectImageFastPath();
void nonDefaultAppearanceActivatesSingleShader();
void presentationKeepsSourceQualityStable();
void missingThemeControllerFallsBackToDefault();
void appearanceOverrideDoesNotMutateController();
```

Create an icon with the existing `createIcon()` helper, set a deterministic local SVG path when pixel output is needed, and assert the missing-controller default. Assert `iconRadius == 0` plus default has no `roundedIconEffect`; monochrome and tinted each create exactly one effect; switching modes preserves `resolvedSource`, `effectiveMaximumLogicalSize`, and `effectiveSourcePixelSize`; and an override changes only `effectiveAppearance`.

Add one controller-backed QML context test using `QQmlComponent`/`QQmlContext` or the existing test engine to set a `ThemeController` context property, switch `default -> monochrome -> tinted`, and update `accentHex` while tinted without recreating the item.

- [x] **Step 2: Add deterministic pixel behavior tests**

Use a generated SVG with transparent corners and at least three opaque regions with different colors/luminances. Capture an unrounded 96px item in the existing offscreen-compatible helper. Assert monochrome RGB channels are approximately equal for opaque interior samples, two luminance regions remain measurably distinct, and alpha/transparent corners are preserved. Set tint `#30d158` and assert tinted output has green-dominant hue, distinct source luminance remains distinct, and transparent pixels stay transparent. Keep tolerance around shader sampling rather than exact antialiasing pixels.

- [x] **Step 3: Run the shared icon tests to verify RED**

Run:

```bash
cmake --build build --target astrea-app-icon-qml-test --parallel 4
./build/shared/tests/astrea-app-icon-qml-test
```

Expected: the tests fail because the public presentation properties and shader activation do not yet exist.

---

### Task 4: Implement the one-pass shared presentation renderer

**Files:**
- Modify: `shared/qml/AstreaAppIcon.qml`
- Modify: `shared/qml/shaders/rounded_icon.frag`
- Modify: `shared/CMakeLists.txt` only if shader/resource registration needs adjustment

**Interfaces:**
- Produce the explicit `AstreaAppIcon` properties and safe global-controller fallback.
- Preserve the existing `roundedIconEffect` object name and resolution-aware source contract.

- [x] **Step 1: Add the public presentation API and effective state**

Add:

```qml
property string appearanceOverride: ""
property color tintColorOverride: "#0a84ff"
property bool hasTintColorOverride: false
readonly property string effectiveAppearance: ...
readonly property color effectiveTintColor: ...
readonly property real presentationMode: ...
```

Resolve a non-empty override first, then `ThemeController.iconAppearance` when the context exists, otherwise `default`. Accept only canonical values after case normalization. Resolve tint from the explicit override when enabled, then `ThemeController.accentHex`, then `#0a84ff`.

- [x] **Step 2: Keep the direct Image fast path and activate one effect for non-default modes**

Change `Image.visible` to require `status === Image.Ready`, `iconRadius <= 0`, and `effectiveAppearance === "default"`. Change the existing loader `active` expression to require a ready source and either positive radius or non-default appearance. Keep one `ShaderEffect` with `source: iconImage`, `roundedRadius`, `presentationMode`, and `tintColor`; do not add another source, effect, image provider, cache key, or consumer-specific binding.

- [x] **Step 3: Extend the existing shader math**

Add float presentation mode and tint uniforms to `rounded_icon.frag`. After sampling once, calculate Rec.709 luminance:

```glsl
float luma = dot(pixel.rgb, vec3(0.2126, 0.7152, 0.0722));
```

Use neutral `vec3(luma)` for monochrome. For tinted, map luminance from a dark accent endpoint such as `tintColor.rgb * 0.24` to `tintColor.rgb` with `mix`, preserving contrast. Keep default RGB unchanged. Apply existing rounded coverage to RGB and source alpha, then multiply by `qt_Opacity`. Do not tint transparent pixels or add a backing plate.

- [x] **Step 4: Run the shared icon tests to verify GREEN**

Run:

```bash
cmake --build build --target astrea-app-icon-qml-test --parallel 4
./build/shared/tests/astrea-app-icon-qml-test
```

Expected: all existing resolution/rounded/fallback tests and the new presentation tests pass.

---

### Task 5: Add failing Settings projection/UI tests

**Files:**
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`

**Interfaces:**
- Tests will require `Theme.iconAppearance`, `iconAppearance-default`, `iconAppearance-monochrome`, and `iconAppearance-tinted`.

- [x] **Step 1: Add the Settings smoke test slot**

Add `appearanceIconChoicesPreserveControllerPropagation()`. Load the existing Appearance route with a temporary theme path and dark platform provider. Find exactly the three cards, assert Default is selected, activate Monochrome and Tinted and then Default, and assert `themePreference`, `shellStyle`, and `accentHex` never change. Evaluate `Components.Theme.iconAppearance` through `QQmlExpression` to prove the proxy binding follows the controller.

Replace the watched JSON with `icon_appearance: "TiNtEd"` and a changed accent, assert the controller canonicalizes to `tinted`, the Tinted card remains selected, and the projected tint/accent changes. Replace it without `icon_appearance` and assert Default is selected. Assert the page emits no new QML warnings.

- [x] **Step 2: Add structural tokens before production UI**

Require all three object names, `apps.settings.pages.appearance.text.app_icons`, and option translation keys in the Appearance source. Keep the existing registered QML count unchanged.

- [x] **Step 3: Run the new smoke/structure tests to verify RED**

Run:

```bash
cmake --build build --target settings-qml-smoke-test --parallel 4
./build/Settings/tests/settings-qml-smoke-test appearanceIconChoicesPreserveControllerPropagation
ctest --test-dir build --output-on-failure -R '^settings-structure-test$'
```

Expected: the new test cannot find the three cards and the structural test reports the missing tokens.

---

### Task 6: Implement Settings projection, previews, translations, and documentation

**Files:**
- Modify: `Settings/qml/theme/State.qml`
- Modify: `Settings/qml/components/Theme.qml`
- Modify: `Settings/qml/pages/appearance/Appearance.qml`
- Modify: `Settings/assets/i18n/en_US.json`
- Modify: `Settings/docs/ARCHITECTURE.md`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`

**Interfaces:**
- Produce the data flow `Appearance.qml -> Theme.setIconAppearance -> State.setIconAppearance -> ThemeController`.
- Produce three real `Shared.AstreaAppIcon` preview groups using existing colorful `display.svg`, `network.svg`, and `sound.svg` assets.

- [x] **Step 1: Add the State and Theme proxy property/mutation**

Mirror the existing Accent Color pattern:

```qml
property string iconAppearance: ThemeController.iconAppearance

function setIconAppearance(value) {
    ThemeController.iconAppearance = value
}
```

Add the matching `Theme.qml` property, change handler, `Connections` handler, and mutation forwarding through `Borealis.State`. Do not alter the projected property directly from Appearance.

- [x] **Step 2: Extend ChoiceCard only for the new group**

Make `ChoiceCard.selected` choose `Components.Theme.iconAppearance` for `choiceGroup === "iconAppearance"`. Make `activate()` call `Components.Theme.setIconAppearance(choiceId)` for that group, then call the existing `save()`. Preserve the existing Appearance/Interface Style branches exactly.

- [x] **Step 3: Add the APP ICONS section and production previews**

Import `Astrea.Shared as Shared` in `Appearance.qml`. Add an `APP ICONS` header below Accent Color and a three-column `FormCard` using `ChoiceCard` with object names `iconAppearance-default`, `iconAppearance-monochrome`, and `iconAppearance-tinted`.

Inside each card's preview, instantiate three `Shared.AstreaAppIcon` items with `appearanceOverride: choiceCard.choiceId`, `hasTintColorOverride: true`, `tintColorOverride: Components.Theme.accent`, and deterministic URLs resolved to the existing Settings `display.svg`, `network.svg`, and `sound.svg` assets. Disable the old abstract preview decorations for this group so the cards visibly use the real shared renderer. Do not forward global controller state manually.

- [x] **Step 4: Add translation keys and architecture documentation**

Add `apps.settings.pages.appearance.text.app_icons` plus `option.default`, `option.monochrome`, and `option.tinted`, reusing the existing Default key only if the current catalog convention permits it. Document the three-way distinction between `icon_appearance`, legacy `icon_style`/`icon_theme`, and `AstreaIconTheme` in `Settings/docs/ARCHITECTURE.md`, including that Monochrome/Tinted are derived from resolved artwork and Dark/Clear are unsupported.

- [x] **Step 5: Run focused Settings tests to verify GREEN**

Run:

```bash
cmake --build build --parallel 4
./build/Settings/tests/settings-qml-smoke-test appearanceIconChoicesPreserveControllerPropagation
ctest --test-dir build --output-on-failure -R '^(settings-qml-smoke-test|settings-structure-test)$'
```

Expected: all three Settings cards, proxy bindings, watcher reset, previews, and structural invariants pass.

---

### Task 7: Verify all shared consumers and close the implementation

**Files:**
- No new production files; review the complete diff for unintended changes.

- [x] **Step 1: Run the required focused consumer suite**

Run:

```bash
ctest --test-dir build \
  -R '^(theme-controller-test|settings-qml-smoke-test|settings-structure-test|astrea-app-icon-qml-test|astrea-icon-provider-test|dock-hover-qml-test|alttab-qml-selection-test)$' \
  --output-on-failure
```

Confirm the shared icon API loads through Dock, Alt+Tab, and Spotlight's shell-embedded QML harness without changing provider/source-theme tests.

- [x] **Step 2: Build and lint the existing tree**

Run:

```bash
cmake --build build --parallel 4
cmake --build build --target astrea-settings-ui_qmllint --parallel 4
```

Record registered/linted QML counts and warning/error counts.

- [x] **Step 3: Run documented Settings audits**

Run every audit in `Settings/docs/TESTING.md`, including deprecated form paths, inline Dock sliders, nine native Slider consumers, controller detents, `writePersonalization()`, and the scoped production dependency scan. Report any pre-existing out-of-scope result without changing it.

- [x] **Step 4: Run the complete suite and diff checks**

Run:

```bash
ctest --test-dir build --output-on-failure
git diff --check
git status --short
```

Expected: all available tests pass, whitespace checks are clean, the existing build directory is reused, and unrelated pre-existing dirty files remain preserved.

- [x] **Step 5: Report limitations and manual qualification path**

Provide the exact freshly built `build/Settings/astrea-settings` path. State explicitly that Dark/Clear are not implemented; legacy icon fields and source-theme resolution remain independent; v1 modes are generated over resolved source artwork; and desktop visual qualification remains a user-side check across Light/Dark, Dock, Spotlight, Alt+Tab, SVG/raster/fallback icons, accent changes, and restoration to Default.
