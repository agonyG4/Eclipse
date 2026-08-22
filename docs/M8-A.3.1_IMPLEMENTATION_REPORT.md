# M8-A.3.1 — Reference-Exact TopBar Visual and Behavioral Parity Closure

Date: 2026-08-21

## Scope

This milestone closes presentation and interaction parity for the currently implemented Eclipse TopBar components. The native Qt/QML, LayerShell, `BarPopupController`, `BarLayoutMetrics`, `ThemeController`, `BarClockService`, `AudioService`, `NetworkService`, and `BluetoothService` architecture was preserved. No Typhon source, system-service source, Dock source, Tray, Control Center, or Notification History backend was changed.

The bundled reference components compared directly were:

* `Bar.qml`, `Theme.qml`, and `BarContent.qml`
* `BarSegment.qml`, `IndicatorButton.qml`, `TopbarIndicator.qml`, `Clock.qml`, and `Workspaces.qml`
* `TopbarPopup.qml`, `PopupHost.qml`, and `PopupHeader.qml`
* `AstreaPopup.qml`, `MenuItem.qml`, and `MenuSeparator.qml`
* `NetworkIndicator.qml` and `NetworkPopup.qml`
* `BluetoothIndicator.qml` and `BluetoothPopup.qml`
* `VolumeIndicator.qml` and `VolumePopup.qml`

## Preflight audit

| Reference | Eclipse | Difference | Classification | Required action |
| --- | --- | --- | --- | --- |
| `BarSegment` keeps `Theme.background` on hover | Outer segment changed its fill to hover/pressed colors | Whole-pill hover feedback was visually wrong | `REFERENCE_REQUIRED` | Keep the fill fixed and animate only the border token |
| Astrea indicator is 28×28 with an 18×18, 0.80-opacity logo | Indicator was 28×34 | Launcher hit target was too tall | `BUG` | Restore 28×28 geometry and exact logo metrics |
| Reference workspaces are active/inactive dots with expanded hit areas | Eclipse used occupied/urgent to change the dot appearance and routed clicks to the workspace controller | Invented visual states and a backend action existed on the presentation path | `CAPABILITY_UNAVAILABLE` | Render only active/inactive; keep the future signal contract but force activation unavailable |
| Reference Clock is a non-popup visual with native time/date data | Eclipse routed Clock to `ClockPopup` | Invented Notification History substitute | `REFERENCE_REQUIRED` | Remove the Clock popup route; keep the native clock service |
| Reference popup enters with 180 ms fade and 220 ms OutBack scale | Eclipse used approximately 300 ms OutCubic for both | Timing/easing mismatch | `BUG` | Use reference durations/easings and preserve native closing lifecycle |
| Reference popup card uses `Theme.background` and `Theme.border` | Eclipse used an elevated surface alias | Card visual was too bright/elevated | `REFERENCE_REQUIRED` | Project the card to the reference background/border |
| Reference scan ring uses a scale animation | Eclipse’s ring presentation was not fully reference-qualified | Ring behavior needed explicit geometry/timing coverage | `REFERENCE_REQUIRED` | Preserve the native scan state and restore reference ring dimensions/tokens |
| Reference status indicator glyphs are icon-only | Eclipse already used native service state but needed availability guards | Native unavailable state needed truthful mapping | `NATIVE_ADAPTER_REQUIRED` | Keep icon-only presentation and suppress stale connected/type state |
| Reference menu has the Search / separator / About / Settings / separator / Force Quit / Lockscreen / Power structure | Eclipse had the structure but a separator/text-width mismatch | Small geometry differences | `BUG` | Restore separator width and row text inset; keep unavailable native actions disabled |

## Parity result

