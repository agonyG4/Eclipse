# Testing

## Build and CTest

Run these commands from the `Spotlight` directory. Build output stays in the
repository-supported Eclipse Debug tree on Aether:

```bash
cmake -S .. -B /mnt/Aether/Desktop/GitHub/Eclipse-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target \
  astrea-spotlight-runtime astrea-spotlight-wayland astrea-spotlight \
  astrea-shell-runtime astrea-shell spotlight-tests \
  shell-shortcut-dispatcher-test shell-runtime-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -R '(spotlight|layer-shell-activation|shell-shortcut-dispatcher-test|shell-runtime-test)' \
  --output-on-failure
```

`spotlight-tests` links the production `astrea-spotlight-runtime`. Its existing
tests still compile the legacy `SpotlightIpcServer.cpp` directly; that server
is not a resident production target. The `spotlight-architecture-guard` test
checks Phase 1 CMake ownership and Phase 2 backend/path source boundaries. The
runtime remains one aggregate target; core and platform source layers do not
need separate CMake targets. Controller tests use a fake backend, while direct
adapter tests continue to exercise the Rust ABI.

The `layer-shell-activation-structure` test protects the existing activation
structure. Build `astrea-spotlight-wayland` explicitly even when only running
the Spotlight test executable; the Spotlight backend tests do not link or
exercise that adapter.

## No-Layer-Shell build

For non-production or test builds that disable Layer Shell, reuse the
repository's separate Aether build tree:

```bash
cmake -S .. -B /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DASTREA_ENABLE_LAYER_SHELL=OFF
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build --target \
  astrea-spotlight-runtime astrea-spotlight-wayland astrea-spotlight \
  astrea-shell-runtime astrea-shell spotlight-tests
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build \
  -R '(spotlight|layer-shell-activation)' --output-on-failure
```

Build the relevant executables before running CTest so a missing executable is
not confused with a test failure.

## Rust and QML checks

Direct Cargo checks share the Debug build's Aether target directory:

```bash
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo fmt --check --manifest-path backend/Cargo.toml
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo clippy --manifest-path backend/Cargo.toml --all-targets -- -D warnings
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo test --manifest-path backend/Cargo.toml
qmllint qml/Main.qml qml/components/*.qml
```

The repository QML gate validates the generated Shell and Settings modules and
their registered QML sources. Run it from the Eclipse repository root:

```bash
ASTREA_CMAKE_BUILD_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build \
  tools/ci/run-qml-gate.sh
```

## Runtime invariants

- `astrea-shell` remains the resident process; `astrea-spotlight` remains a
  compatibility IPC client and rejects `--daemon`.
- Show, hide, toggle, query, selection, launch, and usage recording behavior
  remain unchanged.
- Catalog updates, weather, GameMode suppression, configuration reload, and
  locale behavior remain unchanged. Phase 3 cleanup of launcher ownership,
  catalog coupling, and weather/`QProcess` orchestration remains deferred.
- Layer Shell configuration, Bar search integration, shortcut integration,
  and QML resource/context-property APIs remain unchanged.
- Icon diagnostics remain available from `astrea-spotlight`.

Use temporary XDG config, data, and state directories for runtime checks; do
not use the real home directory.
