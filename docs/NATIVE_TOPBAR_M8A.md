# Native TopBar foundation (M8-A)

`astrea-shell` owns the native TopBar. `BarSurfaceManager` tracks the actual
`QScreen` set and owns one `BarSurfaceBundle` per output. Each bundle creates
four independent Qt Quick windows: a transparent 45 px exclusive reservation,
the left launcher pill, the right status pill, and a fullscreen overlay popup.
Layer Shell configuration is completed before mapping, and output removal or
shutdown tears down the bundle immediately.

## Production closure details

- The manager uses a production-used injectable bundle factory for deterministic
  lifecycle tests. Its real default factory creates `BarSurfaceBundle` objects;
  tests do not exercise an orphan registry or fake `QScreen` pointers.
- `BarLayoutMetrics` is the single geometry authority for C++, production QML,
  and the surface bundle. It owns the 45/36 px heights, 8/6 px margins, 28 px
  launcher/status gap, 8 px popup padding, status sizing/anchors, and popup
  clamping. Status padding is included exactly once.
- The status surface is a non-semantic container. `Clock` owns its own click
  target and calculates its indicator center in output coordinates, leaving
  future tray/network/audio indicators free to own independent popups.
- `BarPopupController` separates intent, rendered kind, closing state, and
  surface-required state. Close keeps the overlay mapped through the QML exit
  animation; the animation calls `completeClose()`. Output removal and
  shutdown use immediate teardown and never wait for QML animation.
- `BarController.enabled` is connected to every live bundle. Disabling hides
  Reserve, Launcher, Status, and Popup surfaces and clears local popup state;
  re-enabling remaps the existing topology without duplicating bundles.
- Spotlight component enablement is authoritative. Search availability follows
  the observable Spotlight state, and disabled Spotlight cannot be reopened by
  the Bar or shell IPC paths.
- Settings and Shell use one shared `shared/theme/ThemeController` state
  authority. The Shell exposes its own instance to QML, watches the shared JSON
  configuration, debounces reloads, handles file creation/replacement, and
  retains the last good state for invalid JSON. `ShellBarTheme` derives its
  dark/light, shell-style, surface, border, interaction, text, icon, separator,
  and accent tokens from that state.
- Bar QML and its Astrea logo are packaged by one shared CMake resource
  definition. Smoke tests load the exact `qrc:/qt/qml/Astrea/Shell/Bar/...`
  URLs used by `BarSurfaceBundle`.

## Deliberate M8-A boundaries

The production Bar contains no Quickshell, process execution, file-cache
bridges, compositor CLI, device utility integration, Python bridge, or JSON
status daemon. About, Force Quit, Lockscreen, Power, and notification history
remain unavailable until Eclipse-native paths exist. Workspace data remains an
empty compositor-neutral `WorkspaceModel` with stable `id`, `active`,
`occupied`, `urgent`, and optional `outputId` roles; no Typhon or Hyprland
workspace protocol is invented here.

M8-B+ work still includes system tray, network, Bluetooth, audio, media,
Control Center, notifications, notification history, volume OSD, and real
workspace/output updates.

The deterministic tests run with the Qt offscreen platform. Layer Shell
protocol negotiation, real Wayland hotplug, exclusive-zone release, and visual
behavior on a live compositor still require release qualification in a Wayland
session.

Shell IPC remains schema version 1 and exposes the stable `bar` state plus
`layerShell.barConfigurationRequested`; this does not imply that any M8-B
service is implemented.

## M8-A.2 visual and lifecycle closure

- `ShellBarTheme` now mirrors the six Borealis combinations from the Settings
  Shell authority exactly: transparent, default, and frosted styles in both
  dark and light modes. Popup cards use the elevated `Apps.popupBg` token;
  Bar segments use the shell background/border at rest and the exact hover,
  pressed, and active interaction tokens.
- Popup opening has a separate opacity/scale enter transition (`0` to `1`,
  `0.97` to `1`) aligned with the Borealis popover duration. Reopen and
  Clock-to-Astrea switches stop stale exits before starting the new enter;
  exit completion remains guarded by the current popup kind and closing state.
- `BarSurfaceManager::shutdown()` is terminal. It disconnects application and
  BarController callbacks before tearing down bundles, rejects reinitialize,
  and ignores later screen, geometry, enablement, and repeated-shutdown events.
- Clock remains a native horizontal date/separator/time indicator and now
  uses the shared Inter/weight/timing tokens with offscreen assertions for its
  structure and typography mapping.
