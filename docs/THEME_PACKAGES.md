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
  "icons": { "settings": "icons/settings", "shell": "icons/shell" },
  "style": {
    "colors": {
      "accent": "#c38fff",
      "textPrimary": "#f3eafb",
      "textSecondary": "#cab5dc",
      "cardBg": "#211b2c",
      "cardBorder": "#42364f",
      "shellBackground": "#18131f",
      "shellSurface": "#251e30",
      "shellBorder": "#4a3f55",
      "shellTextMain": "#f3eafb",
      "shellIconMain": "#f3eafb"
    },
    "fontFamily": "Inter",
    "animationFast": 115
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

Theme `style` values are optional; missing values inherit Astrea defaults.
The v1 runtime supports the documented semantic colors, one installed `fontFamily`,
and `animationFast` (0–2000 ms). Theme packages are data only: arbitrary QML/script
execution, unrestricted compositor effects, full layout replacement and per-component
QML plugins are not implemented in v1. This intentionally avoids executing untrusted
code inside the compositor. Those richer component APIs need a separate versioned,
trusted extension mechanism.

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

