# M6.1 Native Shell Integration Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect Eclipse Alt+Tab and Dock to Typhon's committed native protocols, start one AltTab daemon through the existing session service pattern, expose authoritative Dock runtime state, and suppress duplicate launches without implementing M7 window actions.

**Architecture:** Eclipse will copy Typhon's committed Astrea shortcuts XML and generate C client bindings through the existing CMake Wayland-scanner path. A focused `TyphonShortcutClient` will own a dedicated nonblocking Wayland connection and registration lifecycle; `AltTabApplication` will translate its typed events into the existing controller state machine. Dock will own one existing `TyphonToplevelConnection`, retain its latest committed snapshot, project it through the existing matcher/projector, and publish an explicit runtime-authority flag to the model and QML.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/Test, Wayland client C API, CMake, `wayland-scanner`, QtTest, QML, existing user-systemd packaging.

## Global Constraints

- Work directly on the existing Eclipse `main` branch; do not create branches or worktrees.
- Starting Eclipse HEAD is `246e684142958b1e53ec36d58ebd12ec8fc38ac9`.
- Typhon is read-only for this task; its clean authority HEAD is `5fce17d25540ac6fe188da70f7f6644d4f48e5e0`.
- The authoritative Typhon XML is `Typhon/protocols/astrea-shortcuts-v1.xml`, SHA-256 `4c3999e91e088b3ef5cc57245e9fac544929a811b097f8964d05c0268265867e`.
- Do not use `astreactl`, CLI shortcut bridges, Hyprland commands, synthesized key events, or build-tree executable guessing for production integration.
- Do not implement activation, restore, minimize, or close actions; M7 owns those behaviors.
- Keep all Markdown documentation in English.
- Use the existing Eclipse build directory `/home/agony/GitHub/Eclipse/build/build-release` for compilation and test execution.
- Every behavior change gets a failing regression test before production code, followed by focused and full verification.

---

### Task 1: Add the committed Astrea shortcuts protocol to Eclipse

**Files:**
- Create: `shared/platform/typhon/protocols/astrea-shortcuts-v1.xml` (byte-for-byte copy of the committed Typhon XML)
- Modify: `shared/CMakeLists.txt`
- Test: `shared/tests/TyphonShortcutProtocolContractTest.cpp`

**Interfaces:**
- Consumes: the committed XML and existing `ASTREA_WAYLAND_SCANNER` configuration.
- Produces: generated client/server protocol artifacts in the build tree and a CTest contract that proves the Eclipse fixture remains byte-identical to the authority.

- [ ] **Step 1: Write the failing contract test**

  Add a QtTest that reads the Eclipse fixture and compares its SHA-256 with the recorded authority hash. Also assert the fixture contains the exact manager/interface names and all four lifecycle events (`pressed`, `repeated`, `released`, `cancelled`).

- [ ] **Step 2: Run the focused test to verify the expected failure**

  Run:

  ```bash
  cmake --build build/build-release --target typhon-shortcut-protocol-contract-test
  ctest --test-dir build/build-release -R typhon-shortcut-protocol-contract-test --output-on-failure
  ```

  Expected: the target is unavailable because the fixture and test are not yet registered.

- [ ] **Step 3: Copy the exact XML and register generated outputs**

  Add the XML without reformatting it. Extend `astrea-shared-typhon` with a `wayland-scanner` custom command that generates the shortcuts client header/source, alongside the existing toplevel generation. Add the generated client source/header to the shared target and keep generated files out of Git.

- [ ] **Step 4: Register and run the contract test**

  Add the test to the shared CMake test list, configure the existing release build if needed, then run the focused CTest command. Expected: PASS and the generated client artifacts appear only below `build/build-release`.

- [ ] **Step 5: Commit the protocol fixture and build integration**

  ```bash
  git add shared/platform/typhon/protocols/astrea-shortcuts-v1.xml shared/CMakeLists.txt shared/tests/TyphonShortcutProtocolContractTest.cpp
  git commit -m "feat(shared): add committed Typhon shortcuts protocol"
  ```

### Task 2: Implement the generation-safe native shortcut client

**Files:**
- Create: `shared/platform/typhon/TyphonShortcutClient.hpp`
- Create: `shared/platform/typhon/TyphonShortcutClient.cpp`
- Modify: `shared/CMakeLists.txt`
- Test: `shared/tests/TyphonShortcutClientTest.cpp`
- Test support: existing shared fake Wayland infrastructure, extended only as needed for the shortcuts manager

