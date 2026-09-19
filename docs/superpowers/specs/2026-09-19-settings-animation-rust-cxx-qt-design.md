# Settings Animations Rust/CXX-Qt Migration Design

## Goal and compatibility boundary

Migrate the Settings Animations/Typhon backend from the two hand-maintained
C++ implementation files to a focused Rust crate while preserving the current
`SettingsController.animations` QObject contract and the behavior observed by
`Animations.qml` and the existing animation tests.

The migration is incremental. QML remains presentation and interaction,
`SettingsController` remains the C++ composition root, the generated CXX-Qt
QObject is the thin Qt boundary, and Rust owns animation state, validation,
capability projection, Typhon protocol handling, secure endpoint discovery,
transport, mutation ordering, and asynchronous work.

No other Settings backend or Eclipse component is migrated in this phase.

## Alternatives

1. **Spotlight-style C ABI with JSON values** — rejected. It would make the
   stateful QObject boundary an ad-hoc C ABI and would move JSON across the
   Rust/Qt boundary, contrary to the requested CXX-Qt architecture.
2. **A second QML module/plugin for the Rust object** — rejected. The existing
   Astrea Settings QML module and context-property composition remain the
   authoritative application API.
3. **Direct CXX-Qt static QObject integration** — selected. CXX-Qt generates
   the QObject header/source from Rust, `SettingsController` includes that
   generated header, and the existing `animations` property continues to
   expose the object without QML changes.

## Rust crate and ownership

Create `Settings/backend` as a library crate with this structure:

```text
Settings/backend/
  Cargo.toml
  Cargo.lock
  build.rs
  src/
    lib.rs
    animation/
      mod.rs
      qobject.rs
      state.rs
    typhon/
      mod.rs
      protocol.rs
      discovery.rs
      client.rs
```

`animation/state.rs` contains Qt-independent typed configuration, snapshot,
catalogue, capability projection, defaults, validation, mutation ordering,
pending speed folding, and authoritative snapshot application.

`typhon/protocol.rs` contains typed serde request/response structures and the
wire validation rules. JSON is confined to serialization at the Typhon wire
boundary; the animation domain never uses `serde_json::Value` as its model.

`typhon/discovery.rs` reproduces the existing XDG runtime, Astrea/Typhon
instance, effective-user, `lstat`, mode, instance-name, preferred
`WAYLAND_DISPLAY`, and ambiguity checks.

`typhon/client.rs` runs one request at a time on one bounded worker thread.
Unix socket connect/read/write/deadline work is never performed on the Qt
thread. The worker has a bounded request channel and uses monotonic deadlines;
there is no thread per request or slider event.

`animation/qobject.rs` is deliberately thin. It owns projected Qt values,
notify signals, invokables, request tokens, the worker handle, and queued
completion handling. CXX-Qt's `CxxQtThread` is used for completion callbacks;
queue failure is treated as normal shutdown and never panics. The worker is
detached on QObject destruction after its bounded socket deadline, so QObject
destruction does not synchronously wait for I/O.

## Qt API and data projection

The generated class remains named `SettingsAnimationController` and exposes
the existing properties:

```text
available, busy, enabled, preset, speed, generation, source,
hasOverrides, slots, presets, lastError
```

It exposes the existing invokables with the same C++/QML names. Custom CXX-Qt
notify declarations preserve the existing signal grouping:

- availability changes use `availabilityChanged`;
- request state changes use `busyChanged`;
- authoritative snapshot/configuration projections use `snapshotChanged`;
- user-visible failures use `errorChanged`.

`QVariantList`/`QVariantMap` compatibility is represented with CXX-Qt's
stable `QList<QVariant>` and `QMap<QString, QVariant>` bindings. Conversion is
performed only in the QObject projection layer, without JSON serialization or
policy in C++.

`SettingsController.hpp` consumes the generated `.cxxqt.h` header. No second
wrapper controller or QML singleton is introduced, and `Animations.qml` is
not behaviorally changed.

## Requests, state transitions, and errors

The Rust state machine preserves the existing behavior:

- defaults are enabled, preset `astrea`, speed `1.0`, and no overrides;
- speed is finite, clamped to `0.5..=2.0`, and folded into the next
  submitted mutation after the approximately 80 ms debounce;
- `flush()` commits a pending speed immediately;
- mutations are ordered and coalesced only according to the existing pending
  mutation semantics;
- successful Typhon snapshots are authoritative and incomplete snapshots are
  rejected;
- planned and unavailable effects remain projected separately and cannot be
  submitted; selecting one produces the existing unavailable-effect error;
- server-side configuration rejection leaves the compositor available, while
  transport, discovery, timeout, and protocol failures mark it unavailable;
- every completion carries an operation token, so a stale completion cannot
  mutate newer state.

The protocol keeps newline-framed `astrea.control` version 1 requests, one
in-flight request, 64 KiB maximum requests, 1 MiB maximum responses, a 2000 ms
default deadline, exact one-response framing, matching integral IDs, boolean
`ok`, object success results, and object failure errors. Malformed external
input is converted into a bounded error result rather than a panic.

## Build and CXX-Qt integration

Use the latest stable published CXX-Qt release verified at implementation time:
`cxx-qt`, `cxx-qt-build`, and `cxx-qt-lib` `0.10.0`. Integrate the official
CXX-Qt CMake helper at the matching `cxx-qt-cmake` `0.10.0` tag through
`cxx_qt_import_crate`, with `LOCKED` and Qt modules matching the CMake Qt
installation. The crate `build.rs` uses `CxxQtBuilder` and includes only the
QObject bridge source; it does not define a QML module.

The CMake integration explicitly resolves qmake/qmake6 from the same Qt
installation selected by CMake and fails configuration if it cannot verify a
compatible Qt installation. Rust artifacts and CXX-Qt generated files are
built as part of the normal Eclipse CMake build in the repository build tree;
no separate manual Cargo build is required.

The Settings Rust gate is extended alongside Spotlight with locked format,
clippy warnings-as-errors, and tests. The existing Spotlight gate remains
unchanged in behavior.

## Tests

Protocol, discovery, typed parsing, bounds, mutation semantics, capabilities,
debounce folding, and stale-operation rules move to Rust unit tests. The
existing Qt test retains an in-process Unix control server and proves the
generated QObject behavior from the application side, including authoritative
snapshots, planned/unavailable rejection, async completion, timeout/event-loop
responsiveness, server rejection availability, and pending-speed folding.

The Qt boundary adds shutdown/stale-completion coverage where practical. Static
Settings structure tests assert that the generated Rust backend is in the
production target and that the superseded C++ animation/client implementation
is not listed or compiled.

## Documentation and deferred work

Update the drifted Settings README, architecture, migration, structure, and
testing documentation only where it describes the Settings backend boundary or
Typhon IPC. State explicitly that Animations is the first incremental Rust
backend migration and that the migration is not an all-at-once rewrite.

Theme, Dock, Wallpaper, navigation, Shell services, Audio, Bluetooth,
Network, Typhon itself, the Compositor preview, and all new Settings pages are
deferred.
