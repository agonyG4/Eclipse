# Current AstreaOS TopBar Audit

## 1. Per-screen composition

The current `Quickshell/shell.qml` creates one `Bar` for every entry in `Quickshell.screens`.

Each `Bar.qml` creates three Layer Shell surfaces:

```text
Output
│
├── ReserveSurface
│   ├── top + left + right anchored
│   ├── height: 45 px
│   ├── transparent
│   └── exclusive zone: 45 px
│
├── LauncherSurface
│   ├── top + left anchored
│   ├── height: 36 px
│   ├── left margin: 8 px
│   ├── top margin: round((45 - 36) / 2) = 5 px
│   └── no exclusive zone
│
└── StatusSurface
    ├── top anchored
    ├── height: 36 px
    ├── right visual margin: 6 px
    ├── top margin: 5 px
    └── no exclusive zone
```

The reference calculates the status surface as a left-anchored surface with a dynamic `statusLeft`, but the equation always resolves to the right-aligned position because `statusWidth` is already capped against the launcher gap. The native Eclipse port can therefore anchor the status surface to **top + right** with a fixed 6 px right margin and preserve the same visible geometry with less state.

The minimum intended horizontal separation between launcher and status regions is 28 px.

## 2. Visual constants

Important reference values:

- reserved top work area: 45 px;
- pill height: 36 px;
- top pill margin: 5 px;
- left pill margin: 8 px;
- right pill margin: 6 px;
- segment horizontal padding: 10 px;
- TopBar popup top offset: 54 px;
- workspace reserved slots: 10;
- active workspace width: theme-controlled, currently 32 px through Borealis shell tokens;
- inactive workspace dot size: theme-controlled, currently 10 px through Borealis shell tokens;
- popup scale-in baseline: 0.97;
- popup fade duration: 180 ms;
- popup scale duration: 220 ms.

The reference `BarSegment` fades in and changes border color on hover. Indicator buttons use active, pressed, hover, and idle states with short color transitions.

## 3. Launcher/left region

The left pill contains:

- Astrea logo button;
- workspace indicator strip.

The surface reserves width for ten workspace slots so that workspace population changes do not move the right-side hit geometry unexpectedly, while the visible pill itself still follows actual content width.

The Astrea menu currently exposes:

- Search;
- About this PC;
- Settings;
- Force Quit;
- Lockscreen;
- Power.

The current implementation launches these using external commands such as `rofi`, `quickshell`, `hyprctl`, and `shutdown`. Those commands are implementation legacy, not a native Eclipse contract.

## 4. Status/right region

The right pill contains, in order:

1. system tray;
2. optional tiny spacer when tray is non-empty;
3. network indicator;
4. Bluetooth indicator;
5. volume indicator;
6. Control Center button;
7. clock.

The status width is content-sized but is capped so it cannot overlap the launcher region plus the 28 px gap.

## 5. Workspace implementation

`Workspaces.qml` is directly bound to `Quickshell.Hyprland`:

- source: `Hyprland.workspaces.values`;
- special/negative workspace IDs are filtered out;
- visible workspaces are sorted numerically;
- active workspace expands from a dot to a pill;
- activation dispatches a Hyprland-specific command.

This is one of the clearest pieces that must be **re-modeled rather than ported**.

The Eclipse-facing contract should be compositor-neutral and expose at least:

- workspace ID;
- active state;
- occupied state;
- urgent state;
- output identity when Typhon supports real per-output workspaces.

## 6. System tray implementation

`Tray.qml` uses `Quickshell.Services.SystemTray` and supports:

- icon rendering;
- tooltip title;
- primary activation;
- secondary activation;
- right-click menus;
- separators;
- checked/partially checked items;
- nested menus through `QsMenuOpener`.

This behavior is worth preserving, but Eclipse will need its own StatusNotifierItem/DBus host in a later milestone.

## 7. Audio implementation

The current `AudioProcess.qml` and status daemon use `wpctl` and a file-backed status cache.

The TopBar can set volume directly from QML by spawning `wpctl`.

This must not be reproduced inside native Eclipse QML. The target architecture requires a typed service API with asynchronous state/actions and no shell-command parsing in the UI layer.

## 8. Network implementation

The current status daemon derives network state from:

- `/proc/net/route`;
- `/sys/class/net/*` counters;
- `nmcli` for Wi-Fi connection name;
- `ip route` as fallback.

`NetworkProcess.qml` watches `network.json` through `StatusFile.qml`.

The readout behavior can be preserved; the JSON file and process bridge should not become the Eclipse architecture.

## 9. Bluetooth implementation

Bluetooth currently combines:

- `bluetooth.json` status cache;
- `bluetooth_manager.py`;
- direct `bluetoothctl` calls;
- streamed scan events from the Python helper;
- scan ownership bookkeeping in QML.

The scan-owner idea is useful and should survive conceptually, but the native implementation should expose it through a service object rather than QML-owned subprocesses.

## 10. Popup architecture

The current generic TopBar popup uses two Layer Shell surfaces:

- a full-output transparent click shield on the Top layer;
- a small card surface on the Overlay layer.

The card is positioned using a clamped horizontal anchor and a 54 px top offset.

For Eclipse, a single full-output Overlay surface per output is a cleaner equivalent:

- mapped only while a popup is open;
- outside area closes the popup;
- card consumes its own clicks;
- the card is positioned inside the full-output surface;
- no runtime Layer Shell margin mutation is needed for popup anchoring.

This preserves visible behavior while simplifying ownership and output geometry.

## 11. Clock and notifications

The clock is not only a formatted time label. It also participates in the notification-history popup path and uses a region helper for locale/region behavior.

For M8-A, the time/date clock can be native and self-contained. Notification history remains a follow-up because Eclipse does not yet own the current notification daemon/store.

## 12. Control Center

The current Control Center is already modular and configurable, but its QML still owns several direct process integrations and state-file operations. It should be treated as a visual/reference contract and ported only after shared native system services exist.

## 13. Legacy dependency summary

The reference TopBar directly or indirectly depends on:

- Quickshell;
- Quickshell.Wayland;
- Quickshell.Io;
- Quickshell.Hyprland;
- Quickshell.Services.SystemTray;
- `wpctl`;
- `nmcli`;
- `bluetoothctl`;
- `ddcutil`;
- `hyprctl`;
- `quickshell` child processes;
- Python helpers;
- JSON status files;
- filesystem watchers;
- systemd user-service toggling.

The native Eclipse port must not inherit these as QML responsibilities.
