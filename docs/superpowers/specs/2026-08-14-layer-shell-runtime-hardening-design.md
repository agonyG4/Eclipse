# Layer Shell runtime hardening design

## Scope

Close the remaining Eclipse LayerShellQt integration gaps without changing Typhon
or the existing Dock, Alt+Tab, and Spotlight visual behavior.

## Decisions

1. Require LayerShellQt 6.4.5 or newer at configure time. This is the minimum
   version used by the pinned CI build and exposes every API Eclipse needs.
2. Remove the global `LayerShellQt::Shell::useLayerShell()` bootstrap call. The
   supported per-window `LayerShellQt::Window::get()` path attaches the
   LayerShellQt integration, and the older global switch is unnecessary on the
   Qt versions Eclipse supports.
3. Probe `zwlr_layer_shell_v1` through Qt's existing Wayland display before QML
   surfaces are loaded. This proves the compositor advertises the protocol and
   avoids creating a second Wayland connection.
4. Keep the current surface policies intact: Dock remains a top-layer bottom
   exclusive surface, while Alt+Tab and Spotlight remain overlay keyboard
   surfaces. A configured screen is selected on the `QWindow` before
   LayerShellQt creates its wrapper, which is compatible with LayerShellQt 6.4.5.
5. Status JSON reports compile-time support, the Qt Wayland backend, protocol
   advertisement, and per-surface configuration requests. It does not claim a
   compositor configure acknowledgement that the public LayerShellQt API does
   not expose.

## Verification

- CMake contract tests cover enabled/available, enabled/missing, enabled/too old,
  and explicitly disabled LayerShellQt configurations.
- A Qt test covers the runtime capability decision without requiring a compositor.
- Clean Debug and Release builds use the exact LayerShellQt 6.4.5 CI target.
- If a live Hyprland Wayland session is available, the shell is launched with a
  bounded lifetime and its IPC status is checked; no desktop restart is used.
