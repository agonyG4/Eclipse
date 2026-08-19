# M8-B.1 Native System Services Correctness Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** Make the existing native Audio, Network, and Bluetooth services authoritative, generation-safe, restartable desktop authorities without changing the M8-B architecture or adding M8-C scope.

**Architecture:** Preserve \`shared/system\` backends as the only native API boundary and make each service consume immutable snapshots/events through generation-checked reducers. Add explicit backend generation, pending-operation, scan, primary-connection, traffic-baseline, and PipeWire reconnect state where needed. Extend injectable fake backends and production-QRC Bar tests so deterministic tests exercise the same service/model reducers used by production.

**Tech Stack:** C++20, Qt 6 Core/DBus/Test/QML, PipeWire 0.3, QDBusPendingCallWatcher, QTimer, QAbstractListModel, CMake.

## Global Constraints

- Preserve \`shared/system/audio\`, \`shared/system/network\`, \`shared/system/bluetooth\`, \`astrea-shared-system\`, one authority per \`ShellRuntime\`, and typed Bar QML services.
- Keep PipeWire, NetworkManager D-Bus, and BlueZ D-Bus native; add no subprocess fallback.
- Do not add System Tray, Control Center, pairing UI, Wi-Fi secret flows, MPRIS, per-application mixing, or M8-C functionality.
- Preserve existing M8-A geometry/palette/popup/multi-output behavior and unrelated dirty Paper/Settings work.
- All asynchronous callbacks must be generation-safe and harmless after stop.
- Use production reducers/state models through injectable backend seams; no test-only production state machines.
- Commit only M8-B.1 files in one coherent commit; do not stage concurrent Paper/Settings files.

---

### Task 1: Establish failing recovery and authority tests

**Files:**
- Modify: \`shared/system/tests/SystemServicesTest.cpp\`
- Modify: \`Bar/tests/BarQmlSmokeTest.cpp\`

**Interfaces:** Extend fake backends with delayed authoritative snapshots, operation failures, daemon generations, and late-event emission. Add named behavior tests for stale events, unavailable/recovery, paired-only Bluetooth connect, authoritative scan state, primary network projection, traffic reset, audio default-switch projection, and Bar indicator states.

- [x] **Step 1: Add the named failing tests before production changes.**
- [x] **Step 2: Build and run the focused tests with \`QT_QPA_PLATFORM=offscreen\`.**
- [x] **Step 3: Confirm failures were behavioral, not compilation/test typos, and record the red baseline.**

### Task 2: Correct BlueZ graph typing, event reconciliation, and generation recovery

**Files:**
- Modify: \`shared/system/bluetooth/BluetoothBackend.hpp\`
- Modify: \`shared/system/bluetooth/BluezBackend.hpp\`
- Modify: \`shared/system/bluetooth/BluezBackend.cpp\`
- Modify: \`shared/system/bluetooth/BluetoothService.hpp\`
- Modify: \`shared/system/bluetooth/BluetoothService.cpp\`
- Modify: \`shared/system/tests/SystemServicesTest.cpp\`

**Interfaces:** Use nested D-Bus aliases for managed objects, subscribe to Adapter1/Device1/Battery1 \`PropertiesChanged\`, add generation-aware reconciliation, bounded pending operation settlement, and paired/current-generation validation before Connect.

- [x] **Step 1: Make GetManagedObjects and InterfacesAdded use nested graph types and test multi-interface objects.**
- [x] **Step 2: Increment BlueZ generation on owner loss/reappearance; clear old paths, actual discovery state, and pending operations.**
- [x] **Step 3: Merge PropertiesChanged and invalidated keys into the relevant object snapshot without dropping unrelated state.**
- [x] **Step 4: Settle power/discovery/connect/disconnect on authoritative state, D-Bus error, timeout, object removal, daemon loss, or stop.**
- [x] **Step 5: Keep scan owner demand separate from authoritative Discovering and bounded recovery reconciliation.**
- [x] **Step 6: Run BlueZ-focused tests and the focused matrix.**

### Task 3: Make NetworkManager primary connection and scan/traffic state authoritative

**Files:**
- Modify: \`shared/system/network/NetworkBackend.hpp\`
- Modify: \`shared/system/network/NetworkManagerBackend.hpp\`
- Modify: \`shared/system/network/NetworkManagerBackend.cpp\`
- Modify: \`shared/system/network/NetworkService.hpp\`
- Modify: \`shared/system/network/NetworkService.cpp\`
- Modify: \`shared/system/network/WifiNetworkModel.cpp\` if row reconciliation requires it
- Modify: \`shared/system/tests/SystemServicesTest.cpp\`

**Interfaces:** Represent manager generation, primary active-connection path, selected transport/device paths, Wi-Fi availability/enabled/scanning, and scan state. Reconcile PrimaryConnection → ActiveConnection → Devices coherently, use request epochs, and reset traffic baselines on interface/device/daemon/counter changes.

- [x] **Step 1: Add failing tests for Wi-Fi availability and authoritative scanning.**
- [x] **Step 2: Stop per-device callbacks from publishing the global connection authority; add generation checks.**
- [x] **Step 3: Resolve one primary active connection and deterministic physical transport/device fallback.**
- [x] **Step 4: Add correct DeviceAdded/DeviceRemoved and property reconciliation with stale-path invalidation.**
- [x] **Step 5: Implement explicit scan request/cooldown timers and cancellation on stop/removal/restart.**
- [x] **Step 6: Guard unsigned traffic subtraction and atomically reset the selected-interface baseline.**

### Task 4: Add PipeWire reconnect, per-output state, and default-sink reacquisition

**Files:**
- Modify: \`shared/system/audio/AudioBackend.hpp\`
- Modify: \`shared/system/audio/AudioService.hpp\`
- Modify: \`shared/system/audio/AudioService.cpp\`
- Modify: \`shared/system/audio/PipeWireAudioBackend.hpp\`
- Modify: \`shared/system/audio/PipeWireAudioBackend.cpp\`
- Modify: \`shared/system/tests/SystemServicesTest.cpp\`

**Interfaces:** Add explicit PipeWire generation/reconnect state with bounded backoff and cancellable timers; clear old proxies/listeners/registry IDs; cache per-output volume/mute/channel state; reacquire the new default sink’s properties; preserve cubic 0–150% semantics.

- [x] **Step 1: Add failing reconnect and runtime-failure recovery tests.**
- [x] **Step 2: Implement one-at-a-time bounded reconnect with stop cancellation and generation filtering.**
- [x] **Step 3: Cache per-node state and explicitly request/reacquire the new default node’s properties.**
- [x] **Step 4: Harden volume/mute/channel-volume parsing and authoritative operation error reconciliation.**
- [x] **Step 5: Run live PipeWire lifecycle qualification through ShellRuntime and native host checks.**

### Task 5: Expand Bar projections, diagnostics, docs, validation, and commit

**Files:**
- Modify: \`Bar/tests/BarQmlSmokeTest.cpp\`
- Modify: \`Bar/qml/components/NetworkIndicator.qml\`
- Modify: \`Bar/qml/components/BluetoothIndicator.qml\` if needed
- Modify: \`Bar/qml/components/VolumeIndicator.qml\` if needed
- Modify: \`Shell/app/AstreaShellApplication.cpp\` only if diagnostics omit reconciled fields
- Modify: \`Bar/tests/check_bar_qml.py\` only if guard scope needs expansion
- Modify: \`docs/M8-B_IMPLEMENTATION_REPORT.md\`
- Create: this plan document

**Interfaces:** QML consumes only reconciled Q_PROPERTY state; diagnostics remain read-only and secret-free; the report records root causes, red/green results, sanitizer/live qualification, and explicit limitations.

- [x] **Step 1: Add production-QRC tests for meaningful Network/Bluetooth/Audio indicator states and wheel actions.**
- [x] **Step 2: Run supported Unix Makefiles, no-Typhon/no-layer-shell, sanitizer, and full deterministic configurations.**
- [x] **Step 3: Perform safe read-only live qualification against PipeWire, NetworkManager, and BlueZ.**
- [x] **Step 4: Run legacy scan, diff check, and final acceptance-criteria review.**
- [ ] **Step 5: Stage only M8-B.1 files and create one commit: \`fix(system): close native service recovery semantics\`.**

---
