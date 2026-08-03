# Settings Verification

## Fresh Debug Build

Use a fresh build directory and build the complete project:

```bash
cmake -S . -B /tmp/astrea-settings-cleanup-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build /tmp/astrea-settings-cleanup-debug --parallel
ctest --test-dir /tmp/astrea-settings-cleanup-debug --output-on-failure
ctest --test-dir /tmp/astrea-settings-cleanup-debug -N
```

The Settings tests are:

- `settings-controller-test`;
- `admin-group-detector-test`;
- `settings-group-membership-test`;
- `theme-controller-test`;
- `settings-qml-smoke-test`;
- `compositor-page-source-test`.

The QML smoke test uses the offscreen platform, creates the native controllers,
loads the `Astrea.Settings` module, checks one root object, selects Compositor,
checks page creation and recreation, and fails on QML warnings.

The administrative-group detector tests inject the libc/NSS boundary and cover
immediate non-zero success, buffer resizing, wheel and sudo policy, unrelated
and empty groups, lookup failure, bounded repeated resize failures, and the
`SettingsController::isSudo` property.

## QML Registration and Lint

The authoritative QML list is `SETTINGS_QML_FILES` in
`Settings/CMakeLists.txt`. It contains 35 files. Every listed file must exist,
be registered once, and pass `qmllint` with the generated build import path.

The lint command must pass the complete CMake list, including `qml/Main.qml`
and `qml/pages/system/Compositor.qml`, rather than a hand-maintained subset.

## Policy Scan

Production Settings source must have zero matches for Quickshell,
`Quickshell.Io`, LayerShellQt, `hyprctl`, `QProcess`, QML `Process`,
`system(`, `popen(`, Typhon-private protocol names, persistence, or IPC in
`Compositor.qml`. Generated build directories and historical documentation are
excluded from this scan.

The freshly built Settings executable must also have no LayerShellQt entry in
`readelf -d` or `ldd`. Dock, Spotlight, and AltTab are the Layer Shell
consumers and must retain their LayerShellQt linkage.

## Manual Hyprland Smoke Test

Launch the freshly built native `astrea-settings` executable under the current
Hyprland session. Do not launch Typhon.

Verify that the approved shell is unchanged, Compositor remains immediately
after Services, Performance/Appearance/More Settings remain normal selectable
rows, no row expands or collapses, and the Compositor page loads. Toggle every
switch, select each selector option, leave and return to confirm defaults are
recreated, and close/reopen to confirm no preview value persists or applies.
