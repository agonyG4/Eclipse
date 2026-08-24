# Source Baseline

## Inputs studied

This migration plan is based on the provided source snapshots, not on a generic shell design.

### Eclipse target snapshot

`target/Eclipse/`

Relevant current architecture:

- `Shell/app/AstreaShellApplication.cpp`
- `Shell/runtime/ShellRuntime.cpp`
- `shared/platform/wayland/LayerShellHelper.*`
- `shared/platform/typhon/*`
- `Dock/`
- `AltTab/`
- `Spotlight/`
- `Settings/`

The current shell already owns a unified runtime containing the desktop-entry catalog, application launcher, app identity resolver, Typhon shared connection, Typhon toplevel connection, shortcut client, Dock controller, Alt+Tab controller, Spotlight controller, GameMode monitor, and shell IPC.

The current `AstreaShellApplication` creates one `QQmlApplicationEngine`, loads Dock, Alt+Tab, and Spotlight QML surfaces, then configures their Layer Shell state from C++.

`AstreaLayerShellHelper` already supports:

- Layer selection;
- keyboard interactivity;
- top/bottom/left/right anchors;
- exclusive zone;
- margins;
- explicit `QScreen *` assignment;
- Wayland/`zwlr_layer_shell_v1` runtime validation.

This means the native TopBar does not require another Layer Shell integration library.

### AstreaOS TopBar reference

`reference/astreaos/src/Quickshell/bar/`

The reference source contains approximately 5.8k lines of TopBar QML and includes:

- three per-screen TopBar surfaces;
- Astrea menu;
- workspace strip;
- system tray;
- network indicator/popup;
- Bluetooth indicator/popup;
- volume indicator/popup/OSD;
- clock and notification history entry point;
- Control Center and its module registry;
- generic TopBar popup infrastructure;
- theme constants and animations.

Additional reference files are included for the legacy runtime and bridge behavior:

- `Quickshell/shell.qml`
- `Quickshell/runtime/*`
- `Quickshell/notifications/*`
- selected music-monitor code;
- `System/services/astrea_statusd.py`
- `System/scripts/bluetooth_manager.py`
- `Core/bridge/system/region.py`
- `Core/bridge/state_json.py`
- English i18n catalog;
- TopBar assets.

### Typhon context

`reference/typhon/TYPHON_VS_KWIN_VS_HYPRLAND_AUDIT_2026-08-10.md`

The audit is included only to document why the immediate TopBar port must not invent a temporary Typhon workspace protocol. The audited Typhon snapshot did not yet expose a general production workspace subsystem comparable to Hyprland/KWin. The Eclipse TopBar should therefore define a stable workspace model boundary now and bind it to Typhon only when the compositor protocol is finalized.

## Source authority rules

When reference behavior and legacy implementation conflict, preserve the behavior and replace the implementation.

Examples:

- Preserve the three-surface TopBar visual layout, but do not preserve Quickshell `PanelWindow` code.
- Preserve workspace indicator geometry, but do not preserve `Quickshell.Hyprland`.
- Preserve tray interaction semantics, but do not preserve `Quickshell.Services.SystemTray` as a dependency.
- Preserve network/Bluetooth/audio UI contracts, but do not preserve command execution or status JSON polling in QML.

The target Eclipse snapshot is authoritative for build conventions, runtime ownership, Typhon integration, application launching, i18n, and Layer Shell usage.