**Interfaces:**
- Consumes: generated `astrea_shortcuts_manager_v1` and `astrea_shortcut_v1` client bindings plus `TyphonWaylandDisplay`.
- Produces: `TyphonShortcutClient::start()`, `stop()`, `isReady()`, `registrationCount()`, typed `shortcutEvent(namespace, name, phase, serial, timestamp)`, `stateChanged`, and bounded diagnostics.

- [ ] **Step 1: Add failing lifecycle tests against a real fake Wayland server**

  Extend the existing fake Wayland server, using generated bindings, to cover manager discovery, exact `astrea-shell` registrations for `alt_tab_next`, `alt_tab_previous`, and `alt_tab_commit`, event delivery, cancellation, disconnect, reconnect, stale-generation rejection, stop cleanup, and replacement without a registration storm.

- [ ] **Step 2: Run the focused test and confirm it fails for the missing client**

  ```bash
  cmake --build build/build-release --target typhon-shortcut-client-test
  ctest --test-dir build/build-release -R typhon-shortcut-client-test --output-on-failure
  ```

  Expected: FAIL because `TyphonShortcutClient` and its generated event path do not exist.

- [ ] **Step 3: Implement one dedicated nonblocking client connection**

  Reuse `TyphonWaylandDisplay` for `QSocketNotifier`, `prepare_read`, `read_events`, `dispatch_pending`, `cancel_read`, `EINTR`, `EAGAIN`, and flush behavior. Bind the manager once per connection generation, create exactly three registrations, guard every callback with the generation and live-proxy checks, destroy all proxies on stop, treat `cancelled` as a bounded diagnostic/state event, and schedule bounded reconnects after disconnect or protocol failure. Missing manager must report `Unsupported` without affecting unrelated AltTab IPC.

- [ ] **Step 4: Run the focused lifecycle tests green**

  ```bash
  cmake --build build/build-release --target typhon-shortcut-client-test
  ctest --test-dir build/build-release -R typhon-shortcut-client-test --output-on-failure
  ```

- [ ] **Step 5: Commit the client slice**

  ```bash
  git add shared/platform/typhon/TyphonShortcutClient.hpp shared/platform/typhon/TyphonShortcutClient.cpp shared/CMakeLists.txt shared/tests/TyphonShortcutClientTest.cpp
  git commit -m "feat(shared): add generation-safe Typhon shortcut client"
  ```

### Task 3: Wire AltTab events and enforce daemon ownership

**Files:**
- Modify: `AltTab/CMakeLists.txt`
- Modify: `AltTab/app/AltTabApplication.hpp`
- Modify: `AltTab/app/AltTabApplication.cpp`
- Modify: `AltTab/platform/ipc/AltTabIpcServer.cpp`
- Modify: `AltTab/tests/AltTabIpcTest.cpp`
- Create or modify: `AltTab/tests/AltTabShortcutIntegrationTest.cpp`
- Verify/update: `AltTab/packaging/systemd/astrea-alt-tabd.service`, `AltTab/docs/RUNTIME_FLOW.md`, `AltTab/docs/TESTING.md`

**Interfaces:**
- Consumes: `TyphonShortcutClient` events and existing `AltTabController` methods.
- Produces: daemon-only registrations, event mapping (`pressed`/`repeated` next and previous to `step`, `pressed` commit to `commit`), deterministic duplicate-daemon failure, and the existing installed systemd service as the sole startup mechanism.

- [ ] **Step 1: Add failing AltTab integration and duplicate-owner tests**

  Use a fake shortcut client/server boundary to prove the real daemon-side mapping without invoking `astrea-alt-tab --next`. Add an IPC test where a second `AltTabIpcServer` cannot replace the first, and a stale socket test where a dead socket is recoverable. Assert the AltTab application no longer removes a socket owned by a live daemon.

- [ ] **Step 2: Run the focused tests red**

  ```bash
  cmake --build build/build-release --target alttab-ipc-test alttab-shortcut-integration-test
  ctest --test-dir build/build-release -R 'alttab-(ipc|shortcut-integration)-test' --output-on-failure
  ```

  Expected: the shortcut integration target is missing and the single-instance regression exposes the current unconditional socket removal.

