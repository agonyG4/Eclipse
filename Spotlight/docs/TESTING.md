# Testing

## Build and CTest

Run these commands from the `Spotlight` directory. Build output stays in the
repository-supported Eclipse Debug tree on Aether:

```bash
cmake -S .. -B /mnt/Aether/Desktop/GitHub/Eclipse-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target \
  astrea-spotlight-runtime astrea-spotlight-wayland \
  astrea-shell-runtime astrea-shell spotlight-tests \
  shell-shortcut-dispatcher-test shell-runtime-test \
  shell-unified-runtime-integration-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -R '^(spotlight-tests|spotlight-architecture-guard|layer-shell-activation-structure|shell-shortcut-dispatcher-test|shell-runtime-test|shell-unified-runtime-integration-test|legacy-spotlight-daemon-guard)$' \
  --output-on-failure
```

`spotlight-tests` links the production `astrea-spotlight-runtime`. Its existing
tests still compile the legacy `SpotlightIpcServer.cpp` directly; that server
is not a resident production target. The `spotlight-architecture-guard` test
checks Phase 1 CMake ownership and the Phase 2 and Phase 3A source boundaries.
The runtime remains one aggregate target; core and service source layers do
not need separate CMake targets. Controller tests use fake backend, launcher,
and catalog ports. Separate adapter tests cover request conversion, event
forwarding, catalog snapshots, watched directories, and update forwarding.
Existing direct Rust backend tests remain in the Spotlight suite.

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
  astrea-spotlight-runtime astrea-spotlight-wayland \
  astrea-shell-runtime astrea-shell spotlight-tests \
  shell-shortcut-dispatcher-test shell-runtime-test \
  shell-unified-runtime-integration-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build \
  -R '^(spotlight-tests|spotlight-architecture-guard|layer-shell-activation-structure|shell-shortcut-dispatcher-test|shell-runtime-test|shell-unified-runtime-integration-test|legacy-spotlight-daemon-guard)$' \
  --output-on-failure
```

Build the relevant executables before running CTest so a missing executable is
not confused with a test failure.

## QML and Rust checks

Run the repository QML gate from the Eclipse repository root:

```bash
ASTREA_CMAKE_BUILD_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build \
  tools/ci/run-qml-gate.sh
```

Phase 3A changes no Rust source, ABI, or Rust build configuration. Run Rust
Cargo checks only if a Rust change becomes necessary.

The repository QML gate validates the generated Shell and Settings modules and
their registered QML sources.

## Runtime invariants

- `astrea-shell` remains the resident process; `astrea-spotlight` remains a
  compatibility IPC client and rejects `--daemon`.
- Show, hide, toggle, query, selection, launch, and usage recording behavior
  remain unchanged.
- Catalog/native-index creation, locale recreation, reload, watched-directory,
  update, and search behavior remain unchanged. Controller tests use core fakes;
  production adapters borrow Shell's shared launcher and catalog.
- Weather/`QProcess` orchestration remains the Phase 3B structural debt.
- Layer Shell configuration, Bar search integration, shortcut integration,
  and QML resource/context-property APIs remain unchanged.
- Icon diagnostics remain available from `astrea-spotlight`.

Use temporary XDG config, data, and state directories for runtime checks; do
not use the real home directory.
