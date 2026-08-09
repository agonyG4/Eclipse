# Astrea Native Spotlight

Native Qt Quick + Rust Spotlight for Astrea.

## Architecture

See `docs/ARCHITECTURE.md` and `docs/RUNTIME_FLOW.md`.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Rust

```bash
cargo fmt --check --manifest-path backend/Cargo.toml
cargo clippy --manifest-path backend/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path backend/Cargo.toml
```

## QML

```bash
qmllint qml/Main.qml qml/components/*.qml
```

## Packaging

Spotlight runs inside the unified `astrea-shell` process. The systemd user unit
installs to `share/systemd/user/astrea-shell.service`; `astrea-spotlight` is a
non-resident IPC compatibility client.

## CLI

`astrea-spotlight --daemon`, `--show`, `--hide`, `--toggle`, `--query`, `--activate`, `--reload-index`, `--status`, `--resolve-icon`, and `--icon-theme` remain supported.
