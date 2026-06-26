# Testing

## Local Checks

```bash
cargo fmt --check --manifest-path backend/Cargo.toml
cargo clippy --manifest-path backend/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path backend/Cargo.toml
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
qmllint qml/Main.qml qml/components/*.qml
```

## Clean Build

Delete `build/` and rebuild from scratch.

## Runtime Validation

- daemon starts hidden
- toggle works
- query works
- status works
- icons use active theme
- keyboard focus works
- outside click closes
- Escape closes
- selection wraps
- Enter launches
- weather works
- game mode policy remains unchanged
- config reload works
- index reload works
- systemd service starts

## Notes

- Use temporary XDG config/data/state directories in tests.
- Do not use the real home directory.
