# Runtime Flow

## Daemon Startup

`astrea-spotlight --daemon` starts the QGuiApplication, applies the icon theme, creates the backend, config watcher, game mode monitor, IPC server, and QML engine, then stays resident.

## Toggle/Show

IPC commands request `show`, `hide`, or `toggle`. The controller opens or closes the surface and updates `surfaceVisible` and `open`.

## Search

QML text input updates `SpotlightController::query`, which schedules search, calls the Rust backend, and refreshes the results model.

## Application Launch

Selecting a result records usage through the Rust backend and launches via `astrea-launch`.

## Usage Ranking

The Rust backend loads usage counts and applies them as a tie-break in search ordering.

## Weather Refresh

The controller starts weather refreshes when visible and weather is enabled, and stops them on close or game mode.

## Config Reload

`SpotlightConfigWatcher` watches JSON config files and applies component/weather enablement updates.

## Game Mode

`GameModeMonitor` polls `gamemoded -s` and suppresses weather refresh while active.

## Icon Resolution

`AstreaIconTheme` configures the active Qt theme and merged XDG/Flatpak search
paths. `AstreaIconProvider` resolves individual icons through `QIcon`, keeps
bounded positive and negative caches, and includes logical extent, effective
DPR, physical source extent, and theme revision in its request/cache contract.
Spotlight requests a 40 logical-pixel maximum and lets the shared QML policy
convert it to physical pixels for the current display.

## Theme Change

Theme updates clear the icon provider cache and bump the revision so QML image URLs change.

## Index Reload

Directory changes debounce into a backend reload and result reset.

## Shutdown

Closing the window or daemon exit tears down watchers, processes, and the IPC server through QObject ownership.
