# Astrea Native Spotlight

Native Qt Quick + Rust Spotlight hosted by the unified `astrea-shell` process.

## Runtime and CMake ownership

`astrea-shell` is the resident process. `astrea-spotlight` remains a lightweight
compatibility IPC client and sends resident commands through the Shell IPC
protocol to `ShellIpcServer`. `astrea-spotlight --daemon` is intentionally
rejected; it does not start a standalone Spotlight daemon. Icon diagnostics
remain available in the compatibility executable.

The resident implementation is owned by `astrea-spotlight-runtime`.
`astrea-spotlight-wayland` owns Spotlight's `LayerShellSurface` adapter. Shell
continues to construct and wire Spotlight with the catalog, launcher,
configuration watcher, GameMode monitor, locale service, shortcuts, Bar search,
and existing unified QML module.

Internal Spotlight layering remains unfinished. The runtime target is an
ownership boundary; `SpotlightController` still depends directly on runtime
paths, the Rust wrapper, launcher, catalog, and its weather process handling.
See [the architecture](docs/ARCHITECTURE.md) and [runtime flow](docs/RUNTIME_FLOW.md).

## Build and test

Run these commands from the `Spotlight` directory. They reuse the Eclipse
Debug build tree on Aether:

```bash
cmake -S .. -B /mnt/Aether/Desktop/GitHub/Eclipse-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target \
  astrea-spotlight-runtime astrea-spotlight-wayland astrea-spotlight \
  astrea-shell-runtime astrea-shell spotlight-tests
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -R '(spotlight|layer-shell-activation)' --output-on-failure
```

## Rust and QML

```bash
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo fmt --check --manifest-path backend/Cargo.toml
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo clippy --manifest-path backend/Cargo.toml --all-targets -- -D warnings
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target \
  cargo test --manifest-path backend/Cargo.toml
qmllint qml/Main.qml qml/components/*.qml
```

## Packaging

The `astrea-shell` systemd user unit installs to `share/systemd/user`.
`astrea-shell` remains the resident process; the `astrea-spotlight` executable
is a non-resident compatibility IPC client.

See [IPC](docs/IPC.md) for compatibility-client behavior and
[testing](docs/TESTING.md) for the no-Layer-Shell build.
