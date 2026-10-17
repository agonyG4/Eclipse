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
  `SpotlightResultsModel`, `AstreaI18n`, `GameModeMonitor`,
  `SpotlightConfigWatcher`, `SpotlightRuntimePaths`, and
  `RustSpotlightBackend`. It links the shared core and Qt Core publicly, while
  keeping the native Rust backend and its system link requirements private.
- `astrea-spotlight-wayland` owns `LayerShellSurface` and its Qt and shared
  Layer Shell dependencies. It does not depend on the Spotlight runtime.
- `astrea-shell-runtime` consumes `astrea-spotlight-runtime`, and the final
  `astrea-shell` executable consumes `astrea-spotlight-wayland`.

Spotlight QML remains embedded in the unified `Astrea.Shell` QML module. Its
existing resource aliases, URLs, `SpotlightController` context property, and
`AstreaI18n` context property remain owned by Shell.

## Current internal structure

The `runtime` target is an ownership boundary, not a claim that Spotlight core
is platform-independent. `SpotlightController` still directly owns or uses
`RustSpotlightBackend`, `SpotlightRuntimePaths`, `ApplicationLauncher`, and
`DesktopEntryCatalog`; it also runs weather processes through `QProcess`.
Phase 1 makes source ownership and production linkage truthful while preserving
these known internal dependencies for later cleanup.

## Responsibilities

- `app/`: the non-resident compatibility CLI and command-line parsing.
- `core/`: the current controller and results model. The controller remains
  an orchestration-heavy resident object in this phase.
- `services/`: locale, configuration watching, and GameMode monitoring used by
  the resident Shell composition.
- `platform/runtime/`: Spotlight runtime path resolution.
- `platform/rust/`: the C++ wrapper around the Rust backend ABI.
- `platform/wayland/`: Spotlight surface configuration through shared Layer
  Shell infrastructure.
- `backend/`: Rust search, ranking, persistence, and native ABI implementation.
- `qml/`: the existing Spotlight presentation, embedded by Shell.
- `tests/`: runtime-linked Spotlight tests and static architecture checks.
- `cmake/`: Rust build, warning, and sanitizer helpers.

The Shell composition remains responsible for object construction and wiring,
including Bar search requests, shortcut toggles, configuration reloads, and
GameMode weather suppression.
