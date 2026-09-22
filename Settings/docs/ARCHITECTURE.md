# Astrea Settings Architecture

`astrea-settings` is a native Qt 6 application and a normal frameless Wayland
toplevel. QML / Qt Quick owns presentation and interaction. CXX-Qt is a thin
generated Qt boundary for migrated backends. Rust owns migrated backend,
domain, and system logic such as Animations/Typhon, Appearance preferences, and
installed icon themes. C++ owns application composition and remaining
unmigrated Qt/system services.

## Composition Root

`SettingsApplication` is the only composition root:

```text
AdminGroupDetector
  -> SettingsUserProfileProvider
      -> SettingsUserProfile
SettingsNavigationCatalog
  -> SettingsNavigationModel
SettingsIconResolver
  -> SettingsController
ThemeController
SettingsTranslationController
SettingsDockController -> shared DockConfigStore
SettingsController.animations
  -> CXX-Qt SettingsAnimationController QObject
      -> Rust animation state and typed Typhon client
SettingsController.appearance
  -> CXX-Qt SettingsAppearanceController QObject
      -> Rust Appearance mutation boundary
          -> ThemeConfigStore -> theme.json
SettingsController.icons
  -> CXX-Qt SettingsIconsController QObject
      -> Rust Icons domain
          -> ThemeConfigStore -> theme.json
VisualEffects.qml
  -> Components.Theme shellStyle bridge
      -> ThemeController::save() -> theme.json
theme.json
  -> ThemeController watcher and live QML/Shell/Dock/Bar projections
QQmlApplicationEngine
```

The application registers the stable context properties `SettingsController`,
`ThemeController`, and `I18n`, plus the existing `astrea-icon` image provider.
The provider is owned by the QML engine and is not duplicated as a context
property. It also owns metadata, QML startup, fatal warning reporting, and
exactly-one-root validation.

## Target Boundaries

`astrea-settings-core` is a reusable static native library. Its public link
interface is `Qt6::Core` and `astrea-shared-dock`; `Qt6::Network` is private.
The core contains the controller, navigation, services, and Linux account
implementation, includes the header-only Paper protocol directly, and links
the generated `astrea_settings_backend` target through the public QObject
header. The Rust target is
built by the supported CXX-Qt CMake integration; its generated QObject header
is consumed by the C++ composition root. It does not link Qt QML, Qt Quick,
Quick Controls, LayerShellQt, or a compositor library.

`Settings/backend` is intentionally focused. The CXX-Qt QObject only projects
typed Rust state into Qt-compatible properties, emits the existing notify
signals, accepts the existing invokables, and queues completion onto its Qt
thread. Pure Rust owns configuration mutation semantics, capability projection,
typed serde protocol structures, secure endpoint discovery, and one bounded
worker for Unix-socket I/O. JSON is used only at the Typhon wire boundary.

`Settings/backend/src/theme_config/ThemeConfigStore` is the shared Rust
transaction primitive for `theme.json`. It uses the existing `theme.json.lock`
advisory lock, protects malformed JSON, preserves unrelated fields, and
atomically replaces the file. Both Appearance and Icons domain wrappers use
this store; they never share domain semantics.

The Icons domain is limited to installed Freedesktop icon themes: Rust owns
discovery, `index.theme` metadata, selection, previews, the bounded worker, and
the `system_icon_theme` preference. Its resolver keeps split roots, first
authoritative metadata, Hidden filtering, inheritance, hicolor fallback,
explicit size/scale handling, exact-before-closest lookup, scoped previews,
deterministic sorting, optimistic selection, persisted intent,
newest-request-wins, and live `AstreaIconProvider` invalidation. CXX-Qt remains
the Qt property/signal/invokable boundary. The legacy Settings-only
`icon_theme` key and the Rust-owned `system_icon_theme` key stay separate.

`astrea-settings-ui` is the only `Astrea.Settings 1.0` QML module and registers
46 QML files. The application and QML integration tests consume that same
target and generated plugin. The application additionally links the existing
shared core and QML plugin for compositor-independent shared utilities.

