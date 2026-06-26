# Runtime Flow

## Startup

1. `systemctl --user start astrea-alt-tabd.service`
2. Executable runs `astrea-alt-tab --daemon`
3. CLI parsing detects `--daemon` → `CommandLineRequest::Mode::Daemon`
4. `AltTabApplication::run()`:
   - Sets app metadata, parses CLI, applies icon theme
   - Calls `runClientCommand()` → returns -1 (daemon mode, don't forward)
   - `initializeRuntime()`:
     - Creates `AltTabRuntimePaths` from environment
     - Creates `AppIdentityResolver`
   - `initializeServices()`:
     - Creates `HyprlandWindowSource` (connects to compositor sockets)
     - Creates `AltTabController` with source and resolver
     - Creates `AltTabConfigWatcher`
     - Creates `AltTabIpcServer` and listens on `astrea-alt-tab-v1`
     - Handles stale socket cleanup
     - Sets status reply callback
   - `connectSignals()`:
     - Config watcher → component toggle
     - IPC server → controller commands
     - Controller commit → window source focus
   - `initializeQml()`:
     - Creates QML engine
     - Registers `AstreaIconProvider` as context property
     - Registers controller and window model as context properties
     - Loads `Astrea.AltTab` Main.qml
     - Configures LayerShell on the root window

## Key press flow

### Forward (Alt+Tab)

1. Hyprland `bind = ALT, Tab, exec, astrea-alt-tab --next`
2. Thin CLI sends IPC "next\n" to daemon socket
3. `AltTabIpcServer` receives command → emits `commandReceived`
4. `AltTabApplication` → `m_controller->step(1)`
5. Controller state machine:
   - Hidden → Opening: store direction, request window snapshot
   - Window source sends `hyprctl clients -j` via Hyprland command socket
   - Parse JSON → filter hidden/invalid → sort by focusHistoryID
   - Model setWindows → find active window by focusHistoryID→0
   - Apply offset → Open state → emit focusRequested → QML surface visible
6. Repeated Tab (still in Open): cycle selection immediately

### Alt release (Commit)

1. QML `Keys.onReleased` detects `Qt.Key_Alt` or `Qt.Key_AltGr`
2. Calls `AltTabController.commit()`
3. State: Open → Committing → emit commitRequested(address, workspace)
4. `AltTabApplication` → `m_windowSource->focusWindow(address, workspaceId)`
5. HyprlandWindowSource:
   - `dispatch focusworkspaceoncurrentmonitor <id>`
   - `dispatch focuswindow address:<normalized address>`
6. State → Hidden, model cleared, surface hidden
7. Compositor `bindr` also fires as a safe fallback (idempotent)

## IPC command handling

| IPC Command | Method |
|-------------|--------|
| next | `step(1)` |
| previous | `step(-1)` |
| commit | `commit()` |
| cancel | `cancel()` |
| show | `show()` |
| hide | `hide()` |
| reload-windows | `reloadWindows()` |
| status | callback → JSON |
