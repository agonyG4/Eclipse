# Settings Verification

## Unit tests

Configure and build the focused target:

```bash
cmake -S . -B build-settings-foundation -G Ninja \
  -DASTREA_BUILD_TESTS=ON \
  -DASTREA_SETTINGS_BUILD_TESTS=ON
cmake --build build-settings-foundation --target settings-controller-test
ctest --test-dir build-settings-foundation --output-on-failure \
  -R '^settings-controller-test$'
```

The test covers initial `system` selection, known and unknown IDs,
case-insensitive filtering, reversible filtering, and exactly one selected
model row.

## QML lint

Use the generated import directory so both `Astrea.Settings` and
`Astrea.Shared` resolve:

```bash
find Settings/qml -name '*.qml' -print0 \
  | xargs -0 -n1 qmllint -I build-settings-foundation
```

Every QML file under `Settings/qml` must be registered exactly once in
`Settings/CMakeLists.txt`.

## Sanitizers

```bash
cmake -S . -B build-settings-sanitized -G Ninja \
  -DASTREA_BUILD_TESTS=ON \
  -DASTREA_SETTINGS_BUILD_TESTS=ON \
  -DASTREA_ENABLE_ASAN=ON \
  -DASTREA_ENABLE_UBSAN=ON
cmake --build build-settings-sanitized --target settings-controller-test
ctest --test-dir build-settings-sanitized --output-on-failure \
  -R '^settings-controller-test$'
```

## Policy scan

The source and QML implementation must not contain Quickshell, QML
`Process`, `hyprctl`, `hyprland`, `systemctl`, shell invocation, `JsonAdapter`,
or `FileView`.

## Manual Typhon smoke test

1. Launch `astrea-settings` in the normal Typhon session.
2. Confirm it maps as a normal toplevel rather than a layer surface.
3. Drag the title bar and confirm system move behavior.
4. Minimize, maximize, restore, and close the window.
5. Collapse and expand the sidebar; confirm the selected ID is preserved.
6. Select every navigation item; confirm only placeholder title changes.
7. Type `BLUE`; confirm only Bluetooth remains.
8. Clear search; confirm all nine model rows return.
9. Resize to `800x500`; confirm no control is clipped or inaccessible.
