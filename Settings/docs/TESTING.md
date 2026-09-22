# Settings Verification

All compilation, test output, generated files, Cargo targets, and temporary
build output must stay under `/mnt/Aether/Desktop/GitHub`. The source checkout
remains at `/home/agony/GitHub/Eclipse`. Reuse the stable Eclipse build tree
`/mnt/Aether/Desktop/GitHub/Eclipse-build`; verify its resolved path and
`CMAKE_HOME_DIRECTORY` before configuring or building. Set `TMPDIR` to the
short Aether path `/mnt/Aether/Desktop/GitHub/t` when invoking compiler/build
commands; this also keeps temporary Unix socket paths below Linux's length
limit.

## Existing build directory

```bash
export CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target
export TMPDIR=/mnt/Aether/Desktop/GitHub/t
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --parallel 4
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build --output-on-failure
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target astrea-settings-ui_qmllint --parallel 4
```

Use the build type already recorded in
`/mnt/Aether/Desktop/GitHub/Eclipse-build/CMakeCache.txt`; do not create a
second directory to verify another configuration. Reconfigure that tree in
place only when it is genuinely necessary.

The Settings tests cover controller behavior, Dock defaults and typed atomic
persistence, deferred external Dock replacement and pin preservation,
navigation descriptors, flat Page/Hub/Spacer routing, nested destination
resolution, and bounded Back/Forward history,
profile composition, Linux group enumeration and policy, theme compatibility,
the full application QML route, disabled deferred Wallpaper controls,
representative registered QML components, AppIcon provider ownership,
Compositor source policy, and structural ownership invariants.

Icons coverage includes Rust catalog priority/deduplication, hidden metadata,
split roots, inheritance and bounded preview lookup, conservative atomic
preference persistence, invalid selection rejection, and worker coalescing.
Settings tests cover the generated `SettingsIconsController` projection, the
stable `icons` route, page construction, System Default selection, keyboard
activation, filtering, and bounded loading/error/empty presentation. Appearance
Rust tests cover validation, isolated key patches, malformed-file protection,
shared locking, and bounded/coalesced worker mutations. ThemeConfigStore tests
cover atomic replacement and compatibility with the C++ lock path. Shared icon
tests cover persisted-preference precedence, environment overrides, invalid
fallback, and watcher-driven provider invalidation. ThemeController tests prove
that unrelated legacy saves preserve Rust-owned keys and compatibility inputs.

The Settings Animations test also exercises the Rust-backed CXX-Qt QObject from
the application's perspective: authoritative snapshots, planned and
unavailable effect rejection, server rejection availability, timeout/event-loop
responsiveness, one in-flight request behavior, malformed responses, endpoint
security failures, oversized mutations, pending-speed folding, and destruction
while a request is outstanding. Protocol and secure-discovery cases that do
not need Qt live in `Settings/backend` Rust unit tests.

The Settings Rust gate uses the crate's lockfile and runs:

```bash
export CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target
export TMPDIR=/mnt/Aether/Desktop/GitHub/t
rtk cargo fmt --check --manifest-path Settings/backend/Cargo.toml
rtk cargo test --manifest-path Settings/backend/Cargo.toml
rtk cargo clippy --manifest-path Settings/backend/Cargo.toml --all-targets -- -D warnings
```

The CXX-Qt integration uses the stable 0.10.0 release and the qmake selected
by CMake. A normal Settings CMake build owns generated CXX-Qt headers and the
Rust static library in the Aether CMake build tree. Set `CARGO_TARGET_DIR`
explicitly for manual Cargo commands; CMake's configured Rust target directory
must also resolve under Aether.

Wallpaper correctness is covered by `paper-catalog-test`,
`paper-service-test`, `paper-control-server-test`, and
`settings-wallpaper-controller-test`. These tests exercise native preview URL
projection (including resource and special-character local paths), managed
content-addressed removal, active-wallpaper rejection, bounded JSON removal,
refreshed catalog responses, and Settings' non-optimistic removal transport.

The repository-level `create-source-archive-test` runs Bash syntax checks and
qualifies a Git-based archive in an isolated temporary repository.

## QML Registration and Lint

The authoritative QML list is in `qml/CMakeLists.txt`. It contains 46 files and
is registered once by `astrea-settings-ui`. The application and integration
tests consume the same module and generated plugin.

Build the module lint target from the existing build:

```bash
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target astrea-settings-ui_qmllint --parallel 4
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
