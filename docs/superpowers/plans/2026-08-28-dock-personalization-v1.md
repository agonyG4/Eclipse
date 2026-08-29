# Dock Personalization v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. This checkout uses inline execution because the repository instructions prohibit subagents.

**Goal:** Build a shared, validated Dock preference contract consumed by the resident Dock and native Settings, with atomic persistence, external reload, position-aware runtime geometry, auto-hide policy, and a real Settings route.

**Architecture:** Move compositor-independent configuration/schema/persistence into `shared/dock`, keep Dock watcher/controller and Layer Shell in Dock/Shell, and expose a focused `SettingsDockController` through `SettingsController.dock`. Generalize the existing DockPanel primary-axis algorithm and preserve the existing fixed envelope, input-region, reorder, activation, and exclusive-zone contracts.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick, LayerShellQt through the existing helper, Qt Quick Controls, QTest, CMake, QSaveFile, QFileSystemWatcher.

## Global Constraints

- Configuration remains `~/.config/AstreaOS/dock.json`.
- C++ owns schema, validation, filesystem access, atomic persistence, watching, and runtime policy.
- QML owns presentation, controls, animations, pointer interaction, and preview rendering; it does not parse/write JSON or access filesystem/process/IPC/DBus/compositor APIs.
- Settings links only to compositor-independent shared Dock code and never to LayerShellQt, Typhon-private APIs, Shell runtime objects, or production `DockController`.
- Preserve the 1 MiB file limit, 256-pin limit, pin validation, unknown JSON keys, exact pins, malformed-file refusal, and `QSaveFile` atomic commit behavior.
- `bottomMargin` is read only as a legacy fallback when `edgeMargin` is absent; new Settings writes use `edgeMargin`.
- Preserve all existing Bottom Dock regression invariants and never reserve transient magnification/drag headroom.
- No blur, shaders, opacity/glass/theme controls, Quickshell, new Settings-to-Shell IPC, or global animation policy changes.
- Run focused tests after each subsystem and stage only feature files in each commit; preserve unrelated pre-existing worktree changes.

---

### Task 1: Shared Dock configuration contract and atomic store

**Files:**
- Create: `shared/dock/DockConfig.hpp`
- Create: `shared/dock/DockConfig.cpp`
- Create: `shared/dock/DockConfigStore.hpp`
- Create: `shared/dock/DockConfigStore.cpp`
- Modify: `Dock/services/DockConfigValidation.hpp`
- Modify: `Dock/services/DockConfigValidation.cpp`
- Modify: `Dock/services/DockConfigWatcher.hpp`
- Modify: `Dock/services/DockConfigWatcher.cpp`
- Modify: `Dock/services/DockConfigPersistence.hpp`
- Modify: `Dock/services/DockConfigPersistence.cpp`
- Modify: `Dock/CMakeLists.txt`
- Modify: `Settings/core/CMakeLists.txt`
- Test: `Dock/tests/DockConfigWatcherTest.cpp`
- Test: `Dock/tests/DockConfigPersistenceTest.cpp`
- Create: `shared/tests/DockConfigTest.cpp`
- Modify: `shared/CMakeLists.txt` or the existing shared test registration file that owns shared unit targets.

**Interfaces:**
- `DockConfig` contains `iconSize`, `panelPadding`, `itemSpacing`, `hoverEffect`, `magnificationScale`, `magnificationRadius`, `edgeMargin`, `position`, `floating`, `cornerRadius`, `autoHide`, `indicatorStyle`, `indicatorSize`, `animationsEnabled`, `animationSpeed`, and `pins`, with the specified defaults.
- `DockConfigCodec::parse(const QJsonObject &, QStringList *)` returns a field-locally validated `DockConfig`.
- `DockConfigCodec::readJsonObject(const QString &)` returns `{object, exists, error}` and rejects non-object, malformed, unreadable, and over-limit files.
- `DockConfigStore::writePins(const QStringList &, QString *)` patches only `pins`; `writeConfig(const DockConfig &, QString *)` patches all canonical known fields, removes only `bottomMargin`, preserves unknown keys, and commits with `QSaveFile` and disabled direct fallback.
- Compatibility Dock service APIs continue to expose `DockConfig`, validation constants, `validDesktopFileName`, and `validatePinList` to current consumers.

