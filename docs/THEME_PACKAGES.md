> Navigation icon families are limited to Lucide and locally installed AppleOS. The descriptions below refer to legacy/icon-package designs and the separate visual-theme extensibility surface.

# Eclipse Theme Packages (Declarative v1)

A theme package is an installed directory containing a versioned manifest and optional assets.
Packages let users choose a visual theme and a navigation icon pack **independently**.
An icon pack can replace the entire icon set; the compositor and application code do not have
to be rebuilt. Unprovided icons retain the existing UI fallback glyphs.

## Install a local theme

Copy a directory such as `themes/examples/minimal` to
`~/.local/share/AstreaOS/themes/minimal/`, keeping its `theme.json` and `icons/` contents.
Then reopen Settings → Customization → Appearance → **Visual Theme Package** to choose
visual styling, and Settings → Customization → Icons → **Settings Icons** to independently
choose the icon pack. Installed theme packages are also searched under system XDG data roots.

## Manifest format

```json
{
  "schema": 1,
  "id": "minimal",
  "name": "Minimal",
  "style": {
    "colors": {
      "accent": "#c38fff",
      "accentForeground": "#180d22",
      "textPrimary": "#f3eafb",
      "textSecondary": "#cab5dc",
      "cardBg": "#211b2c",
      "cardBorder": "#42364f",
      "shellBackground": "#18131f",
      "shellSurface": "#251e30",
      "shellSurfaceElevated": "#30263d",
      "shellBorder": "#4a3f55",
      "shellTextMain": "#f3eafb",
      "shellIconMain": "#f3eafb",
      "tooltipSurface": "#30263d",
      "tooltipBorder": "#4a3f55"
    },
    "metrics": { "radiusSmall": 6, "shellRadiusLarge": 14, "spacing": 8 },
    "typography": {
      "fontFamily": "Inter",
      "fontSizeBody": 14,
      "tooltipFontSize": 13
    },
    "durations": { "animationFast": 115, "animationPopover": 180 }
  }
}
```

The `id` must match its folder name and contain lowercase letters, digits and hyphens,
starting with a letter. Icon directories are relative paths inside the package.
Only semantic keys such as `system`, `audio`, `wifi`, and `volume-high` are requested by
the UI. An icon is `icons/settings/<key>.svg`, `icons/shell/<key>.svg`, or an equivalent
`.png` file. Weight-specific resources live at
`icons/{settings,shell}/weights/<weight>/<key>.svg`. Missing icons fall back to the
existing UI icon, including Tabler in Settings and legacy glyphs in Shell.
Broken manifests, invalid paths and resource paths escaping a package are rejected.

Theme `style` values are optional; missing or invalid values inherit each component's
existing Astrea defaults. Colors accept `#RRGGBB` or `#AARRGGBB`. Supported color roles
are `accent`, `accentForeground`, `textPrimary`, `textSecondary`, `textTertiary`,
`cardBg`, `cardBorder`, `popupBg`, `windowBackground`, `windowBorder`, `windowWash`,
`errorColor`, `warningColor`, `successColor`, `shellBackground`, `shellSurface`,
`shellSurfaceElevated`, `shellBorder`, `shellBorderHover`, `shellHover`, `shellPressed`,
`shellActive`, `shellTextMain`, `shellTextSecondary`, `shellTextLight`, `shellTextDim`,
`shellTextActive`, `shellIconMain`, `shellIconActive`, `shellIconMuted`,
`shellIconWarning`, `shellIconAccent`, `shellSeparator`, `shellShadow`,
`tooltipSurface`, `tooltipBorder`, `tooltipShadow`, `workspaceActive`, and
`workspaceInactive`.

`style.metrics` accepts finite numbers from 0 through 256 for `radiusSmall`,
`radiusMedium`, `radiusLarge`, `cornerRadiusSmall`, `cornerRadius`, `cornerRadiusLarge`,
`cardRadius`, `controlRadius`, `shellRadiusSmall`, `shellRadiusMedium`,
`shellRadiusLarge`, `shellControlRadius`, `shellTileRadius`, `shellPillRadius`,
`spacingTiny`, `spacingMicro`, `spacingSmall`, `spacing`, `spacingMedium`,
`spacingLarge`, `spacingXLarge`, `pageMargin`, `dockRadius`, `spotlightRadius`,
`altTabRadius`, and `contextMenuRadius`.

