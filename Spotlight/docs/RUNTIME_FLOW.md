# Runtime Flow

## Resident Shell startup

`astrea-shell` is the resident process. `AstreaShellApplication` creates the
unified UI and `ShellRuntime` composes the catalog, launcher,
`SpotlightController`, `SpotlightConfigWatcher`, `GameModeMonitor`, and other
resident services. Shell resolves `SpotlightRuntimePaths`, converts the paths
needed by the controller to `SpotlightControllerPaths`, owns one
`RustSpotlightBackend`, one `SpotlightWeatherProcessService`, and injects them
into `SpotlightController` through core ports. The backend and weather service
outlive the controller. Shell owns one shared `ApplicationLauncher` and one
shared `DesktopEntryCatalog`, plus a borrowed-service Spotlight adapter for
each. The adapters also outlive the controller. Shell exposes
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

The weather dependency direction is:

```text
SpotlightController
    -> SpotlightWeatherProvider

SpotlightWeatherProcessService
    -> SpotlightWeatherProvider
    -> QProcess
    -> weather-cli summary
```

`SpotlightController` owns the policy and presentation state: enabled/open and
GameMode checks, the 30-minute stale threshold, the 30-minute periodic refresh
timer, the 500 ms startup warmup (including while hidden), loading/ready state,
status text, last successful fetch time, and condition-to-icon mapping. It
cancels requests when Spotlight closes, GameMode activates, or weather is
disabled. Close and GameMode activation retain cached successful weather.
When GameMode becomes inactive while Spotlight is open, a missing or stale
cache can refresh again.

`SpotlightWeatherProcessService` owns each fresh `QProcess`, invokes the
configured executable directly with the single `summary` argument, parses the
JSON object, maps process outcomes to the core failure enum, and suppresses
callbacks from canceled or superseded processes. The core port exposes only
weather values and failure categories. Cancellation guarantees that a canceled
request cannot emit a later result. There is no weather process timeout.

`SpotlightRuntimePaths::weatherCli()` supplies the executable path to Shell's
service composition. `controllerPaths()` supplies only `astreaRoot` and
`weatherAssetDirectory` to core. `SpotlightConfigWatcher` continues to watch
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
controller before its borrowed Rust backend and weather provider, following
`ShellRuntime` member declaration order. No standalone Spotlight resident
process is started.

The planned internal Spotlight layering phases are implemented. Canonical
component promotion remains a separate decision after the final review.
