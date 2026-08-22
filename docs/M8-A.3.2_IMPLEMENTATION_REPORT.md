# M8-A.3.2 — Final TopBar Visual Fidelity and Interaction Safety Closure

Date: 2026-08-21

## Scope and baseline

The implementation baseline was the committed M8-A.3.1 TopBar parity closure (`389e13c`). The bundled AstreaOS Quickshell QML remained the visual and behavioral reference. This closure was limited to the ten requested areas: Bluetooth indicator centering, volume thumb motion, network semantic projection, MenuItem geometry, Bluetooth popup scanning and device grouping, unsupported popup safety, workspace stable width, launcher reserved width, and production-QRC regression coverage.

The native Qt/QML, LayerShell, `BarPopupController`, `BarLayoutMetrics`, `WorkspaceModel`, `ThemeController`, and existing Audio/Network/Bluetooth service ownership were preserved. No Typhon source, workspace backend, system-service source, Dock source, System Tray, Control Center, Notification History backend, or new shell architecture was added.

## Pre-implementation mismatches

* The Bluetooth glyph was inside a fixed visual slot but did not have an explicit centering anchor.
* The volume thumb had direct X binding but lacked the reference programmatic X animation and drag-time opt-out.
* Network popup title/icon derivation could mix disconnected and connected transport semantics.
* MenuItem applied a Row right margin in addition to the reference text-width subtraction.
* Bluetooth scanning changed its border but lacked the reference active background wash, separator, and paired/available visual grouping.
* `PopupKind::Clock` remained representable even though no valid popup component was registered.
* Workspace and launcher width calculations could follow transient animated/model geometry instead of reserved reference geometry.

## Implementation

### Bluetooth indicator

`BluetoothIndicator.qml` now keeps the scan ring and glyph in the same explicit 16×16 visual container, with both centered using `anchors.centerIn`. Native powered-off, powered-on, connected-accent, unavailable, and scan-ring behavior remains intact.

### Volume slider

`VolumePopup.qml` now has the reference-equivalent `Behavior on x`: its OutCubic slider-duration animation is enabled for external/programmatic value changes and disabled while `sliderMouse.pressed`. The existing thumb-size animation, wheel path, drag path, and native 0–150% policy remain unchanged.

### Network popup projection

`NetworkPopup.qml` now derives one explicit semantic state: `Unavailable`, `Disconnected`, `Wifi`, `Wired`, or `Other`. Header title, icon, and state label are all derived from that state, so Wi-Fi availability alone cannot produce a connected Wi-Fi header and `Other` cannot be presented as Ethernet.

### MenuItem geometry

`MenuItem.qml` now matches the reference Row geometry: a single literal 12-pixel left inset and the reference text width calculation, with no duplicate Row right margin. Icon/no-icon, long-text elision, disabled opacity, and hit-target behavior remain covered.

### Bluetooth popup

`BluetoothPopup.qml` now restores the reference scanning wash (`Qt.rgba(0.20, 0.60, 1.0, 0.10)`) and active accent border, adds the one-pixel device/search separator, and renders paired and unpaired model rows in separate `Paired devices` and `Available devices` sections. Paired rows retain native connect/disconnect calls. Unpaired rows expose metadata but have no fake pairing action and their interaction is disabled. Backend availability, adapter state, power state, scan lifecycle, battery, and RSSI remain native-service driven.

### Popup route safety

The invalid Clock popup kind was removed while preserving the explicit numeric values of the supported QML routes. `BarPopupController` rejects unsupported kinds before mutating state. `PopupOverlaySurface` enables its click shield only when the controller requests a surface and a concrete popup component exists. Astrea, Network, Bluetooth, and Volume routes remain functional.

### Workspace and launcher geometry

`WorkspaceStrip.qml` now exposes a reference-style stable width based on the active width plus the inactive dot/spacing contribution, with valid zero geometry for an empty model and reserved ten-slot width for surface sizing. `LauncherSurface.qml` uses the maximum of its visual width and the logo-plus-padding-plus-reserved-workspace width, keeping the LayerShell surface stable while workspace content animates.

## Tests added

Eight logical feature test functions were added:

* `bluetoothIndicatorCentersGlyphInVisualContainer`
* `volumeSliderAnimatesProgrammaticChangesOnly`
* `networkPopupProjectsOneCoherentConnectionState`
* `menuItemMatchesReferenceRightPadding`
* `bluetoothPopupOrganizesPairedAndAvailableDevices`
* `popupOverlayRejectsUnsupportedKinds`
* `workspaceAndLauncherReserveStableWidth`
* `popupRejectsUnsupportedKinds` in `BarCoreTest`

These instantiate the production QML through QRC and cover geometry/state rather than source text alone. The Bluetooth popup test covers no devices, paired disconnected/connected devices, available devices, mixed sections, scanning off/on, separator visibility, active wash/border, and absence of pairing UI. Theme changes and native unavailable-state coverage remain in the existing Bar tests.

## Build and test matrix

Successful non-sanitized focused matrices:

* Baseline build: Bar core, Bar QML smoke, legacy guard, Shell runtime, Shell IPC, and shortcut dispatcher — **6/6 passed**.
* Debug, Release, Clang, no-Typhon, and no-layer-shell configurations: Bar core, Bar QML smoke, and legacy guard — **3/3 passed in each configuration**.
* `astrea-shell_qmllint` built successfully in the baseline and each available configuration. Existing unrelated Dock/AltTab/Spotlight warnings remain; no new Bar warning was observed.

Broader selected Debug regression suite:

* shared system services, Dock application-state projector, Spotlight tests, ten AltTab tests, and ThemeController — **14/14 passed**.

Sanitizer results:

* UBSan focused Bar matrix — **3/3 passed**.
* ASan BarCore (24 QTest cases) and BarQmlSmoke (26 QTest cases) completed with all test cases passing internally; the default CTest run returned failure only because LeakSanitizer reported 183 bytes allocated by `/usr/lib/libnvidia-glcore.so.610.57.04`. Re-running the same ASan CTest matrix with `ASAN_OPTIONS=detect_leaks=0` — **3/3 passed**. This is recorded as an external graphics-driver leak, not suppressed as an application failure.

QTest totals are reported separately from logical feature counts: the final QML smoke binary reports 26 passed assertions including `initTestCase` and `cleanupTestCase` (24 feature scenarios), and BarCore reports 24 including its lifecycle hooks (22 feature scenarios).

## Manual visual qualification

Not executed. No side-by-side runtime capture harness for the bundled reference and live Eclipse LayerShell surfaces was available in this environment. The production-QRC tests provide deterministic geometry, state, and interaction qualification, but no pixel-perfect visual parity claim is made. The recommended 1920×1080, scale-1.0 dark Borealis comparison followed by light-mode comparison remains outstanding.

## Not run and why

The unfiltered CTest registry contains optional targets that are registered but not built in the active trees; running it wholesale would report missing executables rather than meaningful results. The required Bar, Shell, system, Theme, Dock, Spotlight, AltTab, legacy-guard, configuration, and sanitizer suites were run explicitly. No manual visual session was run because the reference/live comparison harness was unavailable.

## Remaining intentional differences before M8-C

The following remain explicitly out of scope and are not faked by this closure:

* System Tray
* Control Center
* Notification History
* real Typhon workspace activation and the `ext-workspace-v1` runtime
* Bluetooth pairing / Agent1 flow
* Wi-Fi password / SecretAgent flow
* VPN UI, MPRIS, notification daemon, and other new backend capabilities

The existing native unavailable states remain authoritative, and paired Bluetooth Connect/Disconnect behavior remains native-service based.
