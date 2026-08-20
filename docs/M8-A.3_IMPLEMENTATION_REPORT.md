# M8-A.3 Astrea TopBar Parity Implementation Report

## Reference components compared

The Eclipse port was compared against the AstreaOS TopBar implementation in
`Bench/reference/astreaos/src/Quickshell/bar`, including:

- `Bar.qml`, `BarContent.qml`, `BarSegment.qml`, `IndicatorButton.qml`, and `TopbarIndicator.qml`;
- the workspace strip and launcher composition;
- the Network, Bluetooth, Volume, and Clock indicators;
- the Topbar popup/card primitives and the Astrea menu item/separator structure;
- the Astrea theme and Borealis geometry/animation tokens.

The native Eclipse implementation remains authoritative for Layer Shell surfaces,
per-output ownership, lifecycle, native services, and controller/model state.

## Components restored

- Added the reference-shaped `IndicatorButton`, `TopbarIndicator`, `PopupHeader`,
  `MenuItem`, and `MenuSeparator` QML primitives.
- Restored dedicated interactive targets for the Astrea launcher icon and every
  status indicator while keeping the outer pills passive.
- Restored per-workspace delegates, reference sizing/spacing, hover hitboxes,
  active/occupied/urgent states, and activation through the existing
  `WorkspaceModel` data.
- Restored Astrea Nerd Font glyphs and state presentation for network, Bluetooth,
  and volume indicators, including Bluetooth scan-ring motion and volume wheel
  routing.
- Restored the native-clock `DATE | TIME` presentation with reference typography,
  separator geometry, and text transitions.
- Added native-service-backed Network, Bluetooth, and Volume popup surfaces and
  registered them in the production QML resource manifest.
- Restored Astrea menu icons, separators, grouping, hover/disabled states, and
  capability-gated actions.
- Added semantic Borealis theme aliases while retaining the existing shared
  `ThemeController` authority and `BarLayoutMetrics` geometry authority.

## Behavior differences fixed

- The complete status or launcher pill no longer captures clicks intended for a
  single indicator or the Astrea icon.
- Popup intent now distinguishes Astrea, Clock, Network, Bluetooth, and Volume
  kinds through `BarPopupController` wrappers.
- Popup enter/exit transitions share the reference opacity and scale behavior and
  preserve the native closing lifecycle until the current popup completes.
- Workspace activation is emitted from real native model-backed delegates rather
  than synthetic production data.
- Network no longer permanently renders a connection-name label in the bar.
- Bluetooth powered, unavailable, connected, pending, and scanning states use the
  native service properties and device model.
- Audio mute, threshold glyphs, slider interaction, and wheel deltas route to
  `AudioService`.

## Architecture preserved

No Quickshell runtime, subprocess bridge, status daemon, compatibility daemon,
second shell, or external service polling was added. The port keeps
`BarSurfaceManager`, `BarSurfaceBundle`, Layer Shell ownership, native popup
lifecycle, `ThemeController`, `WorkspaceModel`, `BarController`, and the native
Audio/Network/Bluetooth services intact.

## Tests added

- `BarCoreTest::popupSupportsNativeIndicatorKinds()` covers the three new native
  popup kinds and close-retains-surface lifecycle.
- `BarQmlSmokeTest` covers passive outer pills, dedicated hit targets, reference
  glyph states, workspace hitboxes, clock typography/layout, popup visibility,
  popup animation replacement, and native service inputs.
- The existing Python legacy guard remains in the focused validation set.

## Validation performed

- Release build: `cmake --build build/release -j2` — passed.
- Debug build: repaired the stale generator metadata in the existing
  `build/debug` directory and built with `ASTREA_ENABLE_LAYER_SHELL=OFF` because
  LayerShellQt is unavailable in this environment — passed.
- Release focused CTest (`bar-core-test`, `bar-qml-smoke-test`,
  `bar-qml-legacy-guard`) — 3/3 passed.
- Debug focused CTest — 3/3 passed.
- Full Release CTest — 60/62 passed. The two failures are outside M8-A.3:
  `settings-navigation-model-test` still contains the prior 12-entry expectation
  while the dirty Settings worktree exposes 13 entries, and
  `shell-unified-runtime-integration-test` fails at runtime with
  `QLocalServer::listen: Name error`.
- Full Debug fallback CTest — the same 60/62 result and the same two failures.
- `git diff --check` — passed.

## Remaining differences

The live compositor/manual visual comparison requested by the brief was not
available in this environment. Therefore this report does not claim pixel parity;
exact raster alignment, font availability, and animation appearance still need
visual confirmation on a running Astrea/Eclipse Wayland session. The automated
checks cover the reference structure, geometry authority, states, hit targets,
popup lifecycle, and native service wiring.
