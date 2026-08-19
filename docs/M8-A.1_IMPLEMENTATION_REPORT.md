# M8-A.1 Native TopBar implementation report

## Outcome

The M8-A native TopBar closure is implemented inside the existing Eclipse
`astrea-shell` architecture. There is no standalone Bar process, no new shell
daemon, no fake workspace source, and no M8-B system service implementation.

## Root causes addressed

1. The lifecycle test exercised `BarOutputRegistry`, while production owned
   bundles directly in `BarSurfaceManager`. This allowed the real output path
   to regress without failing tests.
2. Status and popup geometry was duplicated between C++ tests and QML. Status
   padding was counted twice, and the clock anchor ignored the 6 px right
   Layer Shell margin.
3. The status surface acted as one large Clock button, preventing indicator-
   local popup ownership.
4. Popup close cleared state and unmapped the overlay before QML exit animation
   could run.
5. Spotlight disablement closed the UI but did not persist an authority state,
   so later `show()` calls could reopen it.
6. `BarController.enabled` had no connection to the actual output surfaces.
7. Shell Bar colors were hardcoded separately from Settings theme state.
8. QML smoke tests loaded local source files instead of the production QRC
   paths, including a local logo override that could hide a broken alias.

## Architecture changes

### Output lifecycle and ownership

`BarSurfaceManager` now owns the production lifecycle with one
`BarSurfaceBundle` per live `QScreen`. A small production-used bundle factory
seam makes creation/configuration failure deterministic in unit tests. Add is
idempotent, remove destroys exactly one bundle, re-add creates a fresh bundle,
geometry notifications update the matching bundle, shutdown is idempotent, and
screen callbacks use `QPointer`-safe tracking. Each bundle owns its own
`BarPopupController`; removal clears only that output's popup state.

The obsolete `BarOutputRegistry` test-only path was removed. Surface creation,
mapping, enablement, and destruction are exercised through the manager and
bundle types used by `astrea-shell`.

### Geometry and interaction

`BarLayoutMetrics` delegates to the existing `BarSurfacePolicy` constants and
arithmetic. It is injected into every production surface and consumed by the
surface bundle for window dimensions. It centralizes status width/left/anchor,
launcher anchor, and popup width/X clamping, including narrow-output safety.

`StatusSurface` is now a passive layout container; `Clock` owns its MouseArea
and reports the indicator-local output-space anchor. Clock uses the reference
horizontal date/separator/time structure, native `BarClockService` text, and
minute-boundary fade transition.

### Enablement and popup lifecycle

`SpotlightController` has an observable stored `componentEnabled` state. All
implicit open paths respect it, disabling closes an open Spotlight, and
`BarController` capability notifications follow the state.

`BarSurfaceBundle::setBarEnabled()` hides or remaps all four surface types and
clears local popup state when disabled. Popup intent and rendered state are
separate: close enters `closing`, keeps `surfaceRequired` true, runs the QML
exit animation, and only `completeClose()` allows overlay unmapping. Output
removal and shutdown clear immediately without waiting for animation.

### Shared theme and resources

`ThemeController` moved from Settings into `shared/theme`. Settings keeps the
same public state and persistence behavior, while Shell owns its own shared
instance and exposes it to Shell QML. A debounced `QFileSystemWatcher` watches
the file and its nearest existing parent directory, supports creation and
atomic replacement, re-registers paths, and preserves the last good state on
invalid JSON.

`ShellBarTheme.qml` derives Bar-facing Borealis tokens from the shared state for
dark/light, transparent/default/frosted, surfaces, borders, interactions, text,
icons, separator, and accent. `BarResources.cmake` is the shared production and
test QRC source definition; smoke tests use the exact URLs consumed by
`BarSurfaceBundle`, including `Bar/assets/astrea.png`.

## Files changed by area

- `Bar/core/`: popup state machine, shared geometry metrics, Bar capability
  behavior, and surface policy arithmetic.
- `Bar/platform/wayland/`: production bundle and manager lifecycle,
  enablement, QML injection, and output-local popup ownership.
