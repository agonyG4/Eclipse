# Astrea Paper Wallpaper API v1.1 — Completion Report

Date: 2026-08-19  
Scope: Eclipse Paper wallpaper service, IPC protocol, Settings client, Typhon `astreactl`, source watching, packaging, rendering, and qualification.

## Outcome

The v1.1 hardening work is implemented and verified in the available build/test environments. Wallpaper mutations now have transactional completion, authoritative snapshots, typed terminal statuses, asynchronous IPC clients, source reconciliation, packaged factory artwork, and generation-aware rendering.

The work was performed in the existing dirty Eclipse and Typhon worktrees. No files were reset, staged, committed, or discarded. Existing unrelated changes were preserved.

## Implemented contract

`WallpaperService` now returns a monotonic operation ID for `setWallpaper` and `resetWallpaper` and emits exactly one terminal `WallpaperOperationResult` for each accepted mutation. Terminal statuses are:

- `succeeded`
- `rejected`
- `superseded`
- `persistenceFailed`
- `cancelledByReset`
- `shutdown`

Successful mutations persist before publishing the new authoritative snapshot. Validation and persistence failures retain the previous authoritative state and generation. Newer sets supersede older pending or active sets. Reset cancels/supersedes outstanding sets, clears source watching, restores the factory baseline, and completes as its own operation.

The service destructor completes outstanding mutations with `shutdown`, avoiding clients waiting indefinitely during teardown.

## IPC and clients

The Unix-socket protocol now treats `set` and `reset` as asynchronous transactions. Every mutation acknowledgement is a final response with `completed: true`, `requestId`, `status`, and either an authoritative `snapshot` or typed `errorCode`/`message`. Incomplete responses are not accepted as completion by clients.

The server enforces bounded command size, client count, idle lifetime, operation lifetime, and completion-cache size. It owns and removes the endpoint only when it successfully created that endpoint, so an instance cannot remove another instance's socket.

The Settings controller uses one asynchronous `QLocalSocket` request pipeline with timeout and stale-reply protection. It performs no blocking connect, write, or nested event-loop wait. QML disables conflicting controls while a mutation is pending and displays the pending state/error.

`astreactl` accepts Unicode and spaces in JSON responses, requires final completion, and maps Paper's typed failure code/message into its existing nonzero Paper error path. It rejects an acknowledgement that is still marked `completed: false`.

## Source reconciliation and rendering

The source watcher observes the configured local file and its parent directory with a short debounce. It handles disappearance, recreation, atomic replacement, and corrupt-content changes without losing a queued reconciliation event. A missing or invalid configured source retains the configured value while falling back to the factory effective source; a valid recovery republishes the effective source and advances the generation.

The renderer carries `wallpaperGeneration` through the surface properties. QML starts a new asynchronous image load whenever the generation changes and keeps the existing two-slot swap behavior, preventing a stale image from being treated as the current authoritative result.

## Factory packaging and provenance

The default artwork was copied from the canonical Astrea source:

`/home/agony/.local/share/Astrea/src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg`

The source file is encoded as WebP despite its `.jpg` suffix, while the available Qt image plugins do not include WebP. It was therefore normalized to a Qt-supported JPEG at:

`/home/agony/GitHub/Eclipse/Paper/assets/default.jpg`

The packaged resolver order is explicit override, `ASTREA_WALLPAPER_DEFAULT`, the QML resource default, legacy `ASTREA_ROOT` locations, generic data locations, then the emergency fallback. The emergency fallback remains separate from the packaged factory artwork and is still exercised when no real candidate exists.

## Verification

The following Eclipse command passed with 12/12 tests:

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R 'paper-|settings-(controller|wallpaper|qml|structure)|shell-(runtime|ipc)-test' --output-on-failure
```

This covered descriptor validation, resolver packaging/fallback behavior, persistence, transactional service behavior, source reconciliation, surface management, IPC completion and bounds, Settings async behavior, QML smoke/structure checks, shell runtime, and shell IPC.

The LayerShell-enabled release target also built successfully:

```text
cmake --build build/release --target astrea-shell -j2
```

Typhon checks passed:

```text
cargo fmt --check
cargo test astreactl::wallpaper --lib
cargo test --bin astreactl
cargo check --bin astreactl
```

The focused client tests passed 3/3 and the binary tests passed 2/2. A broader `cargo test --lib astreactl` run reached 20/22; the two pre-existing discovery tests failed before Paper assertions because the current workspace path exceeds the discovery protocol's `SUN_LEN` limit. Those discovery tests were not changed.

## Native qualification

The LayerShell-enabled `astrea-shell` was attempted on the available real Wayland session (`WAYLAND_DISPLAY=wayland-1`). The binary exited with status 1 before creating a usable Paper control socket, and no Typhon compositor process/session was available for end-to-end interaction. Consequently, live multi-output, compositor-mediated set/reset, and visual native qualification could not be claimed. The report's passing qualification is limited to the offscreen Qt suite, direct IPC/service tests, client tests, and successful production-target compilation.

## Relevant files

- `Paper/core/WallpaperService.hpp` and `WallpaperService.cpp`
- `Paper/core/WallpaperSourceWatcher.hpp` and `WallpaperSourceWatcher.cpp`
- `Paper/core/WallpaperResolver.cpp`
- `Paper/platform/ipc/WallpaperControlServer.hpp` and `WallpaperControlServer.cpp`
- `Paper/platform/wayland/WallpaperSurfaceBundle.cpp`
- `Paper/qml/WallpaperSurface.qml`
- `Settings/services/wallpaper/SettingsWallpaperController.*`
- `Typhon/src/astreactl/client.rs`
- `Typhon/src/astreactl/wallpaper.rs`
- `Typhon/src/bin/astreactl.rs`