- [ ] **Step 3: Wire the persistent daemon only**

  Construct `TyphonShortcutClient` only after the daemon path bypasses CLI forwarding, connect it to `AltTabController`, and start/stop it with daemon lifecycle. Handle `alt_tab_next` and `alt_tab_previous` on pressed/repeated phases and `alt_tab_commit` on pressed; ignore unrelated release phases. Do not register shortcuts from one-shot CLI commands. Change IPC listen/recovery so a live owner is never removed and a stale socket is recovered deterministically.

- [ ] **Step 4: Verify systemd ownership and documentation**

  Keep the existing `astrea-alt-tabd.service` pattern (`%h/.local/share/Astrea/bin/astrea-alt-tab --daemon`, `graphical-session.target`, restart policy, Wayland environment), ensure CMake installs it, and document that session enablement starts exactly one daemon. Do not add a second bootstrap manager or development path.

- [ ] **Step 5: Run AltTab tests and commit**

  ```bash
  cmake --build build/build-release --target astrea-alt-tab alttab-ipc-test alttab-shortcut-integration-test
  ctest --test-dir build/build-release -R 'alttab-|typhon-shortcut' --output-on-failure
  git add AltTab shared
  git commit -m "feat(alttab): own Typhon shortcuts from the daemon"
  ```

### Task 4: Connect Dock to authoritative Typhon runtime state

**Files:**
- Modify: `Dock/app/DockApplication.hpp`
- Modify: `Dock/app/DockApplication.cpp`
- Modify: `Dock/core/DockAppInfo.hpp`
- Modify: `Dock/core/DockAppModel.hpp`
- Modify: `Dock/core/DockAppModel.cpp`
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Modify: `Dock/CMakeLists.txt`
- Test: `Dock/tests/DockAppModelTest.cpp`
- Test: `Dock/tests/DockControllerTest.cpp`
- Create/extend: Dock integration-layer test using a fake `TyphonToplevelConnection` source

**Interfaces:**
- Consumes: `TyphonToplevelConnection::stateChanged`, `snapshotChanged`, `DockApplicationStateProjector`, `TyphonAppMatcher`, and `DesktopEntryCatalog`.
- Produces: `runtimeKnown`, `running`, `active`, `windowCount` roles; latest-snapshot reprojection on catalog/pin changes; non-authoritative clearing on `Unsupported`, `Disconnected`, `Degraded`, and `Stopped`.

- [ ] **Step 1: Add failing authority/model tests**

  Cover Ready with Firefox running/active, minimized, multiple windows, closed last window, disconnect clearing `runtimeKnown` and stale running state, catalog refresh reprojecting the retained snapshot, and a newly added pin being projected immediately. Assert metadata refresh preserves runtime and launch state.

- [ ] **Step 2: Run the focused Dock tests red**

  ```bash
  cmake --build build/build-release --target dock-app-model-test dock-controller-test dock-typhon-integration-test
  ctest --test-dir build/build-release -R 'dock-(app-model|controller|typhon-integration)-test' --output-on-failure
  ```

- [ ] **Step 3: Add explicit authority state and preserve it through rebuilds**

  Add `runtimeKnown` to `DockAppInfo`, roles, equality, and `changedRoles`. Keep the latest committed snapshot and a boolean authority state in `DockApplication`/controller. Project only when the connection has a committed Ready snapshot; clear runtime state and authority on terminal/unavailable states. Re-run projection on snapshot, catalog, and pin changes without waiting for another window event. Update `makeItem()` to preserve runtime, launch, and metadata state across catalog/pin rebuilds.

- [ ] **Step 4: Run focused and shared projection tests green**

  ```bash
  cmake --build build/build-release --target astrea-dock dock-app-model-test dock-controller-test dock-application-state-projector-test
  ctest --test-dir build/build-release -R 'dock-|dock-application-state-projector-test' --output-on-failure
  ```

- [ ] **Step 5: Commit the Dock runtime integration slice**

  ```bash
  git add Dock shared
  git commit -m "feat(dock): project authoritative Typhon window state"
  ```

### Task 5: Add the running indicator and M6.1 launch policy