| Component | Reference parity | Native adaptation | Remaining limitation |
| --- | --- | --- | --- |
| BarSegment | Exact fill, border, radius, padding, height, reveal behavior | `ShellBarTheme` projection | None for implemented surfaces |
| Astrea button | Exact 28×28 control and 18×18 logo treatment | `BarPopupController` | None |
| Workspaces | Visual geometry, spacing, colors, transitions, and expanded hit areas match | `WorkspaceModel` | Activation is explicitly deferred; `activationAvailable` is false and no fake action is emitted |
| Clock | Native date/time presented as reference `DATE \| TIME`, including padding, fonts, tracking, separator, and transitions | `BarClockService` | Notification History is unavailable; Clock has no invented popup action |
| Network | Icon-only disconnected, Wi-Fi, wired, and restrained unavailable states match | `NetworkService` | None for currently exposed native data |
| Bluetooth | Powered-off/on glyphs, connected accent, and scan ring match; native device metadata remains available in the popup | `BluetoothService` | Pairing/Agent1 is not implemented; unsupported pairing UI is omitted |
| Volume | Reference muted/zero, low, medium, and high glyphs and slider presentation restored | `AudioService` | Native 0–150% range is intentionally preserved rather than regressed to the old 0–100% wheel policy |
| Astrea Menu | Structure, item height, separators, padding, icon treatment, and hover behavior match | Native `BarController` actions | Search and Settings use native actions; About, Force Quit, Lockscreen, and Power remain visible but disabled |
| NetworkPopup | Reference width, header, traffic rows, typography, spacing, and colors | `NetworkService` | Only data exposed by the native service is shown |
| BluetoothPopup | Reference header/power/scan/device presentation with native connected, battery, and RSSI metadata where available | `BluetoothService` | Pairing and agent flows remain out of scope |
| VolumePopup | Reference header, mute control, track, gradient, thumb behavior, spacing, and geometry | `AudioService` | Default-sink safety remains authoritative |
| System Tray | Not implemented | Future M8-C | Intentionally not faked |
| Control Center | Not implemented | Future milestone | Intentionally not faked |
| Notification History | Not implemented | Future milestone | Intentionally not faked |

## Implementation details

* `BarSegment` now keeps its background fixed and changes only the reference border hover state.
* The launcher uses the reference spacing and 28×28 Astrea indicator geometry. Only that indicator owns the Astrea menu action.
* Workspaces no longer use `occupied` or `urgent` to invent presentation states. The expanded hit geometry remains in place, but the launcher passes `activationAvailable: false` and contains no workspace activation call.
* Clock remains backed by `BarClockService`, is centered as a 34-pixel indicator inside its 36-pixel visual row, and no longer routes to `ClockPopup`.
* Indicator active state follows the native popup controller’s `surfaceRequired` lifetime, including the visible closing transition.
* Popup card defaults now use the reference background and border. Entry is opacity 0→1 over 180 ms with OutCubic and scale 0.97→1 over 220 ms with OutBack. Exit uses 180 ms OutCubic opacity and 220 ms OutCubic scale while retaining the native mapped surface until completion.
* Native availability guards prevent Network, Bluetooth, and Audio indicators from presenting stale connected/default state.

## Tests and verification

Production-QRC tests were expanded in `Bar/tests/BarQmlSmokeTest.cpp` to cover exact geometry and behavior: segment fill/border, launcher hit ownership, workspace spacing/width/colors/animation metadata/unavailable activation, indicator glyphs and scan-ring metrics, clock padding/fonts/action absence, popup card tokens/geometry/animation durations, popup active state, actual indicator popup activation, outside-close lifecycle, and native audio safety. Existing `BarCoreTest` lifecycle and geometry coverage remains green.

Executed successfully:

* `cmake --build build --target astrea-shell`
* `cmake --build build --target astrea-shell_qmllint` (existing unrelated QML warnings remain in Dock/AltTab/Spotlight)
* `ctest --test-dir build -R '^(bar-core-test|bar-qml-smoke-test|bar-qml-legacy-guard)$' --output-on-failure`
* The same focused Bar matrix in `build/no-typhon`
* The same focused Bar matrix in `build/asan` with ASan enabled
* The same focused Bar matrix in `build/ubsan` with UBSan enabled
* Shell runtime, Shell IPC, and shortcut-dispatcher tests

The focused Debug, no-Typhon, ASan, and UBSan Bar matrices all passed. The broader selected run also observed `spotlight-tests` and `dock-controller-test` segfaults in untouched Spotlight/DesktopEntryCatalog and Dock controller paths; these are recorded rather than masked and are not caused by files changed for this milestone.

## Manual visual qualification

Not executed. The available environment did not provide a side-by-side capture harness for the bundled Quickshell reference and the live Eclipse LayerShell surfaces. The production-QRC offscreen tests provide deterministic property and interaction qualification, but no pixel-parity claim is made without the requested 1920×1080, scale-1.0 visual comparison.

## Remaining gaps before complete TopBar parity

The currently implemented native components are reference-aligned. Full TopBar capability parity still requires native System Tray, Control Center, Notification History, and a completed Typhon workspace activation interface. Bluetooth pairing/Agent1 and unsupported reference subprocess actions also remain intentionally outside this milestone.
