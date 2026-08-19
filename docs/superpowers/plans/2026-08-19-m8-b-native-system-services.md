# M8-B Native System Services Core — Implementation Plan

## Constraints

- Work in /home/agony/GitHub/Eclipse.
- Preserve the concurrent Paper, Settings, and Bench work in the worktree.
- Keep astrea-shared-core free of PipeWire and D-Bus dependencies.
- Never use command/process wrappers or legacy status JSON in production paths.
- Keep the existing TopBar geometry, popup, palette, and multi-output behavior.
- Do not add system tray, Control Center, OSD, MPRIS, Wi-Fi password, or
  Bluetooth pairing/agent functionality.

## Phase 1 — Contract tests first

1. Add shared/system/tests/SystemServicesTest.cpp with fake backends and
   failing tests for:
   - service state transitions, idempotent start/stop, restart, and error
     recovery;
   - audio cubic conversion and output-model role/order/no-op semantics;
   - Wi-Fi SSID deduplication/order/no-op semantics;
   - Bluetooth reconciliation/order and scan-owner reference counting;
   - network rate formatting and health JSON safety.
2. Extend Shell/tests/ShellRuntimeTest.cpp for exactly one instance of each
   service and restartable runtime lifecycle.
3. Extend Bar/tests/BarQmlSmokeTest.cpp with fake QObject service facades and
   indicator assertions, including volume wheel forwarding.
4. Extend the existing legacy-token guard to scan the new production service
   directory and add a test assertion for native-only source boundaries.
5. Add the CMake test targets and resource entries needed to compile the tests.
6. Run the focused test target and record the expected red failure before
   writing production implementations.

## Phase 2 — Shared system contracts and models

1. Add SystemServiceState and shared health/state helpers.
2. Add injectable backend interfaces and immutable typed snapshots for Audio,
   Network, and Bluetooth.
3. Implement the three Qt services with QML-safe properties, signals,
   models, lifecycle, and action forwarding.
4. Implement model role registration, deterministic sorting, deduplication,
   semantic equality, and stable model notifications.
5. Implement conversion and formatting helpers as pure testable functions.

## Phase 3 — Native backends

1. Implement the managed PipeWire backend using registry, metadata, and node
   listeners; queue copied events to AudioService and tear down listeners
   before destruction.
2. Implement the asynchronous NetworkManager backend using Qt D-Bus service
   watching, ObjectManager/PropertiesChanged reconciliation, async actions,
   scan coalescing, and a service-owned traffic timer.
3. Implement the asynchronous BlueZ backend using ObjectManager and
   PropertiesChanged; select adapters deterministically, reconcile devices,
   and implement owner-based scan control plus async connect/disconnect.
4. Add explicit unavailable/degraded behavior when daemons or native
   capabilities are missing.
5. Re-run system tests and sanitizer builds, fixing failures by root cause.

## Phase 4 — Runtime and TopBar integration

1. Link astrea-shared-system and add one service instance of each type to
   ShellRuntime; start and stop them in deterministic order.
2. Extend application context properties and status JSON health diagnostics.
3. Pass non-owning service pointers through BarSurfaceManager and
   BarSurfaceBundle initial properties.
4. Add NetworkIndicator.qml, BluetoothIndicator.qml, and VolumeIndicator.qml;
   compose them with the existing clock while preserving status-surface
   geometry.
5. Register the QML resources and retain existing QML popup/interaction
   behavior.

## Phase 5 — Build, CI, and documentation

1. Add Qt DBus and PipeWire CMake dependencies plus CI development packages.
2. Run CMake configure/build/CTest for the canonical debug and no-layer-shell
   presets, then the full available matrix.
3. Run the QML smoke suite and legacy guard with QT_QPA_PLATFORM=offscreen.
4. Perform safe, read-only live qualification where daemons are available;
   record unavailable-daemon evidence otherwise.
5. Update the M8-B implementation report and relevant architecture docs with
   contracts, lifecycle, limitations, verification commands, and live results.

## Verification gates

- Focused system test target passes.
- Runtime, bar-core, QML smoke, IPC, and legacy guard tests pass.
- No forbidden legacy executable/status tokens remain in scanned production
  roots.
- Debug, no-layer-shell, and sanitizer builds configure and compile.
- git diff --check passes.
- Only M8-B files are staged; concurrent work remains unstaged.