- [ ] **Step 1: Add failing shared contract tests** for defaults, all new fields, enum fallback, finite-number fallback, integer/real clamping, legacy `bottomMargin` migration, exact pin preservation, unknown-key preservation, malformed-file refusal, and atomic replacement.
- [ ] **Step 2: Run the new shared test target and confirm expected failures** because the shared headers/store do not yet exist.
- [ ] **Step 3: Implement the shared `DockConfig` and codec** with canonical field parsing and the existing pin/file limits; make integers accept numeric JSON and round after bounded clamping, and make reals reject non-finite values before bounded clamping.
- [ ] **Step 4: Implement `DockConfigStore`** by reading the current JSON object first, refusing malformed/oversized existing input, applying a known-field patch to a copy, serializing under 1 MiB, creating only the parent directory, and committing through `QSaveFile`.
- [ ] **Step 5: Refactor Dock validation/watcher/persistence wrappers** to call the shared codec/store, parse `edgeMargin` before legacy `bottomMargin`, and preserve the old `magnificationEnabled` fallback when `hoverEffect` is absent.
- [ ] **Step 6: Re-run the shared, watcher, and persistence tests** and confirm the new contract is green while all existing pin tests remain green.
- [ ] **Step 7: Commit** with `git add` limited to the shared contract, Dock service, CMake, and tests, using `git commit -m "feat: share Dock configuration contract"`.

### Task 2: Settings Dock controller and native composition root

**Files:**
- Create: `Settings/services/dock/SettingsDockController.hpp`
- Create: `Settings/services/dock/SettingsDockController.cpp`
- Modify: `Settings/core/SettingsController.hpp`
- Modify: `Settings/core/SettingsController.cpp`
- Modify: `Settings/app/SettingsApplication.cpp`
- Modify: `Settings/core/CMakeLists.txt`
- Modify: `Settings/tests/unit/SettingsControllerTest.cpp`
- Create: `Settings/tests/unit/SettingsDockControllerTest.cpp`

**Interfaces:**
- `SettingsDockController(QString configPath = {}, QObject *parent = nullptr)` uses the injected path for tests or `QDir::homePath() + "/.config/AstreaOS/dock.json"` in production.
- Q_PROPERTIES and typed setters exist for every `DockConfig` personalization field, plus `QString lastError`, `bool pendingWrite`, and `flush()`; enum values use validated QString contracts (`bottom/left/right`, etc.) to match the JSON schema.
- The controller loads `DockConfigCodec::readJsonObject`/`parse`, observes the file and its parent directories with a debounced `QFileSystemWatcher`, suppresses redundant property signals, schedules debounced writes, and restores the previous effective state on write failure.
- `SettingsController::dock()` returns the controller as a constant Q_PROPERTY, with the controller owned by `SettingsController`.

- [ ] **Step 1: Add failing controller tests** for defaults, injected path, typed setter persistence, invalid input clamping/fallback, no redundant changes, external atomic replacement/recreation, bounded errors, and final `flush()` behavior.
- [ ] **Step 2: Run `settings-dock-controller-test` and confirm it fails** on the missing controller and property surface.
- [ ] **Step 3: Implement the controller** with a private `DockConfig`, effective snapshot, file watcher, 150–250 ms single-shot write timer, bounded 512-byte errors, typed setter validation through the shared codec, and `flush()` that stops the timer and commits once.
- [ ] **Step 4: Wire `SettingsController.dock`** into both default and injected constructors and ensure its lifetime is below the main controller; do not expose any Dock runtime type.
- [ ] **Step 5: Register the unit test target** against `astrea-settings-core` and the shared Dock source target/library without compiling production `.cpp` files directly in the test.
- [ ] **Step 6: Run the focused Settings unit targets** and confirm the controller behavior and existing navigation/controller tests pass.
- [ ] **Step 7: Commit** only the controller, Settings composition, CMake, and tests with `git commit -m "feat(settings): add Dock preferences controller"`.

