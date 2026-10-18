# Settings and Shell Navigation Icons

The interface supports exactly two navigation icon families:

- **Lucide** (`lucide`): the default. Open-source SVG assets bundled with the
  application for offline use. Supports nine geometric stroke weights.
- **AppleOS** (`apple`): optional user-installed SVGs. On the local Kaiser
  machine, files are under `~/.local/share/AstreaOS/Themes/AppleOS/`
  and were extracted from https://sfsymbols.bhodges.me/.
  Apple's proprietary artwork is **not bundled** in the public repository.

Preferences live in `~/.config/AstreaOS/ui/navigation-icons.json`:

```json
{
  "theme": "lucide",
  "weight": "regular"
}
```

Only `lucide` and `apple` are accepted. Legacy icon theme IDs such as
`astrea`, `eclipse` and custom icon packs resolve to Lucide by default.
Desktop/application icons and visual theme packages are separate systems.

## Lookup

Settings uses `assets/icons/navigation/lucide/` for the built-in family,
and Shell uses `Bar/assets/icons/lucide/`. Both controllers share the preference.

For AppleOS, Settings reads `AppleOS/<semantic-key>.svg` while Shell reads
`AppleOS/bar-<semantic-key>.svg`. Missing AppleOS assets fall back to
the bundled Lucide resource. Supported per-icon overrides may select a weight,
semantic key, theme, or safe local SVG without enabling extra icon families.

## Weights

Lucide provides Ultralight, Thin, Light, Regular, Medium, Semibold, Bold,
Heavy, and Black. To regenerate:

```sh
python tools/icons/generate-lucide-icon-weights.py
```

AppleOS uses the original SVG from the site regardless of chosen weight.
The stroke weight selector is only visible for Lucide. No modified versions
of Apple's proprietary symbols are generated.

## Licensing

Bundled Lucide is based on `lucide-static` v0.468.0, ISC-licensed.
See adjacent license files. Apple's symbols remain subject to Apple licensing:
do not commit or redistribute the local AppleOS folder as a public Linux
icon pack. The previous native Astrea family and old installed icon packs
were archived outside the repo before removal.
