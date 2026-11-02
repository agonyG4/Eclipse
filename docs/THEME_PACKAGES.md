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
SDK consumers without a `ThemeController`. By default packages remain data only. The
optional API 1 panel components below execute package QML only when the application is
started with the explicit `ASTREA_ALLOW_THEME_QML=1` environment opt-in.

## Optional QML panels (API 1)

The example at `themes/examples/qml-panels` replaces only Spotlight and Alt+Tab inner
panels. Install the directory as `~/.local/share/AstreaOS/themes/qml-panels/`, select
`qml-panels` as the visual theme, then launch the application with the opt-in set, for
example `ASTREA_ALLOW_THEME_QML=1 astrea-shell`. Selecting a package alone never runs
its QML. The opt-in must be present when the process starts; toggling it later has no
effect. No user configuration or packaged service enables it automatically.

The manifest may add:

```json
"components": {
  "apiVersion": 1,
  "spotlightPanel": "qml/SpotlightPanel.qml",
  "altTabPanel": "qml/AltTabPanel.qml"
}
```

The resolver accepts only existing canonical `.qml` files inside the package directory.
Absolute paths, URLs, traversal, symlink escapes, unknown component names and unsupported
API versions are rejected. A rejected or missing component leaves the built-in panel in
place. `Astrea.Shared.ThemeComponentLoader` is public in the installed SDK and accepts
`source`, `fallbackComponent`, optional `initialProperties` and optional `validateItem`;
its `item`, `usingFallback` and `error` properties report the active content and status.
The requested source stays bound while a failed external item is discarded.

Each external panel must declare `readonly property int themeApiVersion: 1` and a
`controller` property. Spotlight receives `SpotlightController`; it must provide a
callable `focusSearch()` and a finite, nonnegative `radius`. Alt+Tab receives
`AltTabController` and `AltTabWindowModel` as `windowModel`; it must provide a non-null
Item `backdropItem` (the rounded background). Use `Astrea.Shared.ThemeTokens` to follow
the selected style tokens. Keep the panel's geometry within its loader and provide
keyboard focus in the panel: Spotlight focus moves to search, while Alt+Tab focus handles
selection and commit/cancel keys. Backdrop regions track the loaded background and use
zero radius when a custom radius is invalid. Native windows, focus cancellation, surface
readiness, LayerShell lifecycle, compositor adapters and hit testing remain host-owned.

The only supported executable theme surfaces are Spotlight and Alt+Tab panels. Dock,
Bar and Settings layout replacement is not supported; their independent shared-token
customization remains available. External QML runs as trusted application code with the
Shell's permissions and access to its QML engine. This API is not a sandbox and packages
must come from sources the user trusts.

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



## Astrea Typography System

Borealis defines six semantic text roles. ThemeController resolves each role once
and publishes the same effective map to every Eclipse process. Qt's platform font
matcher (Fontconfig on Linux) handles unavailable families and glyph fallback.

| Role | Default size | Default Qt weight | Line height |
| --- | ---: | ---: | ---: |
| display | 24 px | 600 | 1× |
| title | 18 px | 500 | 1× |
| body | 14 px | 400 | 1× |
| label | 13 px | 500 | 1× |
| caption | 11 px | 400 | 1× |
| mono | 13 px | 400 | 1× |

A theme may set primary/mono families and override individual roles:

```json
{
  "style": {
    "typography": {
      "fontFamily": "Inter",
      "monoFontFamily": "JetBrains Mono",
      "roles": {
        "title": { "pixelSize": 18, "weight": 500, "letterSpacing": -0.2, "lineHeight": 1 },
        "body": { "pixelSize": 14, "weight": 400, "lineHeight": 1 }
      }
    }
  }
}
```

`roles` accepts `display`, `title`, `body`, `label`, `caption` and `mono`.
Each supports `family` (the same validation as other families), integral
`pixelSize` 1–96, integral Qt `weight` 1–1000, `letterSpacing` −5–20 px and
proportional `lineHeight` 1–3. Values must be finite JSON numbers; invalid fields
are ignored individually. Unspecified fields inherit Borealis. Legacy typography
tokens remain supported: explicit semantic role sizes take precedence over their
legacy equivalents; otherwise legacy metrics retain their compatibility defaults.
Built-in text components choose semantic roles directly.

Settings → Customization → Typography changes only user preferences, stored beside
`theme.json` in `typography.json`:

```json
{
  "schema": 1,
  "preferences": {
    "fontFamily": "Inter",
    "monoFontFamily": "JetBrains Mono",
    "textScale": 1.25,
    "roles": { "caption": { "pixelSize": 12 } }
  }
}
```

