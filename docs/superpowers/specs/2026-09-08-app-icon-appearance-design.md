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

## Shared rendering

`AstreaAppIcon.qml` remains the only presentation implementation. Its existing source URL, resolution-aware request sizing, fallback text, and rounded-alpha behavior remain unchanged.

The existing rounded shader is extended with presentation uniforms. The effect is active when rounded presentation is needed or when the mode is not `default`; the direct `Image` path remains active for the unchanged `default`/unrounded case. The shader applies the selected RGB transform before the existing rounded coverage mask and leaves alpha multiplied only by the coverage mask. This keeps source-theme resolution, caching, fallback behavior, and presentation concerns separate.

The active controller's `iconAppearanceChanged` signal drives the QML binding, so Dock, Spotlight, and Alt+Tab update live because they all instantiate the shared component. No per-consumer mode logic is added.

Dark and Clear are explicitly out of scope. No opacity-only or arbitrary darkening approximation is introduced for those modes.

## Settings UI

The existing Appearance page gains an `APP ICONS` section below the current Interface Style section. It contains exactly three focusable choices:

- Default
- Monochrome
- Tinted

The choices use stable `appIconAppearanceOption-default`, `appIconAppearanceOption-monochrome`, and `appIconAppearanceOption-tinted` object names. They use the existing card interaction conventions: selected state, hover/focus styling, pointer activation, Space/Enter activation, and the shared controller-backed save path. The existing Appearance, Interface Style, and Accent Color behavior remains unchanged.

Translations are added under `apps.settings.pages.appearance.text` and `apps.settings.pages.appearance.option`.

## Tests

Test-first coverage will include:

1. `ThemeController` unit tests for the default, accepted values, invalid-value normalization, JSON persistence, external reload, and preservation of legacy icon fields.
2. Shared `AstreaAppIcon` QML tests proving the default path is unchanged, monochrome and tinted activate the shared effect, mode changes update live, and the source URL/resolution target remains stable.
3. Settings QML smoke coverage for all three choices, selected-state transitions, controller persistence, external watched-file propagation, and preservation of theme/accent/shell state.
4. Structural checks for the Appearance section, stable object names, translation keys, and no changes to the source-theme or legacy icon ownership paths.

## Scope exclusions

- No Dark or Clear modes.
- No changes to `AstreaIconTheme` or `AstreaIconProvider` source resolution.
- No repurposing, migration, or removal of `iconStyle` or `iconTheme`.
- No duplicated presentation logic in Dock, Spotlight, or Alt+Tab.
- No global icon preview or unrelated Settings icon redesign.