- `Bar/qml/`: production surfaces, horizontal Clock, popup close animation,
  shared theme tokens, and indicator geometry.
- `Bar/tests/` and `Bar/cmake/`: lifecycle/action/geometry/QML tests, legacy
  source guard, and shared QRC resource definition.
- `Shell/CMakeLists.txt`, `Shell/app/`, `Shell/runtime/`, and Shell tests:
  Bar integration and shared ThemeController ownership/context exposure.
- `Spotlight/core/`: authoritative component enablement.
- `shared/theme/`, `shared/CMakeLists.txt`, and Settings references: promoted
  ThemeController implementation without duplicate Settings/Shell copies.
- `docs/NATIVE_TOPBAR_M8A.md` and this report: current architecture,
  boundaries, evidence, and limitations.

## Tests added or corrected

- Production-used manager lifecycle: initial add, duplicate add, geometry
  update, remove, re-add, enable/disable, popup clearing, shutdown, repeated
  shutdown, and bundle initialization failure unwind.
- Per-bundle popup ownership.
- Geometry authority, narrow-output status sizing, status anchor, and popup
  clamp behavior.
- Spotlight missing/disabled/enabled Search capability and re-enable behavior.
- Settings catalog discovery, capability notification, exact virtual launch
  request, and disabled-Bar rejection.
- Production QRC surface loading and production Astrea logo resolution.
- Rendered QML status width, output-space clock anchor, popup clamp and close
  lifecycle, horizontal Clock structure, and live Bar palette changes.
- Shared ThemeController external replacement and invalid-JSON retention.
- Expanded legacy guard across production Bar QML plus Bar core/platform C++.

## Tests that exposed issues before fixes

The original lifecycle coverage was green but did not reach the production
manager, which is the defect this closure removes. During closure, the new
production QML anchor assertion initially failed by one pixel because the test
used the full status-surface center; it was corrected to use the Clock's mapped
indicator position plus the real right margin. A production Shell build also
caught an incomplete shared ThemeController type at the application context
boundary; the missing shared include was added.

## Validation

Using the existing `build/release` CMake/Unix Makefiles configuration:

```text
cmake --build build/release --target astrea-shell bar-core-test bar-qml-smoke-test -j2
  passed

cmake --build build/release --target astrea-settings settings-qml-smoke-test
    settings-component-smoke-test theme-controller-test -j2
  passed

ctest --test-dir build/release -R \
  '^(bar-core-test|bar-qml-smoke-test|bar-qml-legacy-guard|shell-runtime-test|\
theme-controller-test|settings-qml-smoke-test|settings-component-smoke-test)$' \
  --output-on-failure
  7/7 passed

ctest --test-dir build/release --output-on-failure
  55 passed; 2 unrelated Paper tests were Not Run because their executables
  were absent while that uncommitted subproject changed concurrently
```

The targeted tests ran with Qt's offscreen platform where applicable. All
Eclipse/M8-A tests in the full run passed; the only non-passing entries were
unrelated Paper tests whose executables were not available. Paper files and its
root CMake addition are not part of this closure commit. After the Paper
subproject changed again, normal CMake regeneration was also blocked by its
missing `core/WallpaperPersistence.cpp`; the already-generated Makefiles were
used to rebuild the final Bar policy object and relink `astrea-shell` and
`bar-core-test`, followed by direct final Bar/QML/guard execution. The debug
build directory was not used because its cached generator requires Ninja,
which is unavailable in this environment. No ASan/UBSan or live Wayland
compositor validation was claimed or run. The remaining compositor-dependent
checks are documented in `docs/NATIVE_TOPBAR_M8A.md`.

## Remaining limitations

The Bar intentionally does not implement system tray, network, Bluetooth,
audio/media, Control Center, notifications, volume OSD, or real workspace/output
updates. `WorkspaceModel` remains empty until a real compositor-neutral/Typhon
source is agreed. Live Wayland Layer Shell negotiation, hotplug, exclusive-zone
release, and compositor-rendered visual qualification remain release-session
checks rather than offscreen unit-test claims.
