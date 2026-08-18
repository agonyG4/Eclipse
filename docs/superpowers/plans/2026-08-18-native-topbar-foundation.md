# Native TopBar Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build M8-A native TopBar ownership, visual shell, popup foundation, native clock, and deterministic model/policy coverage inside `astrea-shell`.

**Architecture:** ShellRuntime owns BarController, BarClockService, and WorkspaceModel. AstreaShellApplication owns BarSurfaceManager, which creates one screen-bound four-window BarSurfaceBundle per QScreen and configures every QQuickWindow through AstreaLayerShellHelper before mapping.

**Tech Stack:** C++20, Qt 6.8 QtCore/Gui/Qml/Quick/Test, LayerShellQt, Qt Quick QML, CMake/CTest.

## Global Constraints

- Reserve exactly 45 logical pixels at the top of every active output.
- Launcher and Status visual pills are 36 pixels high with 5-pixel top margin, left margin 8, right margin 6.
- Popup overlay is an output-local all-edge Overlay surface, unmapped while closed, with default top offset 54.
- Assign QScreen before LayerShellQt wrapper creation and configure Layer Shell before first map.
- Never use QGuiApplication::primaryScreen() for Bar ownership.
- Production Bar QML contains no Quickshell, Process, FileView, IpcHandler, shell commands, or JSON cache integration.
- Do not invent a Typhon workspace protocol or expose fake production workspace data.
- Preserve existing Dock, Alt+Tab, Spotlight, Typhon, catalog, launcher, and shell IPC behavior.
- Do not add ordinary Qt-window fallback when Layer Shell is unavailable.

---

### Task 1: Pure Bar contracts and failing tests

**Files:**
- Create: `Bar/core/BarSurfacePolicy.hpp`, `Bar/core/BarSurfacePolicy.cpp`
- Create: `Bar/core/BarClockService.hpp`, `Bar/core/BarClockService.cpp`
- Create: `Bar/core/BarPopupController.hpp`, `Bar/core/BarPopupController.cpp`
- Create: `Bar/core/WorkspaceModel.hpp`, `Bar/core/WorkspaceModel.cpp`
- Create: `Bar/tests/BarCoreTest.cpp`

**Interfaces:**
- `BarSurfacePolicy::reserve()`, `launcher()`, `status()`, `popupOverlay()` return deterministic `AstreaLayerShellConfig` values.
- `BarSurfacePolicy::statusWidth(int outputWidth, int launcherWidth, int contentWidth)` and `popupX(int outputWidth, int cardWidth, int anchorX, int sidePadding)` return clamped geometry.
- `BarClockService::formatTime(QDateTime, QLocale, ClockTimeFormat)` and `nextMinuteDelay(QDateTime)` are pure helpers; the QObject emits `changed` on minute-boundary updates.
- `WorkspaceModel` is a QAbstractListModel exposing id/active/occupied/urgent/outputId roles and `replaceWorkspaces()`.
- `BarPopupController` exposes closed/open kind, anchorX, `open(kind, anchorX)`, `toggle(kind, anchorX)`, `close()`, and `clearForOutput()`.

- [ ] Write Qt tests for exact policy values, status non-overlap/capping, popup clamping, deterministic clock formatting/scheduling, model ordering/roles, popup replacement/idempotence, and output teardown.
- [ ] Run the new test target and observe the expected missing-file/build failure.
- [ ] Implement the smallest pure contracts and QObject behavior needed by the tests.
- [ ] Re-run the focused tests and then refactor only while green.

### Task 2: Runtime ownership and native actions

**Files:**
- Modify: `Shell/runtime/ShellRuntime.hpp`, `Shell/runtime/ShellRuntime.cpp`
- Create: `Bar/core/BarController.hpp`, `Bar/core/BarController.cpp`
- Modify: `Shell/tests/ShellRuntimeTest.cpp`

**Interfaces:**
- `ShellRuntime::barController()`, `barClock()`, and `workspaceModel()` return the single runtime-owned objects.
- `BarController::showSearch()` calls the injected `SpotlightController::show()`.
- `BarController::launchSettings()` resolves `astrea-settings.desktop` through `DesktopEntryCatalog::findByDesktopFileName()` and passes the resulting record to `ApplicationLauncher::launchDesktop()`; it returns false when unavailable.
- Capability properties explicitly report Search, Settings, About, Force Quit, Lockscreen, Power, and notification-history support.

