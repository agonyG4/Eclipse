# Architecture

## Process and CMake ownership

`astrea-shell` is the resident process. Shell composes the Spotlight controller,
configuration watcher, game-mode monitor, locale service, application catalog,
and launcher with the other resident components.

`astrea-spotlight` is a lightweight compatibility IPC client. Its resident
commands go through `ShellIpcClient` to the `ShellIpcServer` hosted by
`astrea-shell`. `--daemon` is intentionally rejected with directions to use
`astrea-shell`; it does not start a Spotlight process. The old
`SpotlightIpcServer` is retained only for existing tests and is not the
resident IPC server.

The production CMake ownership is:

- `astrea-spotlight-runtime` owns `SpotlightController`,
  `SpotlightResultsModel`, the core `SpotlightBackend`, `SpotlightLauncher`,
  `SpotlightCatalogSource`, and `SpotlightControllerPaths` contracts,
  `AstreaI18n`, `GameModeMonitor`, `SpotlightConfigWatcher`,
  `SpotlightApplicationLauncherAdapter`, `SpotlightDesktopCatalogAdapter`,
  `SpotlightRuntimePaths`, and `RustSpotlightBackend`. It links the shared core
  and Qt Core publicly, while keeping the native Rust backend and its system
  link requirements private.
- `astrea-spotlight-wayland` owns `LayerShellSurface` and its Qt and shared
  Layer Shell dependencies. It does not depend on the Spotlight runtime.
- `astrea-shell-runtime` consumes `astrea-spotlight-runtime`, and the final
  `astrea-shell` executable consumes `astrea-spotlight-wayland`.

Spotlight QML remains embedded in the unified `Astrea.Shell` QML module. Its
existing resource aliases, URLs, `SpotlightController` context property, and
`AstreaI18n` context property remain owned by Shell.

## Internal dependency direction

The `runtime` target remains an aggregate CMake target for core, services,
runtime-path resolution, and the Rust adapter. The source layers inside it now
follow these dependency directions:

```text
RustSpotlightBackend
        │ implements
        ▼
SpotlightBackend
        ▲ borrowed by
SpotlightController

SpotlightRuntimePaths
        │ resolves to
        ▼
SpotlightControllerPaths
        ▲ stored by
SpotlightController

SpotlightLauncher
        ▲ implemented by
SpotlightApplicationLauncherAdapter
        │ borrows
        ▼
ApplicationLauncher

SpotlightCatalogSource
        ▲ implemented by
SpotlightDesktopCatalogAdapter
        │ borrows
        ▼
DesktopEntryCatalog
```

`SpotlightController` borrows the core-owned backend and launcher contracts,
optionally borrows a catalog source, and stores only controller-consumed path
values. A null catalog source selects the Rust backend's native desktop-entry
index mode; a non-null source selects the shared external-catalog mode.
`SpotlightRuntimePaths` still resolves platform paths, including
`astreaLaunch()` for Shell's shared launcher, while controller paths contain
only the Astrea root and weather values.

Shell owns one shared `ApplicationLauncher` and one shared
`DesktopEntryCatalog`. It also owns one adapter for each service. The
`SpotlightApplicationLauncherAdapter` borrows the shared launcher, and the
`SpotlightDesktopCatalogAdapter` borrows the shared catalog. Both adapters and
the Rust backend outlive the controller that borrows them. Controller core no
longer owns or knows the application-launcher or desktop-catalog
implementations.

Phase 3B structural debt remains only in weather orchestration through
`QProcess`. Spotlight is not yet declared fully cleaned up or canonical.

## Responsibilities

- `app/`: the non-resident compatibility CLI and command-line parsing.
- `core/`: the controller, results model, backend/launcher/catalog ports, and
  neutral controller path values. The controller remains an
  orchestration-heavy resident object.
- `services/`: the borrowed shared-service adapters, locale, configuration
  watching, and GameMode monitoring used by the resident Shell composition.
- `platform/runtime/`: Spotlight environment and runtime path resolution, with
  conversion to `SpotlightControllerPaths`.
- `platform/rust/`: the C++ `SpotlightBackend` implementation around the Rust
  backend ABI.
- `platform/wayland/`: Spotlight surface configuration through shared Layer
  Shell infrastructure.
- `backend/`: Rust search, ranking, persistence, and native ABI implementation.
- `qml/`: the existing Spotlight presentation, embedded by Shell.
- `tests/`: runtime-linked Spotlight tests and static architecture checks.
- `cmake/`: Rust build, warning, and sanitizer helpers.

The Shell composition remains responsible for object construction and wiring,
including Bar search requests, shortcut toggles, configuration reloads, and
GameMode weather suppression.
