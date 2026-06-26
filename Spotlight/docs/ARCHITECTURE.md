# Architecture

```text
Rust backend
    ↓ C ABI
platform/rust
    ↓ QObject / QAbstractListModel
core
    ↓ services / platform
app
    ↓ Qt Quick/QML
qml
```

## Top-Level Responsibilities

- `app/`: process bootstrap and composition.
- `core/`: Spotlight state, orchestration, and the results model.
- `services/`: reusable process/watchers/monitoring helpers.
- `platform/`: IPC, icon resolution, Wayland, runtime paths, Rust bridge.
- `backend/`: Rust search, ranking, persistence, and FFI.
- `qml/`: presentation-only UI.
- `tests/`: native validation.
- `packaging/`: systemd and other install-time assets.
- `cmake/`: build helpers.

## Ownership

- `SpotlightApplication` owns the app lifecycle and wires dependencies.
- `SpotlightController` owns open state, query, selection, weather, game mode, and launch coordination.
- `SpotlightResultsModel` owns the QAbstractListModel boundary to QML.
- `RustSpotlightBackend` owns the C ABI handle and JSON conversion.
- QML reads only QObject/model APIs.

## Boundary Rules

- Backend has no Qt dependency.
- QML does not access filesystem, process, XDG, or backend logic directly.
- Core does not touch Wayland or sockets directly.

## Rust/C++ Boundary

- The only contract is `backend/include/astrea_spotlight_backend.h`.
- C++ converts Qt types to/from UTF-8/JSON and forwards calls across the ABI.
