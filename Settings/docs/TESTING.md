# Settings Verification

All verification uses fresh build directories. Do not use a checked-in or
archived build directory as evidence.

## Debug and Release

```bash
cmake -S . -B /tmp/eclipse-settings-structure-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build /tmp/eclipse-settings-structure-debug --parallel
ctest --test-dir /tmp/eclipse-settings-structure-debug --output-on-failure
ctest --test-dir /tmp/eclipse-settings-structure-debug -N

cmake -S . -B /tmp/eclipse-settings-structure-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build /tmp/eclipse-settings-structure-release --parallel
ctest --test-dir /tmp/eclipse-settings-structure-release --output-on-failure
```

The Settings tests cover controller behavior, navigation descriptors and
selection, profile composition, Linux group enumeration and policy, theme
compatibility, the full application QML route, representative registered QML
components, Compositor source policy, and structural ownership invariants.

The repository-level `create-source-archive-test` runs Bash syntax checks and
qualifies a Git-based archive in an isolated temporary repository.

## QML Registration and Lint

The authoritative QML list is in `qml/CMakeLists.txt`. It contains 35 files and
is registered once by `astrea-settings-ui`. The application and integration
tests consume the same module and generated plugin.

Build the module lint target in a fresh build:

```bash
cmake --build /tmp/eclipse-settings-structure-debug --target astrea-settings-ui_qmllint
```

The final report must record the registered count, linted count, and warning and
error counts. Registered and linted counts must match.

## Source and Dependency Policy

Production Settings source must have zero matches for Quickshell,
`Quickshell.Io`, LayerShellQt, `hyprctl`, `QProcess`, QML `Process`,
`system(`, `popen(`, `pageIndex`, Typhon-private protocol names, and process or
IPC access in `Compositor.qml`.

`astrea-settings-core` must expose only Qt Core in its link interface. The
freshly built Settings executable must have no LayerShellQt or unexpected
compositor dependency in `readelf -d` or `ldd`.

## Manual Visual Qualification

The agent does not inspect or control the desktop session, use desktop input or
screenshot automation, or claim visual parity. The user performs the manual
Hyprland qualification with the exact freshly built executable path supplied in
the final report. No Typhon session is launched by this workflow.