- [ ] Add failing ownership/action tests, including a temporary catalog entry and a launcher spy.
- [ ] Run the focused tests to verify they fail for missing Bar runtime ownership.
- [ ] Add Bar objects to ShellRuntime initialization/destruction and implement BarController action/capability behavior.
- [ ] Run ShellRuntime tests and the focused Bar core tests.

### Task 3: Per-output surface lifecycle

**Files:**
- Create: `Bar/platform/wayland/BarSurfaceManager.hpp`, `Bar/platform/wayland/BarSurfaceManager.cpp`
- Create: `Bar/platform/wayland/BarSurfaceBundle.hpp`, `Bar/platform/wayland/BarSurfaceBundle.cpp`
- Modify: `Shell/app/AstreaShellApplication.hpp`, `Shell/app/AstreaShellApplication.cpp`
- Modify: `shared/platform/wayland/LayerShellHelper.hpp`, `shared/platform/wayland/LayerShellHelper.cpp` only if required for input transparency
- Create: `Bar/tests/BarSurfaceLifecycleTest.cpp`

**Interfaces:**
- `BarSurfaceManager(QGuiApplication&, QQmlApplicationEngine&, BarController*, BarClockService*, WorkspaceModel*)` subscribes to `screens()`, `screenAdded`, and `screenRemoved`.
- `bundleCount()`, `popupOpen()`, `layerConfigurationRequested()` provide stable diagnostics.
- `BarSurfaceBundle::create()`, `map()`, `destroy()`, and `clearPopup()` are idempotent and never use primaryScreen ownership.

- [ ] Add lifecycle tests using fake screen identities/model operations and assert one bundle per add/remove cycle.
- [ ] Run the focused lifecycle tests before implementation.
- [ ] Implement hidden QML window creation with initial `screen` assignment, Layer Shell policy configuration before show, reserve `Qt::WindowTransparentForInput`, and deterministic bundle teardown.
- [ ] Wire manager creation/destruction to AstreaShellApplication initialization/run lifecycle without changing existing surface ownership.
- [ ] Run focused lifecycle and existing shell tests.

### Task 4: Visual QML port, resource, and static guards

**Files:**
- Create: `Bar/qml/ReserveSurface.qml`, `LauncherSurface.qml`, `StatusSurface.qml`, `PopupOverlaySurface.qml`
- Create: `Bar/qml/components/ShellBarTheme.qml`, `BarSegment.qml`, `IndicatorButton.qml`, `WorkspaceStrip.qml`, `AstreaMenu.qml`, `Clock.qml`, `PopupCard.qml`
- Create: `Bar/assets/astrea.png` copied from the supplied reference asset
- Modify: `Shell/CMakeLists.txt`
- Create: `Bar/tests/BarQmlSmokeTest.cpp`, `Bar/tests/check_bar_qml.py`

**Interfaces:**
- Root surfaces accept `barController`, `clockService`, `workspaceModel`, `popupController`, `outputWidth`, and `screen` context properties.
- AstreaMenu calls `barController.showSearch()` and `barController.launchSettings()`; unsupported actions are disabled/hidden by capability properties.
- PopupOverlaySurface handles outside click on the background and consumes card clicks.
- Clock renders native `BarClockService` values in the reference date/time arrangement.

- [ ] Add static guard tests and QML smoke expectations before production QML exists.
- [ ] Run them and observe the expected failures.
- [ ] Add the resource and Qt Quick module registration, preserving the existing Borealis-derived visual tokens, spacing, hover/pressed/active states, workspace animation, and popup fade/scale values.
- [ ] Implement the static guard over only new production `Bar/qml` files; keep reference/migration docs exempt.
- [ ] Run QML smoke/static tests.

### Task 5: Diagnostics, documentation, and full verification

**Files:**
- Modify: `Shell/app/AstreaShellApplication.cpp` status JSON
- Create: `Shell/docs/TOPBAR_M8_A.md`
- Modify: `Shell/CMakeLists.txt`, root/Bar CMake files, and test registration as needed

- [ ] Add stable `bar.enabled`, `bar.outputBundles`, `bar.popupOpen`, and Bar Layer Shell configuration fields without changing schema version 1.
- [ ] Document implemented M8-A behavior, intentional differences, live validation status, and M8-B+ deferrals.
- [ ] Reconfigure/reuse `build/debug`, build the full tree, and run `ctest --test-dir build/debug --output-on-failure`.
- [ ] Run available QML lint/static/CI-equivalent checks and inspect the final diff, graph coverage, and git status.
- [ ] Report exact commands/results and do not claim live Wayland/hotplug checks unless executed.
