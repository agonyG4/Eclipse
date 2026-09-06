# Settings Verification

All verification uses fresh build directories. Do not use a checked-in or
archived build directory as evidence.

## Debug and Release

```bash
cmake -S . -B build/settings-dock-defaults-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DASTREA_BUILD_TESTS=ON -DASTREA_SETTINGS_BUILD_TESTS=ON \
  -DASTREA_ENABLE_LAYER_SHELL=OFF
cmake --build build/settings-dock-defaults-debug --parallel
ctest --test-dir build/settings-dock-defaults-debug --output-on-failure
cmake --build build/settings-dock-defaults-debug --target astrea-settings-ui_qmllint

cmake -S . -B build/settings-dock-defaults-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DASTREA_BUILD_TESTS=ON -DASTREA_SETTINGS_BUILD_TESTS=ON \
  -DASTREA_ENABLE_LAYER_SHELL=OFF
cmake --build build/settings-dock-defaults-release --parallel
ctest --test-dir build/settings-dock-defaults-release --output-on-failure
```

The Settings tests cover controller behavior, Dock defaults and typed atomic
persistence, deferred external Dock replacement and pin preservation,
navigation descriptors, flat Page/Hub/Spacer routing, nested destination
resolution, and bounded Back/Forward history,
profile composition, Linux group enumeration and policy, theme compatibility,
the full application QML route, disabled deferred Wallpaper controls,
representative registered QML components, AppIcon provider ownership,
Compositor source policy, and structural ownership invariants.

Wallpaper correctness is covered by `paper-catalog-test`,
`paper-service-test`, `paper-control-server-test`, and
`settings-wallpaper-controller-test`. These tests exercise native preview URL
projection (including resource and special-character local paths), managed
content-addressed removal, active-wallpaper rejection, bounded JSON removal,
refreshed catalog responses, and Settings' non-optimistic removal transport.

The repository-level `create-source-archive-test` runs Bash syntax checks and
qualifies a Git-based archive in an isolated temporary repository.

## QML Registration and Lint

The authoritative QML list is in `qml/CMakeLists.txt`. It contains 40 files and
is registered once by `astrea-settings-ui`. The application and integration
tests consume the same module and generated plugin.

Build the module lint target in a fresh build:

```bash
cmake --build build/settings-dock-defaults-debug --target astrea-settings-ui_qmllint
```

The final report must record the registered count, linted count, and warning and
error counts. Registered and linted counts must match.

After implementation, run these source audits from the repository root:

```bash
grep -RIn 'Form\.ToggleSwitch\|Form\.SelectButton\|Form\.SearchField' Settings/qml
grep -RIn 'components/form/ToggleSwitch.qml\|components/form/SelectButton.qml\|components/form/SearchField.qml' Settings
grep -nE '^[[:space:]]*Slider[[:space:]]*\{' Settings/qml/pages/appearance/Dock.qml
rg -n 'Controls\.Slider|detentValue|writeConfig|writePersonalization' Settings/qml/pages/appearance/Dock.qml Settings/services/dock
```

The first three current-source results must be empty. The final audit should
show exactly nine `Controls.Slider` consumers, only controller-sourced detent
values, and persistence through `writePersonalization()` rather than
`writeConfig()`. Historical documentation may mention the old paths only when
it is clearly describing the migration.

## Source and Dependency Policy

Production Settings source must have zero matches for Quickshell,
`Quickshell.Io`, LayerShellQt, `hyprctl`, `QProcess`, QML `Process`,
`system(`, `popen(`, `pageIndex`, Typhon-private protocol names, and process or
IPC access in `Compositor.qml`.

The Dock QML route is loaded offscreen through the same registered module and
checks its preview, nine native Slider consumers and canonical detents,
formatted value text, Restore Defaults footer, local Undo toast, disabled-state
dependencies, and required translation keys. Its source has no JSON, file,
process, IPC, DBus, compositor, Shell, Typhon, or Layer Shell API access. The
real Quick-window Slider fixture also covers continuous editing, hysteretic
default detents, one-shot pulse entry, mirrored marker geometry, and keyboard
crossing of the detent.

`astrea-settings-core` must expose only Qt Core and `astrea-shared-dock` in its
public link interface; Qt Network and the direct Paper protocol include are
private implementation details. The freshly built Settings executable must
have no LayerShellQt or unexpected compositor dependency in `readelf -d` or
`ldd`.

## Manual Visual Qualification

The agent does not inspect or control the desktop session, use desktop input or
screenshot automation, or claim visual parity. The user performs the manual
Hyprland qualification with the exact freshly built executable path supplied in
the final report. Check light and dark themes, two accent colors, minimum/
middle/maximum values, hover, pressed/dragged, disabled magnification/
animation-speed/indicator-size controls, live Dock preview updates, endpoint
alignment, and no clipping at the minimum Settings window size. Also check
detent approach/release from both sides, keyboard and wheel crossing where
supported, tooltip placement and formatting, Restore Defaults, immediate
preview updates, Undo, and toast expiry. No Typhon session is launched by this
workflow.
