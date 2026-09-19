# Astrea Settings Architecture

`astrea-settings` is a native Qt 6 application and a normal frameless Wayland
toplevel. QML / Qt Quick owns presentation and interaction. CXX-Qt is a thin
generated Qt boundary for migrated backends. Rust owns migrated backend,
domain, and system logic such as Animations/Typhon. C++ owns application
composition and remaining unmigrated Qt/system services.

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

`astrea-settings-ui` is the only `Astrea.Settings 1.0` QML module and registers
44 QML files. The application and QML integration tests consume that same
target and generated plugin. The application additionally links the existing
shared core and QML plugin for compositor-independent shared utilities.

The QML component taxonomy separates reusable interaction from page composition:
`components/controls/` owns interactive primitives, while `components/form/`
owns Settings-specific structural/layout composition. A semantic Qt Quick
Controls type is the preferred base for a new interactive primitive; replace
its visual delegates and retain the framework's pointer, keyboard, touch,
focus, range, and RTL behavior instead of implementing a second input state
machine with `MouseArea`.

`pages/appearance/MaterialPreview.qml` is the reusable preview surface for
Appearance and Interface Style cards. It consumes the effective wallpaper
snapshot projected by `SettingsController.wallpaper`, including its preview
URL and fit mode. `MaterialShowcase.qml` owns the existing fallback visual
tree, while the Frosted variant additionally offers that same content through
the `Astrea.Effects.BackdropEffectSurface` child-surface primitive. The public
Wayland-effects service uses Qt's existing Wayland display and window surface;
it never opens a second display connection. If the compositor or protocol is
unavailable, the fallback showcase remains visible.

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
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Animations.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Wallpaper.qml
qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml
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

`ThemeController.iconAppearance` is the canonical global application-icon
presentation preference. It persists the lowercase `default`, `monochrome`, or
`tinted` value as `icon_appearance` in the shared theme configuration; missing
or invalid values resolve to `default`. The Settings `Theme`/`State` proxies
forward this property to the controller, and the Appearance page mutates it
through that path rather than binding a projected property directly.

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
