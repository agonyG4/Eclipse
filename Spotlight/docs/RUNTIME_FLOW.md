# Runtime Flow

## Resident Shell startup

`astrea-shell` is the resident process. `AstreaShellApplication` creates the
unified UI and `ShellRuntime` composes the catalog, launcher,
`SpotlightController`, `SpotlightConfigWatcher`, `GameModeMonitor`, and other
resident services. Shell creates the Spotlight objects as before and exposes
`SpotlightController` and `AstreaI18n` to the existing `Astrea.Shell` QML
module.

## Compatibility CLI and IPC

`astrea-spotlight` remains a non-resident compatibility client. `ShellIpcClient`
sends resident commands to the `ShellIpcServer` in `astrea-shell` using the
Shell IPC protocol. If Shell is not running, a command cannot be delivered.
`--daemon` is intentionally rejected and directs users to `astrea-shell`.
`SpotlightIpcServer` is legacy/test-only and is not part of this runtime path.
Icon resolution and icon-theme diagnostics continue to run locally in the CLI.

## Show, hide, and toggle

Shell IPC commands reach the existing resident controller. The controller
updates open and surface visibility state, preserving the current QML focus
and close-animation flow. `ShellShortcutDispatcher` continues to route the
`spotlight_toggle` shortcut to `SpotlightController::toggle`.

## Search and selection

QML text input updates `SpotlightController::query`. The controller schedules
search through the existing Rust backend wrapper and updates
`SpotlightResultsModel`. Result selection and activation continue through the
same controller methods.

## Application launch and usage

Selecting a result records usage through the Rust backend and launches it via
`ApplicationLauncher` and `astrea-launch`. The catalog and launch wiring remain
owned by Shell composition. The Rust backend loads usage counts and applies
them as a tie-break in search ordering.

## Weather, configuration, and GameMode

The controller starts weather refreshes when visible and weather is enabled,
and stops them on close or while GameMode is active. Weather process execution
remains in `SpotlightController`. `SpotlightConfigWatcher` continues to watch
the Spotlight and component configuration files and apply updates. The
`GameModeMonitor` continues polling `gamemoded -s` and suppressing weather
refresh while active.

## Icons, locale, and catalog updates

`AstreaIconTheme` and `AstreaIconProvider` continue to configure and resolve
icons through shared icon infrastructure. Theme changes clear the provider
cache and bump its revision so QML image URLs change. `AstreaI18n` continues to
load locale catalogs and watch for changes. Directory changes continue to
debounce into a backend catalog reload and result reset.

## Shutdown

Closing or stopping `astrea-shell` tears down the Shell-owned Spotlight
controller and services through the existing QObject and `ShellRuntime`
ownership. No standalone Spotlight resident process is started.