### Task 3: Navigation route, translations, and Settings Dock page

**Files:**
- Create: `Settings/qml/pages/appearance/Dock.qml`
- Modify: `Settings/qml/CMakeLists.txt`
- Modify: `Settings/core/navigation/SettingsNavigationCatalog.cpp`
- Modify: `Settings/assets/i18n/en_US.json`
- Modify: `Settings/tests/unit/SettingsNavigationModelTest.cpp`
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake` only if its route/page assertions require the new page.

**Interfaces:**
- Navigation entry ID is `dock`, under the existing `appearance` group, with route `qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml`.
- The page consumes only `SettingsController.dock`, `I18n`, `Theme`, and existing `Form` components.
- Preview object names include `dockPreview`, `dockPreviewChrome`, and `dockPreviewIcons`; control object names are stable for layout, behavior, and indicator sections.

- [ ] **Step 1: Add failing route and QML smoke tests** for the catalog entry, exact route, offscreen page load, preview, all required controls, dependent enabled states, translation keys, and forbidden runtime/platform API text.
- [ ] **Step 2: Run the focused Settings tests and confirm the route/page assertions fail** before adding the page.
- [ ] **Step 3: Add the native catalog descriptor and QML module file** using the existing stable-ID/Loader route architecture.
- [ ] **Step 4: Add translation keys** for section headers, labels, sublabels, enum options, preview text, and bounded write-error presentation; use `I18n.tr(key, fallback)` at every user-facing QML string.
- [ ] **Step 5: Implement the page** with a small representative-icon preview driven directly by controller properties, Layout controls, Behavior controls, and Indicators controls. Use `Slider`/`SelectButton`/`ToggleSwitch` and disable magnification/radius when hover effect is not magnification, animation speed when animations are disabled, and indicator size when indicator style is none.
- [ ] **Step 6: Run `settings-qml-smoke-test`, `settings-navigation-model-test`, and `settings-structure-test` offscreen** and fix any QML warnings or missing object names.
- [ ] **Step 7: Commit** only the navigation, QML, translations, and Settings tests with `git commit -m "feat(settings): add Dock personalization page"`.

### Task 4: Dock controller runtime policy and generalized metrics

**Files:**
- Modify: `Dock/core/DockMetrics.hpp`
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Modify: `Shell/runtime/ShellRuntime.hpp`
- Modify: `Shell/runtime/ShellRuntime.cpp`
- Modify: `Shell/app/AstreaShellApplication.hpp`
- Modify: `Shell/app/AstreaShellApplication.cpp`
- Test: `Dock/tests/DockControllerTest.cpp`
- Test: `Shell/tests/ShellRuntimeTest.cpp` if runtime wiring needs a focused assertion.

**Interfaces:**
- Metrics expose the current Bottom-compatible resting cross thickness plus orientation-aware delegate primary/cross extents and `position` helpers.
- `DockController` exposes all validated settings as Q_PROPERTIES, `effectiveEdgeMargin`, `restingCrossThickness`, `exclusiveZone`, `revealed`, and an obstruction/policy signal; it keeps `visible` as mapping/manual-override state.
- `DockController::setPointerInside(bool)` is the QML-facing policy input; `show()`/`hide()` remain manual mapping overrides.
- Auto-hide uses a single bounded leave timer. Always and intelligent-obstructed modes use zero exclusive zone; never and intelligent-unobstructed use resting cross thickness. Temporary reveal never changes the zone.

- [ ] **Step 1: Add failing controller tests** for propagation of every field, unchanged config signal suppression, floating effective-margin behavior, indicator/animation properties, Never/Always/intelligent policy, active maximized/fullscreen obstruction, edge reveal/leave-delay, and manual show/hide coherence.
- [ ] **Step 2: Run `dock-controller-test` and confirm new cases fail** against the current bottom-only config/controller.
- [ ] **Step 3: Extend `DockMetrics` and `DockController`** with the orientation/settings properties, full-config equality, policy timer, obstruction calculation from `m_runtimeSnapshot`, and exact signal transitions.
- [ ] **Step 4: Change Shell wiring** so config changes reconfigure the surface and policy/reservation changes update only the exclusive zone; mapping uses `controller.visible()` and `controller.exclusiveZone()`.
- [ ] **Step 5: Run focused Dock/controller/Shell tests** and confirm existing pin, launch, runtime-order, and exact-window tests remain green.
- [ ] **Step 6: Commit** the runtime controller and Shell wiring with `git commit -m "feat(dock): add orientation and auto-hide policy"`.

### Task 5: Position-aware Layer Shell and output geometry

**Files:**
- Modify: `Dock/platform/wayland/DockLayerShellSurface.hpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`
- Modify: `Dock/core/DockSurfaceGeometry.hpp`
- Modify: `Dock/core/DockSurfaceGeometry.cpp`
- Modify: `Dock/tests/DockLayerShellSurfaceTest.cpp`
- Modify: `Dock/tests/DockSurfaceGeometryTest.cpp`
- Modify: `Dock/CMakeLists.txt` if shared config headers need adding to the target source set.

**Interfaces:**
- `DockLayerShellSurface::configurationFor(const DockConfig &, int)` returns an `AstreaLayerShellConfig` policy for deterministic tests; `configure` applies it.
- Bottom anchors Bottom/margins Bottom, Left anchors Left/margins Left, and Right anchors Right/margins Right; all other margins are zero.
- `DockSurfaceGeometry::delegateRectInOutput(output, surface, position, edgeMargin, rect)` maps Bottom centered/bottom, Left margin-left/centered vertically, and Right margin-right/centered vertically. The old bottom overload remains for existing callers.

- [ ] **Step 1: Add failing geometry and Layer Shell tests** for all three anchor policies, selected-edge margins, resting exclusive zone, zero auto-hide zone, and output-local context-menu rectangles.
- [ ] **Step 2: Run the focused geometry/Layer Shell tests and confirm expected failures.**
- [ ] **Step 3: Implement the pure position policy and generalized output-local mapping** while retaining the old Bottom helper behavior exactly.
- [ ] **Step 4: Update DockAppDelegate context-menu anchoring** to pass `DockController.position` and `effectiveEdgeMargin`, never a bottom-only margin contract.
- [ ] **Step 5: Run `dock-layer-shell-surface-test` and `dock-surface-geometry-test`** and confirm the current Bottom expected rectangles still match.
- [ ] **Step 6: Commit** the position policy and geometry changes with `git commit -m "feat(dock): support edge-aware Layer Shell geometry"`.

### Task 6: Generalize Dock QML primary-axis geometry and presentation

**Files:**
- Modify: `Dock/qml/Main.qml`
- Modify: `Dock/qml/components/DockPanel.qml`
- Modify: `Dock/qml/components/DockAppDelegate.qml`
- Modify: `Dock/tests/DockHoverQmlTest.cpp`

**Interfaces:**
- DockPanel derives `vertical`, `primaryExtent`, `crossExtent`, `primaryPointer`, and `primaryDirection` from `DockController.position`; the existing `pointerX`, Bottom properties, and helper behavior remain compatible for current tests.
- One `Grid`/primary-axis pass computes scales, closest delegate, prefix displacement, drag origin/translation/target, transformed interaction rectangles, and stable structural envelope for all edges.
- Delegate exposes an edge-aware transform origin and indicator style/size/side; existing activation, context menu, drag cancellation, and input-region hooks remain intact.

- [ ] **Step 1: Add failing focused vertical QML tests** for Left/Right inward growth, symmetric scale influence, first/middle/last delegates, stable envelope, input region, primary-axis drag/reorder/release, and indicator side; add settings-driven radius/spacing/padding/corner/animation assertions.
- [ ] **Step 2: Run `dock-hover-qml-test` and confirm the new cases fail** while the current Bottom suite remains the baseline.
- [ ] **Step 3: Replace only the axis-specific coordinate calculations** with primary/cross helpers and a single algorithm pass. Keep Bottom anchors, bottom-origin scale, spacing, and transform values matching existing behavior.
- [ ] **Step 4: Update delegate visual geometry** for left/right origin and lift direction, configured corner radius, configured indicator line/dot/none, and configured indicator thickness; keep indicators outside icon transforms and only show them for known-running rows.
- [ ] **Step 5: Add collapsed edge reveal geometry** to the input-region union without making fixed transparent envelope headroom clickable; route HoverHandler enter/leave to `DockController.setPointerInside`.
- [ ] **Step 6: Centralize Dock-specific animation duration** in a QML helper expression/property so disabled animations are immediate, `animationSpeed=1.0` preserves current durations, and speed changes scale durations consistently.
- [ ] **Step 7: Run the complete focused Dock QML suite plus `qmllint`** for Main/Panel/Delegate and resolve all warnings without touching shared icon-quality behavior.
- [ ] **Step 8: Commit** the QML runtime generalization and tests with `git commit -m "feat(dock): generalize personalized edge geometry"`.

### Task 7: Documentation and complete verification

**Files:**
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/CONFIGURATION.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TESTING.md`
- Modify: `Settings/docs/ARCHITECTURE.md`
- Modify: `Settings/AGENTS.md` only if the new shared boundary needs a durable rule.