The QML component taxonomy separates reusable interaction from page composition:
`components/controls/` owns interactive primitives, while `components/form/`
owns Settings-specific structural/layout composition. A semantic Qt Quick
Controls type is the preferred base for a new interactive primitive; replace
its visual delegates and retain the framework's pointer, keyboard, touch,
focus, range, and RTL behavior instead of implementing a second input state
machine with `MouseArea`.

`pages/appearance/MaterialPreview.qml` is the reusable preview surface for the
Appearance previews and the transitional Visual Effects choices. It consumes
the effective wallpaper snapshot projected by `SettingsController.wallpaper`.
`MaterialShowcase.qml` retains the existing fallback visual tree, while the
Frosted variant may show the existing public Qt Wayland-effects preview when
available. Visual Effects currently exposes only the three existing
`shell_style` modes. The future continuous material model and shader controls
belong to Phase 2 and are not current capabilities.

Unit tests link `astrea-settings-core`. Integration tests link both reusable
production targets. No test target lists a production `.cpp` file owned by the
core library.

## Dependency Direction

```text
app -> core -> services -> platform/linux
app -> qml context properties
qml -> presentation and interaction
tests -> production targets and explicit fakes
shared -> compositor-independent utilities and capability-gated Wayland effects
Rust -> migrated backend/domain/system logic such as typed animation state, Typhon protocol, secure discovery, and bounded transport
CXX-Qt -> thin generated Qt boundary and queued projection of migrated Rust backends
C++ -> application composition and remaining unmigrated Qt/system services
```

QML has no filesystem, process, IPC, DBus, or compositor API. Platform access
is implemented in the owning Rust or C++ backend boundary.

## Navigation and Routing

`SettingsNavigationCatalog` owns the authoritative descriptor catalogue:
stable IDs, visible metadata, Page/Hub/Spacer kind, parent relationships, and
optional `pageSource`. The model exposes only sidebar-visible rows while also
providing native lookup, child, ancestor, and first-destination helpers. A
destination is navigable only when it is enabled and has a valid route; a Hub
also requires at least one navigable child. `SettingsController` owns the
current destination, sidebar highlight, selected route, and session history.

`Main.qml` passes the native URL directly to a `Loader`. The catalogue is the
single source of truth for row order and page routing. There is no numeric page
index and no QML route-ID condition. The current routable descriptors are:

```text
qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Appearance.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/VisualEffects.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Icons.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Animations.qml
```

Rows without implemented pages remain visible but cannot be selected. The first
navigable sidebar destination is supplied by the catalogue and is currently
Compositor. The sidebar is flat: `Customization` remains highlighted while its
nested Wallpaper or Dock destination is open. Hub pages render their children
from controller metadata, and the controller maintains a bounded 64-entry
Back/Forward history without duplicate current entries. Leaving Compositor
destroys the page and recreates its local preview state when selected again.

## Native Ownership

`SettingsController` is the stable QML facade. It delegates navigation to
`SettingsNavigationModel`, profile values to an immutable `SettingsUserProfile`,
resource URL construction to `SettingsIconResolver`, and Dock personalization to
the focused `SettingsDockController`. The latter exposes typed validated
properties, canonical defaults sourced from shared `DockConfig`, debounced
atomic writes, bounded errors, a file watcher for external replacement, and
the bounded Restore Defaults/Undo state. It depends only on the shared
compositor-independent Dock configuration boundary; it does not reach into
Shell, Typhon, or LayerShellQt. QML remains presentation-only: the reusable
slider observes native Qt edits for detents and the Dock page formats values,
while the controller owns persistence and undo.

`SettingsUserProfileProvider` resolves the current username, the readable
AccountsService avatar path, and administrative membership. `AdminGroupDetector`
owns libc/NSS enumeration; `AdministrativeGroupPolicy` recognizes exactly
`wheel` and `sudo`.

