# Application Launch Wayland Environment Design

## Goal

Make applications launched from Eclipse's Spotlight and Dock connect to the active Wayland compositor when the systemd user manager has a stale `WAYLAND_DISPLAY` value.

## Observed Failure

Eclipse launches applications through `systemd-run --user` and GIO. The shell process has the active `WAYLAND_DISPLAY=wayland-1`, while the systemd user manager has `WAYLAND_DISPLAY=oblivion-one-tty`, whose socket is absent. Launched Qt applications then fail to create a Wayland display and never open a window. The app-launch helper itself reports that it started successfully, so its status does not indicate whether the GUI connected to the compositor.

## Behavior

- Before dispatching an application launch through the systemd user manager, copy the shell's current `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` into that manager's activation environment.
- Read these values from the current shell process environment for each launch, so a changed compositor socket is picked up without restarting Eclipse.
- Preserve the existing launch command and status reporting.
- Do not import `DISPLAY`; it can refer to a different XWayland session in multi-session setups.
- If environment synchronization fails, report the failure in the launch log and continue to return the normal launch failure result instead of silently claiming the environment was refreshed.

## Architecture

Keep synchronization at the shared application-launch boundary used by Spotlight and Dock. Add a small, testable helper that constructs and runs the `dbus-update-activation-environment --systemd` update from the current process environment. Invoke it immediately before the existing systemd/GIO launch dispatch. This keeps UI surfaces independent of session activation details and applies the same fix to every caller of the shared launcher.

## Regression Coverage

- Verify that synchronization uses the current `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR`, even when the manager previously held another socket value.
- Verify that the update does not include `DISPLAY`.
- Verify that an update failure is surfaced and prevents a launch from being reported as successful.

## Constraints

- Limit changes to Eclipse; the Typhon checkout has unrelated local modifications.
- Keep all build and test output in `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
- Preserve current Spotlight and Dock launch APIs and the existing GIO launch mechanism.
