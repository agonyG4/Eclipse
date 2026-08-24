# Target Architecture

## 1. Architectural objective

The TopBar must become a first-class part of the unified `astrea-shell` process, using the same ownership model already established for Dock, Alt+Tab, Spotlight, Typhon, app launching, and shell IPC.

The immediate milestone is deliberately split between **foundation that must be correct now** and **system integrations that need dedicated follow-up milestones**.

## 2. Proposed ownership tree

```text
AstreaShellApplication
│
├── QQmlApplicationEngine
├── ShellRuntime
│   ├── DesktopEntryCatalog
│   ├── ApplicationLauncher
│   ├── TyphonSharedConnection
│   ├── TyphonToplevelConnection
│   ├── TyphonShortcutClient
│   ├── DockController
│   ├── AltTabController
│   ├── SpotlightController
│   ├── BarController                  [M8-A]
│   ├── BarClockService                [M8-A]
│   └── SystemRuntime                  [M8-B+]
│       ├── AudioService
│       ├── NetworkService
│       ├── BluetoothService
│       ├── MediaService
│       ├── PowerService
│       ├── BrightnessService
│       └── StatusNotifierHost
│
└── BarSurfaceManager                  [M8-A]
    └── one BarSurfaceBundle per QScreen
        ├── ReserveSurface
        ├── LauncherSurface
        ├── StatusSurface
        └── PopupOverlaySurface
```

`BarSurfaceManager` belongs to the application/surface layer, not to `ShellRuntime`, because it owns `QQuickWindow`, `QQmlComponent`, `QScreen`, and Layer Shell mapping lifecycle.

`BarController` belongs to runtime/core ownership and contains UI-facing state/actions that are independent of a particular `QQuickWindow`.

## 3. Per-output surface bundle

Every output gets exactly one bundle:

```text
BarSurfaceBundle(screen)
├── reserveWindow
├── launcherWindow
├── statusWindow
└── popupOverlayWindow
```

### Reserve surface

Configuration:

- Layer: Top;
- anchors: top + left + right;
- exclusive zone: 45;
- visual height: 45;
- transparent;
- keyboard interactivity: none;
- input-transparent.

The input-transparent requirement is mandatory. A fully transparent full-width Qt window must never become a 45 px input blocker.

Prefer `Qt::WindowTransparentForInput` on this QQuickWindow and verify it on real Wayland/LayerShellQt. If that flag is not sufficient in the target Qt/LayerShellQt combination, use the narrowest Layer Shell/Wayland input-region mechanism available, but do not leave the reserve surface interactive.

### Launcher surface

Configuration:

- Layer: Top;
- anchors: top + left;
- exclusive zone: -1;
- left margin: 8;
- top margin: 5;
- height: 36;
- keyboard interactivity: none.

Its window may reserve horizontal space for future workspace slots, but only the visible pill should draw background.

### Status surface

Configuration:

- Layer: Top;
- anchors: top + right;
- exclusive zone: -1;
- right margin: 6;
- top margin: 5;
- height: 36;
- keyboard interactivity: none.

This is intentionally simpler than the reference's dynamic left margin and produces the same final position.

The QML status width must remain capped so it never overlaps the launcher region plus the 28 px gap.

### Popup overlay surface

Configuration:

- Layer: Overlay;
- anchors: top + bottom + left + right;
- exclusive zone: -1;
- keyboard interactivity: none for M8-A;
- mapped only while a TopBar popup is active.

The overlay contains:

- an outside-click area filling the output;
- one popup card positioned by a clamped `anchorX`;
- a 54 px top offset;
- popup content selected by a typed popup kind.

This replaces the legacy shield+small-card pair without changing the visible interaction model.

## 4. Multi-screen lifecycle

`BarSurfaceManager` must subscribe to:

- existing `QGuiApplication::screens()` during initialization;
- `screenAdded(QScreen *)`;
- `screenRemoved(QScreen *)`;
- screen geometry changes where they affect QML size/placement.

Lifecycle rules:

1. Create QML windows hidden.
2. Assign the target `QScreen` before Layer Shell wrapper creation.
3. Configure Layer Shell before first map/show.
4. Only then map visible bar surfaces.
5. On output removal, close/hide and destroy all windows in that bundle exactly once.
6. No surviving object may dereference a removed `QScreen`.
7. Re-adding an output creates a new bundle; no stale popup or controller state leaks into it.

Use `QPointer<QScreen>` or equivalent guarded ownership where useful.

## 5. QML split

Do not port the Quickshell `Scope` object directly.

Recommended new package:

```text
Bar/
├── CMakeLists.txt
├── core/
│   ├── BarController.*
│   ├── BarClockService.*
│   ├── BarPopupController.*
│   └── WorkspaceModel.*
├── platform/wayland/
│   ├── BarSurfaceManager.*
│   ├── BarSurfaceBundle.*
│   └── BarSurfacePolicy.*
├── qml/
│   ├── ReserveSurface.qml
│   ├── LauncherSurface.qml
│   ├── StatusSurface.qml
│   ├── PopupOverlaySurface.qml
│   └── components/
│       ├── BarSegment.qml
│       ├── IndicatorButton.qml
│       ├── AstreaButton.qml
│       ├── AstreaMenu.qml
│       ├── WorkspaceStrip.qml
│       ├── Clock.qml
│       └── PopupCard.qml
└── tests/
```

Exact file names may vary if the existing project convention strongly suggests a better location, but preserve the ownership boundaries.

## 6. BarController responsibilities

M8-A responsibilities:

- expose whether the Bar is enabled;
- coordinate Astrea menu popup actions;
- bridge Search to the existing `SpotlightController`;
- bridge Settings launch through the existing `DesktopEntryCatalog` + `ApplicationLauncher` where the desktop entry exists;
- expose capability flags for actions not yet natively supported;
- expose the workspace model contract;
- expose clock service/model;
- provide status fields for shell diagnostics.

It must **not** execute `hyprctl`, `wpctl`, `nmcli`, `bluetoothctl`, `ddcutil`, `quickshell`, or arbitrary shell strings.

## 7. Workspace model boundary

Create the compositor-neutral data model now even if production data is empty in M8-A.

Minimum role contract:

```text
id          integer/string stable identity
active      bool
occupied    bool
urgent      bool
outputId    optional/stable output identity when available
```

Production behavior before Typhon workspace protocol completion:

- do not invent fake workspace state;
- do not bind to Hyprland from QML;
- render an empty workspace strip while preserving the reference's reserved transparent launcher surface width;
- provide test fixtures/fake data so the visual behavior is covered deterministically.

## 8. Theme strategy

The reference TopBar theme already derives from Borealis shell tokens. Eclipse Settings contains the corresponding shell token values.

M8-A should create a shell-shared QML token surface rather than hard-code a second independent palette in every new component.

The port should preserve the current visible TopBar geometry and state colors first. Avoid redesigning Dock/Spotlight in this task.

## 9. Future SystemRuntime boundary

M8-A should define clean extension points so later services can be injected without rewriting QML ownership.

Future UI-facing service objects should be typed QObjects/QAbstractItemModels, not JSON strings or child-process stdout parsers.

The intended direction is:

```text
QML -> typed controller/service API -> native backend
```

not:

```text
QML -> Process -> command output -> parser
QML -> FileView -> JSON cache -> signal -> service restart
```

## 10. Build dependency rule

Do not add Qt DBus/PipeWire/NetworkManager/BlueZ dependencies in M8-A unless they are actually used by the implemented scope. The system-service milestone can add them deliberately later.

M8-A should remain focused on shell/window architecture and visual porting.