`ThemeController` and `SettingsTranslationController` retain their existing
public QML names and semantics, but live under their service ownership paths.

### Animations backend migration

`SettingsController.animations` remains the authoritative QML-facing property.
`Animations.qml` does not know whether its backend is C++ or Rust. The first
production migration replaces the hand-maintained C++ animation controller and
Typhon client with a stable CXX-Qt 0.10 QObject backed by `Settings/backend`.
This is an incremental migration pattern, not an all-at-once C++ rewrite:
QML stays presentation, CXX-Qt stays thin, Rust owns the migrated backend
logic, and the remaining C++ composition and unrelated services are deferred.

### Application icon appearance

Rust Appearance is the Settings mutation boundary for `theme_preference`,
`accent`, and `icon_appearance`. It normalizes those values, patches only the
requested key through `ThemeConfigStore` on one bounded/coalescing worker, and
emits `configurationChanged()` only after a successful transaction. The
composition root connects that signal to `ThemeController.reload()`. The
ThemeController watcher and `Components.Theme` continue to expose the live
read-only projection used by the existing QML/Shell/Dock/Bar token system.
Invalid theme preferences resolve to `auto`, invalid icon appearance resolves
to `default`, and empty Accent resolves to `#0a84ff`.

This preference is independent from the legacy Settings-only `iconStyle` and
`iconTheme` fields. Those fields continue to serve Settings navigation icon
colorization and Settings resource resolution through `SettingsIconResolver`.
They do not select the source artwork for application icons. Source artwork is
still resolved by the shared `AstreaIconTheme`/`AstreaIconProvider` XDG icon
pipeline. The shared `AstreaAppIcon` then derives the v1 Monochrome and Tinted
presentations from that resolved artwork, so Dock, Spotlight, and Alt+Tab share
the same live result and Accent Color changes propagate to Tinted icons.

Dark and Clear app-icon modes are intentionally not supported in v1: they
require a richer icon-asset representation and are not approximated with
opacity or darkening filters.

### Icons

`Icons.qml` is a presentation-only installed-theme picker. It consumes the
`SettingsController.icons.iconThemes` descriptor list and never reads the
filesystem, environment, or theme metadata itself. Selecting a card writes
only `system_icon_theme` in `~/.config/AstreaOS/ui/theme.json`; System Default
removes that key. `AstreaIconTheme` applies the preference after explicit
`ASTREA_ICON_THEME` and `QS_ICON_THEME` overrides, while `AstreaIconProvider`
continues to own watcher-driven QIcon reapplication, cache invalidation, and
the shared `themeRevision` update used by Shell, Dock, Alt+Tab, and Settings.

### ThemeController transition

The C++ `ThemeController` remains the live theme reader, filesystem watcher,
and existing Qt projection. Its `save()` only writes the remaining unmigrated
legacy fields: `shell_style`, `icon_style`, `icon_theme`, and
`audio_osd_style`. It preserves `theme_preference`, `accent`,
`icon_appearance`, `system_icon_theme`, compatibility inputs `theme` and
`theme_mode`, and all unknown fields. `applyConfig()` may still read the legacy
compatibility values as fallback. The Visual Effects bridge uses the existing
`shellStyle`, `setShellStyle()`, and `save()` path until Phase 2 migrates the
material/effects control plane.

The single System Theme entry in Appearance is the built-in Astrea theme card;
there is no persisted preset key or alternate preset catalogue in Phase 1.

## Exclusions

Settings has no Quickshell import, LayerShellQt dependency, Hyprland command,
Typhon-private protocol, compositor backend, persistence for the Compositor
preview, or shell command execution. The Animations page does have the public
Astrea `astrea.control` Typhon IPC boundary described above. The shared public Wayland
effects module is a narrow Qt-owned child-surface bridge; it is independent of
Typhon and falls back cleanly when unsupported. The Dock page is a native route
under the Customization hub and its preview is presentation-only; it does not
import resident Dock QML or implement a second schema. Performance and More
Settings are visible but unavailable hubs until they receive navigable children.