`style.typography` accepts nonempty family names (up to 80 characters, without path
separators or control characters) for `fontFamily`, `monoFontFamily`,
`fontFamilyDisplay`, `fontFamilyText`, and `iconFontFamily`. Its integer size keys from
1 through 96 are `fontSizeHero`, `fontSizeHeader`, `fontSizeIconLarge`,
`fontSizeAvatar`, `fontSizeSubtitle`, `fontSizeTitle`, `fontSizeLarge`,
`fontSizeNormal`, `fontSizeSmall`, `fontSizeTiny`, `fontSizeMicro`, `fontSizeBody`,
`fontSizeCaption`, `fontSizeExtraSmall`, `fontSizeIcon`, and `tooltipFontSize`.
`style.durations` accepts integer milliseconds from 0 through 2000 for
`animationMicro`, `animationQuick`, `animationFast`, `animationNormal`,
`animationSlow`, `animationPopover`, `animationHover`, `animationSubtle`,
`animationStandard`, `animationInstant`, `animationSlider`, `animationPulse`, and
`animationSpin`. The original top-level `style.fontFamily` and
`style.animationFast` remain supported; valid nested typography/duration values take
precedence. Numeric zero is a valid override. Unknown keys and invalid values are
ignored independently, so partial themes retain valid overrides.

The shared `ThemeTokens` QML object resolves a token against a component's own fallback
and follows live `ThemeController` updates when one is present. It can also be used by
SDK consumers without a `ThemeController`. Theme packages are data only: arbitrary
QML/script execution, unrestricted compositor effects, full layout replacement and
per-component QML plugins are not implemented. This avoids executing untrusted code
inside the compositor; richer component APIs need a separate versioned, trusted
extension mechanism.

Visual theme and icon pack preferences are stored independently in
`~/.config/AstreaOS/ui/navigation-icons.json` as `visual_theme`, `theme`, and `weight`.
The Shell watches this file and updates while running; Settings applies its icon pack
choice immediately. Installed custom SVG files stay outside the source tree.

## Built-in Apple icon assets

The Apple-style pack is based on ISC-licensed Lucide, **not** Apple's SF Symbols.
The 45 original SVG source files retain their upstream `stroke-width="2"`.
`python tools/icons/generate-apple-icon-weights.py` generates all nine weights,
including a dedicated **Thin** variant; **Regular** (2) is the default, under `weights/<weight>/` without modifying
the source files.

## Per-icon overrides in `navigation-icons.json`

Each icon may override the selected weight, source icon, and pack.
Unconfigured icons inherit the globally selected `theme` and `weight`.
Changes to the JSON are watched by the running Shell.

```json
{
  "theme": "apple",
  "weight": "regular",
  "visual_theme": "minimal",
  "overrides": {
    "shell": {
      "wifi": "thin",
      "ethernet": "regular",
      "bluetooth": { "weight": "light", "icon": "radio" },
      "volume": { "weight": "medium" }
    },
    "settings": {
      "audio": { "weight": "bold" },
      "display": { "theme": "minimal", "icon": "system" },
      "network": { "file": "personal-network.svg" }
    }
  }
}
```

A value is either a string weight or an object with optional `weight`,
`theme`, `icon` (semantic name in that pack), and `file` (custom SVG or PNG).
Flat keys directly inside `overrides` are also supported, with scoped
`shell` and `settings` settings taking precedence.

The internal Ethernet icon is `wired`, but `ethernet` is a recognized alias.
When a dedicated override is missing, `wifi-off` inherits `wifi`,
`bluetooth-off` inherits `bluetooth`, and all `volume-*` inherit `volume`.

`file` is relative to `$XDG_DATA_HOME/AstreaOS/icon-overrides/shell/`
or `$XDG_DATA_HOME/AstreaOS/icon-overrides/settings/`, not the JSON directory.
Only local SVG/PNG files inside the corresponding directory are accepted;
traversal and symlinks escaping the directory are rejected. Unsupported
override fields fall back to defaults.

The running Shell reacts to manual JSON edits. Reopen Settings if the JSON
was manually edited while its window was open. Settings UI actions preserve
the entire `overrides` object when changing global theme and weight.

