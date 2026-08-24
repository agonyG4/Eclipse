# Port Map

This document tells the implementation agent what to preserve, redesign, postpone, or discard.

| AstreaOS reference | Target Eclipse concept | M8-A action |
|---|---|---|
| `Quickshell/bar/Bar.qml` | `BarSurfaceManager` + per-surface QML | Re-architect; preserve geometry/behavior |
| `reserveSurface` | `ReserveSurface.qml` | Port behavior exactly; make input-transparent |
| `launcherSurface` | `LauncherSurface.qml` | Port visual shell |
| `statusSurface` | `StatusSurface.qml` | Port visual shell; anchor top+right |
| `BarSegment.qml` | shared Bar visual component | Port/refine |
| `IndicatorButton.qml` | shared Bar visual component | Port/refine |
| `TopbarIndicator.qml` | shared Bar visual component | Fold into native popup/controller model if simpler |
| `AstreaPopup.qml` | `AstreaMenu.qml` + `BarController` actions | Port UI; replace process actions |
| `Workspaces.qml` | `WorkspaceModel` + `WorkspaceStrip.qml` | Port visual contract; no production provider yet |
| `Tray.qml` | future `StatusNotifierHost` | Do not implement in M8-A |
| `TrayContextMenu.qml` | future DBus menu model | Do not implement in M8-A |
| `NetworkIndicator.qml` | future `NetworkService` | UI placeholder/hidden until M8-B |
| `NetworkPopup.qml` | future native popup content | Postpone |
| `BluetoothIndicator.qml` | future `BluetoothService` | UI placeholder/hidden until M8-B |
| `BluetoothPopup.qml` | future native popup content | Postpone |
| `VolumeIndicator.qml` | future `AudioService` | UI placeholder/hidden until M8-B |
| `VolumePopup.qml` | future native popup content | Postpone |
| `VolumeOsd.qml` | future shared OSD service | Postpone |
| `Clock.qml` | `BarClockService` + `Clock.qml` | Implement in M8-A |
| clock notification popup | future NotificationStore | Keep clock interaction capability-gated/disabled until notification milestone |
| `TopbarPopup.qml` | `PopupOverlaySurface.qml` | Re-architect into one fullscreen overlay surface |
| `PopupHost.qml` | `BarPopupController` | Replace |
| `ControlCenterButton.qml` | future Control Center entry point | Keep hidden/disabled until M8-C |
| `ControlCenterPopup.qml` | future Control Center | Do not copy direct process logic |
| `ControlCenterRegistry.js` | future typed module registry/model | Preserve concept, not implementation |
| `ShellRuntime.qml` state loaders | `ShellRuntime` native ownership | Replace with native controller/service ownership |
| `StatusFile.qml` | none | Delete from target architecture |
| `AudioProcess.qml` | future `AudioService` | Do not port |
| `NetworkProcess.qml` | future `NetworkService` | Do not port |
| `BluetoothProcess.qml` | future `BluetoothService` | Do not port |
| `astrea_statusd.py` | reference only | Do not make Eclipse depend on it |
| `bluetooth_manager.py` | reference only | Do not make Eclipse depend on it |
| `Quickshell.Hyprland` | future Typhon workspace provider | Never import into Eclipse Bar QML |
| `Quickshell.Services.SystemTray` | future StatusNotifier implementation | Replace later |

## Existing Eclipse pieces to reuse

### `AstreaLayerShellHelper`

Use it for all Bar Layer Shell configuration. Extend it only with narrowly scoped runtime setters if a real need remains after the fullscreen popup-overlay design. Do not add a parallel Layer Shell implementation.

### `ShellRuntime`

The new Bar controller belongs here so it shares lifetime and dependencies with Dock/Alt+Tab/Spotlight.

### `SpotlightController`

The Astrea menu Search action should call existing Spotlight behavior, not launch another search UI.

### `DesktopEntryCatalog` + `ApplicationLauncher`

Use them for application-style actions such as Settings when an appropriate desktop entry is available. Do not spawn a shell command directly from QML.

### `AstreaI18n`

Reuse the current native shell i18n context. The supplied English AstreaOS catalog contains all current TopBar keys and can be used to identify missing translation coverage.

### `AstreaIconProvider` / icon theme

Prefer the native Eclipse icon pipeline. Package the Astrea logo as a QML resource instead of building file URLs from `ASTREA_ROOT`.

## New source boundaries

A practical implementation split is:

```text
Bar/core/BarController
    runtime-facing UI state/actions

Bar/core/BarPopupController
    active popup kind + per-output anchor state

Bar/core/BarClockService
    deterministic time/date formatting + one-minute tick boundary

Bar/core/WorkspaceModel
    compositor-neutral future-facing workspace roles

Bar/platform/wayland/BarSurfacePolicy
    pure Layer Shell constants/config generation

Bar/platform/wayland/BarSurfaceBundle
    four QQuickWindow pointers for one QScreen

Bar/platform/wayland/BarSurfaceManager
    QScreen lifecycle + QQmlComponent creation/destruction + mapping
```

Keep geometry/policy math testable without a live compositor.