- [ ] **Step 1: Document** canonical fields/defaults/ranges, legacy migration, unknown/pin preservation, intelligent obstruction limitation, floating margin semantics, fixed envelope/exclusive-zone/input-region behavior, vertical direction, indicator/animation semantics, and Settings dependency boundaries.
- [ ] **Step 2: Run focused tests in Debug** for shared config/store, Dock watcher/controller/geometry/Layer Shell/QML, Settings controller/navigation/QML/static tests, and record actual results.
- [ ] **Step 3: Run the required full verification commands**:
  ```bash
  cmake --build --preset debug
  ctest --preset debug --output-on-failure
  cmake --build --preset release
  ctest --preset release --output-on-failure
  qmllint Dock/qml/Main.qml Dock/qml/components/DockPanel.qml Dock/qml/components/DockAppDelegate.qml
  git diff --check
  git status --short
  ```
- [ ] **Step 4: Review the diff against every scope-discipline and regression requirement**, noting that offscreen tests cannot establish live Wayland delivery or Typhon overlap geometry.
- [ ] **Step 5: Commit documentation and verification updates** with `git commit -m "docs: document Dock Personalization v1"`, staging only the documentation and feature verification changes.
- [ ] **Step 6: Report** architecture, schema/compatibility, Settings UI, Dock runtime, tests, exact command results, live Typhon qualification still required, and final `git status --short`.

## Plan self-review

- Shared parsing/validation/atomic persistence: Task 1.
- Settings typed controller, watcher, debounce, error contract: Task 2.
- Native route, QML page, preview, translation/static tests: Task 3.
- Runtime properties, floating margin, auto-hide, intelligent heuristic, manual visibility: Task 4.
- Position-aware anchors, exclusive zone, and context-menu geometry: Task 5.
- One generalized Dock algorithm, indicators, corner radius, animations, input reveal, and QML regression coverage: Task 6.
- Documentation, full Debug/Release verification, live qualification disclosure, and final status: Task 7.
- No placeholders or undefined neighboring interfaces are used; all new interfaces are named in the task that produces them before later tasks consume them.
