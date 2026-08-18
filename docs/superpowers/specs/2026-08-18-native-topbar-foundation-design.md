# Native TopBar Foundation Design

## Goal

M8-A adds the AstreaOS TopBar visual shell to the unified `astrea-shell` process with native Qt/C++ ownership, one Layer Shell bundle per `QScreen`, a native clock, native menu actions, and an output-local popup overlay. It deliberately leaves system services, tray, notifications, and real Typhon workspaces to follow-up milestones.

## Ownership and data flow

`ShellRuntime` owns the UI-facing `BarController`, `BarClockService`, and compositor-neutral `WorkspaceModel` alongside the existing catalog, launcher, Dock, Alt+Tab, Spotlight, and Typhon services. `BarController` exposes capability-gated actions: Search calls the existing `SpotlightController::show()` path, and Settings resolves `astrea-settings.desktop` through `DesktopEntryCatalog` and calls `ApplicationLauncher::launchDesktop()`.

`AstreaShellApplication` owns `BarSurfaceManager`. The manager owns one `BarSurfaceBundle` per actual `QScreen`; each bundle owns Reserve, Launcher, Status, and Popup Overlay `QQuickWindow`s plus one output-local `BarPopupController`. A bundle assigns its screen before calling `AstreaLayerShellHelper::configure()`, configures every surface while hidden, and maps only after all policy calls succeed. Screen add/remove signals create and destroy bundles deterministically.

## Surface policy

The reserve surface is a transparent, input-transparent Top-layer surface anchored to top/left/right with height and exclusive zone 45. Launcher and Status are transparent Top-layer surfaces with 36-pixel content pills, top margin 5, left margin 8 and right margin 6 respectively, and exclusive zone -1. Status width is capped using pure geometry policy so it preserves a 28-pixel gap from the launcher region. The Popup Overlay is an all-edge Overlay surface with exclusive zone -1; it is mapped only while one popup is open, closes on outside click, and keeps card clicks inside the card.

## QML and theme

New production Bar QML is a visual-only module. It imports Qt Quick only, consumes typed/context-provided objects, and contains no Quickshell, process, file-cache, compositor-command, or legacy service integration. A reusable `ShellBarTheme.qml` mirrors the existing Borealis shell tokens and animation values. The Astrea logo is compiled as a Qt resource. Workspace QML renders `WorkspaceModel` roles (`id`, `active`, `occupied`, `urgent`, optional `outputId`) and uses deterministic test data only in tests, never in production.

## Compatibility and diagnostics

Existing shell IPC remains schema version 1. Status JSON gains stable `bar` and Layer Shell Bar fields without pointer addresses or unstable IDs. Bar shutdown is owned by the application/runtime lifecycle and cannot stop Dock, Alt+Tab, Spotlight, or Typhon services. No ordinary Qt fallback is permitted when Layer Shell is unavailable.

## Verification

Pure Qt tests cover policy values, geometry clamping, workspace role/order behavior, clock formatting and minute scheduling, popup transitions, and screen bundle lifecycle. QML smoke/static tests load the new components and reject prohibited legacy patterns. Existing full CTest, QML lint/build, and available CI gates remain authoritative. Live Wayland/hotplug checks are reported separately and are not inferred from offscreen tests.