**Files:**
- Modify: `Dock/qml/components/DockAppDelegate.qml`
- Modify: `Dock/core/DockController.cpp`
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockAppModelTest.cpp`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TYPHON_RUNTIME_STATE.md`

**Interfaces:**
- Consumes: model roles `runtimeKnown`, `running`, `active`, and `windowCount`.
- Produces: visually distinct inactive-running and active indicators, and click behavior that launches only when runtime is unavailable or authoritative-not-running.

- [ ] **Step 1: Add failing policy/model tests**

  Assert `runtimeKnown=false` preserves existing launch behavior, `runtimeKnown=true/running=false` calls the launcher once, and `runtimeKnown=true/running=true` does not call it for one or multiple windows while emitting a bounded diagnostic instead of a launch error. Add role-name coverage for all four runtime roles.

- [ ] **Step 2: Run the focused tests red**

  ```bash
  cmake --build build/build-release --target dock-app-model-test dock-controller-test
  ctest --test-dir build/build-release -R 'dock-(app-model|controller)-test' --output-on-failure
  ```

- [ ] **Step 3: Implement minimal visual and launch behavior**

  Add required QML properties and a small indicator below the icon. Use the existing Astrea accent/theme exposure or the Qt palette accent rather than introducing unrelated colors, blur, gradients, or layout changes. In `DockController::launch`, suppress duplicate launches only when runtime is authoritative and running; leave M7 activation as a bounded `qInfo`/`qWarning` diagnostic without setting `launchError`.

- [ ] **Step 4: Run QML/static and controller verification**

  ```bash
  cmake --build build/build-release --target astrea-dock dock-app-model-test dock-controller-test
  ctest --test-dir build/build-release -R 'dock-(app-model|controller)-test' --output-on-failure
  qmllint Dock/qml/Main.qml Dock/qml/components/*.qml
  ```

- [ ] **Step 5: Commit the visual/policy slice**

  ```bash
  git add Dock
  git commit -m "feat(dock): show runtime state and suppress duplicate launches"
  ```

### Task 6: Full build, verification, and live qualification

**Files:**
- Modify: English docs updated by Tasks 3 and 5 only
- Build output: `/home/agony/GitHub/Eclipse/build/build-release` (not committed)

**Interfaces:**
- Consumes: all prior slices and the existing clean Typhon session.
- Produces: verified release artifacts and a completion report separating automated tests from live Typhon/TTY evidence.

- [ ] **Step 1: Reconfigure the existing release build if protocol files changed**

  ```bash
  cmake -S . -B build/build-release -DCMAKE_BUILD_TYPE=Release -DASTREA_ENABLE_TYPHON_BACKEND=ON -DASTREA_BUILD_TESTS=ON
  ```

- [ ] **Step 2: Build through the requested build folder**

  ```bash
  cmake --build build/build-release -j"$(nproc)"
  ```

- [ ] **Step 3: Run the full CTest suite and QML lint**

  ```bash
  ctest --test-dir build/build-release --output-on-failure
  qmllint Dock/qml/Main.qml Dock/qml/components/*.qml AltTab/qml/Main.qml AltTab/qml/components/*.qml
  ```

- [ ] **Step 4: Verify installed-service packaging without changing Typhon**

  ```bash
  cmake --install build/build-release --prefix "$HOME/.local"
  test -x "$HOME/.local/bin/astrea-alt-tab"
  test -f "$HOME/.local/share/systemd/user/astrea-alt-tabd.service"
  grep -F '%h/.local/share/Astrea/bin/astrea-alt-tab --daemon' AltTab/packaging/systemd/astrea-alt-tabd.service
  ```

- [ ] **Step 5: Run live qualification only if the current Typhon session remains available**

  Confirm the daemon is started once by the session mechanism, inspect its status and registration diagnostics, exercise physical Alt+Tab/Alt+Shift+Tab/Alt release, and verify Dock runtime indicators plus duplicate-launch suppression. Do not log out, stop SDDM, restart Hyprland, or claim hardware qualification beyond what was actually run.

- [ ] **Step 6: Review the complete diff and report exact scope**

  ```bash
  git status --short --branch
  git diff HEAD~4..HEAD --stat
  git log --oneline --decorate -8
  ```

  Record the actual starting/ending Eclipse HEADs, unchanged Typhon HEAD, protocol hash, test counts, live-session evidence, and any unverified requirement.
