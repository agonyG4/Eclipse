# Runtime Flow

## Resident Shell startup

`astrea-shell` is the resident process. `AstreaShellApplication` creates the
unified UI and `ShellRuntime` composes the catalog, launcher,
`SpotlightController`, `SpotlightConfigWatcher`, `GameModeMonitor`, and other
resident services. Shell resolves `SpotlightRuntimePaths`, converts the paths
needed by the controller to `SpotlightControllerPaths`, owns one
`RustSpotlightBackend`, and injects it into `SpotlightController`. The backend
owner outlives the controller. Shell owns one shared `ApplicationLauncher` and
one shared `DesktopEntryCatalog`, plus a borrowed-service Spotlight adapter
for each. The adapters and backend outlive the controller. Shell exposes
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
search through the core `SpotlightBackend` port, implemented in production by
`RustSpotlightBackend`, and updates `SpotlightResultsModel`. Result selection
and activation continue through the same controller methods.

## Application launch and usage

Selecting a result records usage through the Rust backend, closes Spotlight,
then submits the six-field launch request through this path:

```text
SpotlightController
    -> SpotlightLauncher
    -> SpotlightApplicationLauncherAdapter
    -> ApplicationLauncher
    -> astrea-launch
```

Ordinary launch failures are reported while Spotlight remains closed. A launch
timeout reports the existing timeout message and reopens Spotlight. The Rust
backend loads usage counts and applies them as a tie-break in search ordering.

## Catalog updates and reloads

Shell's one shared catalog feeds both other Shell consumers and Spotlight:

```text
DesktopEntryCatalog::indexUpdated
    -> SpotlightDesktopCatalogAdapter
    -> SpotlightCatalogSource::updated
    -> SpotlightController
    -> SpotlightBackend::setCatalog
```

After a successful update, an active non-empty query runs again with the same
search limit. If setting the catalog fails, the controller logs the error and
does not rerun the search. Catalog mode uses the adapter snapshot for startup,
locale recreation, reload, and watched directories. With no catalog source,
the controller preserves native Rust index creation, reload, and watched
directory behavior.

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
controller before its borrowed Rust backend, following `ShellRuntime` member
declaration order. No standalone Spotlight resident process is started.

## Remaining Phase 3 work

Phase 3B remains for weather orchestration through `QProcess`. The controller
core no longer owns or depends on the shared application-launcher or
desktop-catalog implementations. Spotlight is not yet declared fully cleaned
up or canonical pending Phase 3B and final review.
