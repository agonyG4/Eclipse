# Runtime Flow

## Startup

1. The session enables `astrea-alt-tabd.service` through `graphical-session.target`.
2. Executable runs `astrea-alt-tab --daemon`
3. CLI parsing detects `--daemon` → `CommandLineRequest::Mode::Daemon`
4. `AltTabApplication::run()`:
   - Sets app metadata, parses CLI, applies icon theme
   - Calls `runClientCommand()` → returns -1 (daemon mode, don't forward)
   - `initializeRuntime()`:
     - Creates `AltTabRuntimePaths` from environment
     - Creates `AppIdentityResolver`
   - `initializeServices()`:
     - Creates the selected compositor backend and `AltTabController`
     - Creates `AltTabConfigWatcher`
     - Creates `AltTabIpcServer` and listens on `astrea-alt-tab-v1`
     - Preserves a live daemon when an IPC socket name is already owned; only a
       stale local socket is removed and retried
     - Creates `TyphonShortcutClient` and registers `astrea-shell/alt_tab_next`,
       `astrea-shell/alt_tab_previous`, and `astrea-shell/alt_tab_commit`
     - Keeps IPC and the daemon alive when the Typhon shortcuts manager is unavailable
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

### Forward (native Alt+Tab)

1. Typhon matches physical `ALT+Tab` and dispatches `pressed` for the
   `astrea-shell/alt_tab_next` registration.
2. `TyphonShortcutClient` maps the event to `m_controller->step(1)`.
3. Repeated compositor events map to additional `step(1)` calls; Shift+Tab
   uses `alt_tab_previous` and `step(-1)`.
4. Controller state machine:
   - Hidden → Opening: store direction and request the active opening generation
   - If the backend is still starting, `Ready` requests the opening snapshot;
     the matching `snapshotReady` opens the UI
   - Window source sends `hyprctl clients -j` via Hyprland command socket, or
     waits for Typhon's first committed snapshot
   - Parse/map → filter hidden and explicitly invalid workspace metadata → sort by focusHistoryID
   - Model setWindows → find active window by focusHistoryID→0
   - Apply offset → Open state → emit focusRequested → QML surface visible
5. The protocol's synthetic `alt_tab_commit` `pressed` event is dispatched when
   the compositor commits the Alt sequence, and maps to `m_controller->commit()`.

### IPC compatibility path

The CLI remains available for scripted and compatibility callers:

1. `astrea-alt-tab --next` sends IPC `next\n` to the daemon socket.
2. `AltTabIpcServer` emits `commandReceived` and the application invokes the
   same controller operation.
3. `status` reports backend, overlay, and Typhon shortcut registration state.

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
