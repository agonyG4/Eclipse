# System-wide App Icon Appearance Design

**Date:** 2026-09-08

## Goal

Add a shared app-icon presentation preference with exactly three v1 modes—`default`, `monochrome`, and `tinted`—and expose it under Settings > Customization > Appearance. The preference must update the live Shell surfaces and persist through the existing shared theme configuration.

## Context and ownership

The shared `ThemeController` in `shared/theme` is used by the Shell runtime and the Settings application. It already owns the shared theme JSON path, file watching, reload signals, and persistence. The Shell injects that controller into the common QML context.

Application icons resolve through `AstreaIconTheme` and `AstreaIconProvider`, then render through `shared/qml/AstreaAppIcon.qml`. Dock, Spotlight, and Alt+Tab all use that shared QML component, so presentation belongs there. Source-theme lookup remains independent and does not read or mutate the new preference.

The legacy `iconStyle` and `iconTheme` properties remain unchanged. They continue serving Settings-specific icon behavior and are not migrated or repurposed.

## Canonical preference

`ThemeController` gains:

```cpp
Q_PROPERTY(QString iconAppearance
           READ iconAppearance
           WRITE setIconAppearance
           NOTIFY iconAppearanceChanged)
```

The accepted canonical values are:

| Value | Meaning |
| --- | --- |
| `default` | Render the resolved source icon without a presentation transform. |
| `monochrome` | Convert source RGB to luminance grayscale while preserving source alpha. |
| `tinted` | Use the active accent color modulated by source luminance while preserving source alpha. |

The default is `default`. Missing, empty, or invalid persisted values normalize to `default`. The persisted JSON key is `icon_appearance`; existing `icon_style` and `icon_theme` keys remain intact.

The Settings QML state projection exposes the same property and an explicit `setIconAppearance(value)` mutation function. The Appearance page uses that function and the existing `save()` function; it never assigns the projected property directly.

`AstreaAppIcon` exposes a small presentation contract for deterministic previews and tests:

```qml
property string appearanceOverride: ""
property color tintColorOverride
property bool hasTintColorOverride: false
readonly property string effectiveAppearance
readonly property color effectiveTintColor
```

An empty override follows the optional global `ThemeController`; valid non-empty overrides take precedence; invalid overrides resolve to `default`. An explicit tint override is used only when `hasTintColorOverride` is true. Standalone consumers without `ThemeController` use `default` and the built-in blue fallback tint.

## Shared rendering

`AstreaAppIcon.qml` remains the only presentation implementation. Its existing source URL, resolution-aware request sizing, fallback text, and rounded-alpha behavior remain unchanged.

The existing rounded shader is extended with presentation uniforms, retaining one GPU fragment pass for rounding and appearance. The effect is active when rounded presentation is needed or when the mode is not `default`; the direct `Image` path remains active for the unchanged `default`/unrounded case. The shader samples the source once, applies the selected RGB transform, calculates the existing rounded coverage, then combines source alpha, coverage, and `qt_Opacity`. Transparent source pixels remain transparent. No `MultiEffect`, `ColorOverlay`, `ShaderEffectSource`, CPU recoloring, or provider cache invalidation is added.

The `monochrome` transform uses Rec.709-style luminance weighting and writes neutral grayscale. The `tinted` transform maps dark source luminance toward a dark form of the accent and bright luminance toward the full accent, retaining interior contrast. Both preserve source alpha apart from rounded-edge coverage. Fallback initials remain on the existing fallback path and are not treated as successfully rendered transformed artwork.

The active controller's `iconAppearanceChanged` signal drives the QML binding, so Dock, Spotlight, and Alt+Tab update live because they all instantiate the shared component. No per-consumer mode logic is added.

Dark and Clear are explicitly out of scope. No opacity-only or arbitrary darkening approximation is introduced for those modes.

## Settings UI

The existing Appearance page gains an `APP ICONS` section below the current Interface Style section. It contains exactly three focusable choices:

- Default
- Monochrome
- Tinted

The choices use stable `iconAppearance-default`, `iconAppearance-monochrome`, and `iconAppearance-tinted` object names. They use the existing card interaction conventions: selected state, hover/focus styling, pointer activation, Space/Enter activation, and the shared controller-backed save path. Each card previews the real `Shared.AstreaAppIcon` renderer with an explicit mode override and deterministic repository-owned colorful Settings artwork; the Tinted preview consumes the current Accent Color live. The existing Appearance, Interface Style, and Accent Color behavior remains unchanged.

Translations are added under `apps.settings.pages.appearance.text` and `apps.settings.pages.appearance.option`.

## Tests

Test-first coverage will include:

1. `ThemeController` unit tests for the default, accepted values, case normalization, invalid-value normalization, complete external replacement reset, JSON persistence, legacy-only compatibility, and preservation of legacy icon fields and unrelated theme state.
2. Shared `AstreaAppIcon` QML tests proving the default fast path is unchanged, monochrome and tinted activate one combined effect, rounding coexists with appearance, mode switching preserves source resolution targets, deterministic pixel luminance/alpha behavior, live controller/accent propagation, missing-controller fallback, and override precedence.
3. Settings QML smoke coverage for all three choices, selected-state transitions, controller persistence, external watched-file propagation, invalid/default fallback, no binding severing, and preservation of theme/accent/shell state.
4. Focused Dock, Alt+Tab, Spotlight, shared icon-provider, and structural checks to prove consumers remain loadable and source-theme ownership is unchanged.

## Scope exclusions

- No Dark or Clear modes.
- No changes to `AstreaIconTheme` or `AstreaIconProvider` source resolution.
- No repurposing, migration, or removal of `iconStyle` or `iconTheme`.
- No duplicated presentation logic in Dock, Spotlight, or Alt+Tab.
- No icon-theme chooser UI or unrelated Settings icon redesign.

## Documentation

The current architecture documentation will distinguish:

```text
icon_appearance
    = global Astrea application-icon presentation

icon_style / icon_theme
    = legacy Settings-specific navigation icon configuration

AstreaIconTheme
    = Freedesktop/QIcon source-theme resolution
```

The documentation will state that v1 Monochrome and Tinted are generated presentation modes over resolved default artwork, not dedicated adaptive icon assets. Dark and Clear remain unsupported.
