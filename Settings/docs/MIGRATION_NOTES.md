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
- model selection and section expansion: `core/navigation/SettingsNavigationModel.*`;
- stable QML facade: `core/SettingsController.*`;
- profile value and provider: `services/profile/`;
- icon URL resolution: `services/assets/`;
- shared theme and Settings translations: `shared/theme/` and `services/i18n/`;
- wallpaper presentation/controller boundary: `qml/pages/appearance/Wallpaper.qml`
  and `services/wallpaper/SettingsWallpaperController.*`;
- libc/NSS and Linux account policy: `platform/linux/`;
- presentation and interaction: `qml/`.

No service or platform construction occurs in QML.

## Routing Policy

The navigation catalogue contains stable IDs and optional native `QUrl` page
descriptors. Page, Section, Child, and Spacer rows preserve the approved order.
Sections toggle by stable ID and are never selected; Wallpaper and Dock are
children of Appearance. A Page or Child without a route remains visible but is
not selectable, so the first routable descriptor (currently Compositor) is the
initial selection. Compositor is a visual/local preview route; Wallpaper and
Dock are native routes backed by their focused C++ boundaries. Future pages
must add a descriptor and QML source through the catalogue; numeric page
indexes and QML route-ID conditions are prohibited.

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

## Source Handoff Policy

Use only `tools/create-source-archive`. It requires `main` and a clean worktree
by default, archives `HEAD` with `git archive`, writes outside the checkout by
default, and excludes filesystem build artifacts because it does not archive the
checkout directory.
