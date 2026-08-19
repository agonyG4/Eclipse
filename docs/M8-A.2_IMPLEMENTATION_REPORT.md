# M8-A.2 TopBar visual and lifecycle closure report

## Outcome

M8-A.2 closes the remaining native TopBar fidelity and lifecycle gaps inside
the existing Eclipse `astrea-shell` architecture. The change preserves one
`BarSurfaceBundle` and one `BarPopupController` per `QScreen`, keeps
`BarLayoutMetrics` as the production geometry authority, and does not add any
M8-B service or legacy bridge.

## Root causes

1. `Bar/qml/components/ShellBarTheme.qml` had approximate colors and collapsed
   transparent, default, and frosted states instead of mirroring the
   repository's Borealis Shell formulas.
2. `BarSegment` used a surface fill and a transparent normal border, so its
   idle, hover, pressed, and active states did not follow the reference
   background/border and interaction semantics.
3. `PopupOverlaySurface.qml` implemented exit animations but forced a selected
   card directly to its final opacity and scale on open. That removed the
   intended popover entrance and left close/reopen behavior dependent on stale
   animation completion.
4. `BarSurfaceManager::shutdown()` reset `m_initialized` but left application
   and controller callbacks connected. A later screen event could therefore
   recreate or mutate bundles after shutdown.
5. Clock typography and transition timing were locally specified rather than
   using the shared Borealis font and animation tokens.

## Architecture and visual corrections

The six `(themeMode, shellStyle)` combinations now use the exact formulas
from `Settings/qml/theme/Shell.qml` for background, surface, border, hover,
pressed, active, text, icon, and separator semantics. The Bar-facing theme
also exposes the exact `barBorderHover` value and the elevated popup formula
from `Settings/qml/theme/Apps.qml`.

`BarSegment` now has a visible normal shell border, shell background at rest,
the shell hover fill plus `barBorderHover` border on hover, and the exact
pressed/active fills. Its interaction color and border changes retain short
transitions. Popup cards use the elevated popup surface rather than the shell
background. Clock remains the native horizontal date/separator/time layout,
with Inter as the requested font family, normal/medium weights for date/time,
8 px row spacing, and shared quick-transition timing. Qt's normal font
fallback behavior remains available when Inter is not installed.

## Popup lifecycle

Astrea and Clock each have independent enter and exit `ParallelAnimation`
tracks. Opening initializes the selected card to opacity `0` and scale `0.97`,
stops both exits, and starts only the selected enter track. Closing stops
enters and starts the current-kind exit while the overlay remains required.
Exit completion still calls `completeClose()` only when the controller is both
closing and still on that popup kind. This makes same-kind close/reopen and
Clock-close/Astrea-open races leave the newly opened popup mapped. Output
removal and manager shutdown still clear output-local popup state
immediately, without waiting for animation.

## Terminal shutdown

`BarSurfaceManager` now has an explicit terminal `m_stopped` state. Shutdown
sets it before teardown, disconnects the `QGuiApplication` screen signals and
BarController enablement signal, disconnects screen geometry callbacks, and
then deletes bundles and emits the existing topology notifications once.
Repeated shutdown is harmless. `initialize()` returns `false` with a useful
error after shutdown, and every add/remove/geometry/enablement path rejects
post-shutdown mutation.

## Tests

The production QML smoke tests now cover:

- exact background, surface, border, hover, primary/secondary text, and
  separator values for all six mode/style combinations;
- BarSegment normal, hover, pressed, active, and normal-border semantics;
- popup enter state and completion, close/reopen, and Clock-to-Astrea kind
  switching;
- native horizontal Clock spacing, Inter family, and normal/medium weights.

The production manager factory-seam tests now cover popup output removal while
closing and terminal shutdown, including post-shutdown screen, geometry,
enablement, reinitialize, and repeated-shutdown events.

## Validation

The final validation used the existing release build directory and the Qt
offscreen platform for deterministic QML tests. The exact command results are
recorded here after the final run:

```text
cmake --build build/release --target astrea-shell bar-core-test bar-qml-smoke-test -j2
  passed

QT_QPA_PLATFORM=offscreen build/release/Shell/bar-core-test
  22 passed; 0 failed

QT_QPA_PLATFORM=offscreen build/release/Shell/bar-qml-smoke-test
  12 passed; 0 failed

ctest --test-dir build/release -R \
  '^(bar-core-test|bar-qml-smoke-test|bar-qml-legacy-guard|shell-runtime-test|\
theme-controller-test|settings-qml-smoke-test|settings-component-smoke-test|spotlight-tests)$' \
  --output-on-failure
  8/8 passed

ctest --test-dir build/release --output-on-failure
  55 passed; 5 Not Run (missing concurrent Paper/wallpaper executables)
  ctest exits 8 because those five executables are not present

git diff --check
  passed
```

The repository's uncommitted Paper and Settings work remained outside this
M8-A.2 change. The five unavailable CTest executables are four Paper tests
(`paper-persistence-test`, `paper-service-test`,
`paper-surface-manager-test`, and `paper-control-server-test`) plus
`settings-wallpaper-controller-test`; they are not used as evidence for this
TopBar closure. No live Wayland compositor, hotplug protocol, exclusive-zone
release, ASan/UBSan, or installed-font visual qualification was claimed.

## Scope limitations

System tray, network, Bluetooth, audio/media, Control Center, notifications,
volume OSD, and real workspace/output data remain intentionally outside M8-A.
The native implementation still requires a live Wayland release-session check
for compositor-specific rendering and Layer Shell negotiation.