User overrides win over theme values. Primary font changes are inherited by Body,
Display and Title, including when the theme specified different families, unless
the user overrides the corresponding family or individual role. Mono remains
independent. Scale is 0.75–2.0 and is applied once after resolving base sizes; it
does not scale icons, borders, radii or spacing. Advanced sizes are base pixels,
not already-scaled pixels.

Preferences retain unavailable families, letting Qt supply fallback without
forgetting the user's choice. Per-field locked, atomic writes preserve other
writers and extension keys. Existing `theme.json.font_preferences` are read until
the first typography write migrates them. Reset removes known overrides and
retains an empty preference document so old overrides do not return; neither
operation changes the theme package or writes `theme.json`. Malformed documents
are preserved and the Settings page reports persistence errors.

QML consumers import `Astrea.Shared` and use `DisplayText`, `TitleText`, `BodyText`,
`LabelText`, `CaptionText` or `MonoText`, setting presentation properties such as
text, color, wrapMode and layout constraints normally. `Typography.role(name)`
returns family, scaled pixelSize, weight, letterSpacing and lineHeight for custom
controls. `TypographyText.typographyRole` allows dynamic role selection.

The migration covers Settings text and form/control primitives, Spotlight search
and results, Shell/Bar text adapters and menus, Dock badges and tooltips. AltTab
contains icon tiles rather than text labels. Paper's present QML is a wallpaper
surface; Downloads has no QML typography. This checkout does not contain an
Explorer application to migrate. Remaining literal text sizes in Settings are
icon/symbol glyphs and miniature material demonstrations. Dock preview close
symbols and theme-example search glyphs also intentionally keep icon metrics.
Theme-supplied replacement QML must adopt the semantic components or tokens to
participate in the contract.


### Per-surface fine tuning

Built-in proportional text uses the shared `Typography.font(role)` factory and
native text rendering. The clock uses Caption for the date and Label for the
time; Spotlight uses Title for search and Body for results. They inherit the
primary family unless the theme or user intentionally overrides a role.

Themes may refine metrics for `topbar`, `spotlight`, `dock`, `menu` and `settings`
without adding fonts to application code. Each surface supports the same six
roles and the same field validation as global typography:

```json
{
  "style": {
    "typography": {
      "fontFamily": "SF Pro",
      "monoFontFamily": "SF Mono",
      "roles": {
        "body": { "pixelSize": 14, "weight": 400, "lineHeight": 1 },
        "label": { "pixelSize": 13, "weight": 500 }
      },
      "surfaces": {
        "topbar": {
          "caption": { "pixelSize": 11, "weight": 400, "letterSpacing": 0.125 },
          "label": { "pixelSize": 13, "weight": 500, "letterSpacing": 0.25 }
        },
        "spotlight": {
          "title": { "pixelSize": 22, "weight": 400, "letterSpacing": -0.125, "lineHeight": 1 },
          "body": { "pixelSize": 14, "weight": 400, "lineHeight": 1 }
        },
        "dock": { "label": { "pixelSize": 13, "weight": 500 } },
        "menu": { "label": { "pixelSize": 13, "weight": 500 } }
      }
    }
  }
}
```

Resolution order is Borealis defaults → legacy/global theme tokens → theme
semantic roles → theme surface refinements → user overrides → text scale.
Unspecified surface fields inherit global roles; unknown surfaces and invalid
fields are ignored. All user role overrides also apply to surface styles, and a
user primary-family override replaces theme surface families unless the user
explicitly chooses another family for a role. Scaling happens exactly once.

Manifest edits propagate live through ThemeController. Menus measure the exact
resolved QFont used by their labels and shortcuts, so weight and tracking changes
participate in width calculation. Dock title and tooltip containers adapt to
text metrics. Icon fonts and icon dimensions remain separate.

Custom QML uses `ThemeTokens { typographySurface: "topbar" }` or
`Typography { surface: "spotlight" }`, then reads `typography.role("title")` for
lineHeight and `typography.font("title")` for a QFont including family, size,
weight, tracking, kerning and hinting. An optional second argument to `font()`
preserves an existing explicit component-family override. Text components use
`Text.NativeRendering` (or `TextInput.NativeRendering`) for consistent rendering.


Settings sidebar navigation uses the Label role for all font properties, in both
selected and unselected states. Body controls paragraph/body text; changing its
family or size does not resize navigation labels. Semantic fonts clear inherited
platform style names such as Regular before resolving numeric weights, so the
chosen weight selects the corresponding installed face rather than retaining a
fixed platform face. Settings applies the same policy to its application font.
