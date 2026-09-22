# Settings Migration Notes

The legacy Settings source remains the visual reference. This refactor changes
native ownership and build ownership without redesigning the approved shell.

## Preserved

- Inter and JetBrains Mono font names;
- legacy typography, radius, spacing, and animation scales;
- dark/light palette intent and glass shell treatment;
- the 256 px sidebar, profile header, navigation row geometry, and selected
  accent treatment;
- native frameless desktop behavior and `startSystemMove()`;
- form cards, setting rows, toggles, selectors, section headers, and the
  Compositor preview layout;
- the exact navigation order, Compositor placement after Services, and the
  approved legacy Wallpaper page hierarchy.

The feedback components were repaired from their canonical Astrea sources. They
are registered compatibility components and are not used by the current visible
shell.

## Native Boundaries

- application lifecycle and QML startup: `app/SettingsApplication.*`;
- navigation descriptors: `core/navigation/SettingsNavigationCatalog.*`;
- sidebar-visible rows, catalogue lookup, and child/ancestor resolution:
  `core/navigation/SettingsNavigationModel.*`;
- stable QML facade: `core/SettingsController.*`;
- profile value and provider: `services/profile/`;
- icon URL resolution: `services/assets/`;
- Rust Appearance mutation boundary: `backend/src/appearance/` through the
  generated `SettingsAppearanceController` QObject;
- installed icon-theme domain: `backend/src/icons/` through the generated
  `SettingsIconsController` QObject;
- shared `theme.json` lock and atomic transaction primitive:
  `backend/src/theme_config/ThemeConfigStore`;
- shared theme and Settings translations: `shared/theme/` and `services/i18n/`;
- wallpaper presentation/controller boundary: `qml/pages/appearance/Wallpaper.qml`
  and `services/wallpaper/SettingsWallpaperController.*`;
- libc/NSS and Linux account policy: `platform/linux/`;
- presentation and interaction: `qml/`;
- Animations backend domain, protocol, discovery, transport, and QObject
  projection: `backend/`.

No service or platform construction occurs in QML.

## Incremental Rust backend migration

Animations is the first production Settings backend migrated from manually
maintained C++ to Rust through CXX-Qt. `Animations.qml` and
`SettingsController.animations` remain unchanged at the behavior boundary.
The generated QObject is deliberately thin: it projects the existing
properties, notify signals, and invokables and queues worker completions onto
the QObject's Qt thread. The Rust crate owns typed animation state, validation,
capability projection, the version-1 `astrea.control` protocol, secure Typhon
endpoint discovery, and bounded Unix-socket transport.

Customization Phase 1 additionally moves mutations of `theme_preference`,
`accent`, and `icon_appearance` to Rust Appearance. Its generated QObject is a
thin Qt boundary; a single bounded worker coalesces rapid changes and
`ThemeConfigStore` preserves unrelated keys under the shared `theme.json.lock`
protocol. A successful commit signals the application composition root to
reload the existing C++ ThemeController projection.

The ownership after Phase 1 is:

```text
QML = presentation and interaction
Rust Appearance = theme_preference / accent / icon_appearance mutation semantics
Rust Icons = installed icon-theme domain and system_icon_theme
ThemeConfigStore = canonical Rust transaction primitive for theme.json
ThemeController C++ = live Qt reader/watcher/projection and transitional writer
                      for remaining unmigrated legacy keys
Visual Effects = transitional shell_style UI until Phase 2
```

This phase does not migrate `shell_style`, ThemeController as a whole, Dock,
Wallpaper, navigation, Shell services, Audio, Bluetooth, Network, Typhon
itself, or the preview-only Compositor page. Phase 2 will migrate the material
and effects control plane. Continuous material sliders, advanced overrides,
blur parameters, shaders, refraction, noise, and Typhon effects integration
are not current capabilities.

## Icons Ownership

The v1 Icons destination manages installed Freedesktop icon themes only.
QML is limited to filtering, card presentation, focus, and immediate
selection. Rust owns discovery, metadata parsing, validation, preview lookup,
background scanning, error state, and atomic `system_icon_theme` persistence
through `ThemeConfigStore`. CXX-Qt is a thin QObject projection. Shared C++ owns
only Qt/QIcon rendering integration and the existing watcher/cache
invalidation path.

`icon_theme` remains the legacy Settings navigation/resource preference used by
`SettingsIconResolver`; `system_icon_theme` is the canonical system icon-pack
preference. They must not be merged, and `iconAppearance` remains the separate
Default/Monochrome/Tinted presentation setting.

The Customization hub children are ordered Appearance, Visual Effects, Icons,
Wallpaper, Dock, and Animations. Appearance shows the single built-in Astrea
System Theme card and does not persist a preset selection. Visual Effects
temporarily owns only the existing Default, Transparent, and Frosted
`shell_style` controls. Phase 2 replaces these discrete controls with the
material/effects control plane; none of its future slider or shader controls
are implemented here.

## Routing Policy

The navigation catalogue contains stable IDs and optional native `QUrl` page
descriptors. Page, Hub, and Spacer rows preserve the approved flat sidebar
order; nested destinations remain in the authoritative catalogue with a
`parentId` and `sidebarVisible=false`. A Hub is navigable only when it has at
least one navigable child. A Page without a route remains visible but is not
selectable, so the first navigable descriptor (currently Compositor) is the
initial selection. The controller owns the selected route, ancestor highlight,
and bounded session-only Back/Forward history. Future pages must add a
descriptor and QML source through the catalogue; numeric page indexes and QML
route-ID conditions are prohibited.

## Compositor Preview Policy

Compositor controls are local-only QML properties. They are not persisted,
applied, read from a backend, or sent through IPC. Leaving the page destroys the
values and recreates the approved defaults on return.

## Wallpaper Route Policy

Wallpaper keeps the legacy Astrea visual hierarchy while using native Settings
and Paper boundaries. Change imports and selects through Paper; User Wallpapers
`+` adds to the Paper-managed catalog without changing the active wallpaper.
Stable content-addressed IDs remain authoritative, while user-facing names are
persisted as Paper-owned metadata. Transition rendering, blur, per-workspace
wallpapers, Screensaver, Lockscreen, dynamic execution, and full historical
landscape asset migration remain deferred follow-up work. If a user metadata
sidecar is missing or malformed, Paper leaves the display name empty so Settings
can provide a localized generic label; digest IDs are never shown as names.

Wallpaper library correctness is also native-owned: Paper projects a preview
`QUrl` for Settings while retaining raw source and resolved-source metadata,
and QML consumes only those projections. The bounded Paper actions are:
`wallpaper get`, `list`, `import`, `add`, `set`, `remove`, `reset`, and
`default`. Remove accepts a stable logical ID only, removes only a managed user
image and its sidecar inside Paper's configured library, rejects the active
wallpaper, and never deletes the original import source. An inactive removal
does not change configured/effective selection, generation, fit, watchers, or
wallpaper-change signals.

## Source Handoff Policy

Use only `tools/create-source-archive`. It requires `main` and a clean worktree
by default, archives `HEAD` with `git archive`, writes outside the checkout by
default, and excludes filesystem build artifacts because it does not archive the
checkout directory.
