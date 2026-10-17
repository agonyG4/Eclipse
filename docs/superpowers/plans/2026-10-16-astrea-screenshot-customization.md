# Astrea Screenshot Customization Implementation Plan

> **For agentic workers:** Execute inline on `main`. AGENTS.md explicitly prohibits subagents and requires Aether build outputs. Steps use checkbox syntax for tracking.

**Goal:** Deliver a shared, live screenshot presentation configuration across Eclipse Shell and Astrea Settings, with real rounded masking, predictable dismissal, and preserved clipboard and native drag behavior.

**Architecture:** A typed `ScreenshotConfiguration` QObject in shared Eclipse code owns strict JSON parsing, atomic writes, external replacement watching, and defaults. Shell and Settings own separate instances pointed at the same XDG config file. A shared `ScreenshotVisual.qml` source is compiled into both QML modules and is used by the thumbnail, preview, and Settings mock desktop.

**Tech Stack:** C++20, Qt 6.8 Core/Gui/Qml/Quick, Qt Quick Effects `MultiEffect`, QML Settings controls, CMake, Qt Test.

## Global Constraints

- Work on `main`; do not create a branch.
- Preserve all current dirty changes. Typhon's unrelated dirty tree must not be staged or committed.
- Build outputs stay in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; verify the effective path before every build command.
- Do not restart the running Shell or compositor.
- Leave Typhon protocols, global Material Programs, and global animation catalogs unchanged unless a concrete missing capability is discovered.
- No user screenshot PNG pixels or clipboard payload bytes may be altered for presentation.
- Commit completed Eclipse logical changes; stage only the intended files and retain unrelated changes.

---

## File Structure

- `shared/screenshot/ScreenshotConfiguration.hpp/.cpp`: typed values, defaults, bounds, JSON parser/serializer, `QSaveFile` persistence, debounced file/directory watcher, status, reset.
- `shared/qml/ScreenshotVisual.qml`: common aspect-fit image, rounded `MultiEffect` alpha mask, border, and shadow.
- `Shell/screenshot/ScreenshotController.hpp/.cpp`: own/expose configuration; respect show/copy behavior.
- `Shell/screenshot/ScreenshotThumbnail.qml`: four-corner placement, configuration bindings, lifecycle/idle ownership, frozen drag payload and image, interaction suspension.
- `Shell/screenshot/ScreenshotPreview.qml`: shared visual styling and configured maximum scale while preserving transition geometry.
- `Settings/core/SettingsController.hpp/.cpp`: own/expose shared screenshot configuration.
- `Settings/core/navigation/SettingsNavigationCatalog.cpp`: route `screenshots` under `customization`.
- `Settings/qml/CMakeLists.txt`, `Shell/CMakeLists.txt`, `shared/CMakeLists.txt`: register shared model and QML source with both applications.
- `Settings/qml/pages/appearance/Screenshots.qml`: localized controls, live mock desktop, replay, status, and reset.
- `Settings/assets/i18n/en_US.json`: screenshots page/navigation strings.
- Focused Settings configuration/navigation/QML tests and Shell screenshot QML/controller tests: behavior and regression coverage.

## Tasks

### Task 1: Add the shared typed configuration and persistence model

**Files:** Create `shared/screenshot/ScreenshotConfiguration.hpp/.cpp`; update `shared/CMakeLists.txt`; create or extend a Settings unit test target for configuration behavior.

**Interface:** `ScreenshotConfiguration` is a `QObject` with typed `Q_PROPERTY` values for every schema field, `configurationChanged`, `statusChanged`, `filePath()`, and `Q_INVOKABLE resetToDefaults()`. It resolves the default file from `QStandardPaths::GenericConfigLocation` and accepts an optional path for tests.

- [ ] Write tests for defaults, per-field normalization, enum rejection, QColor validation, the 64 KiB cap, unknown-key preservation, field reset, quick consecutive edits, and external atomic replacement.
- [ ] Verify the new tests fail because the configuration type does not exist.
- [ ] Implement the typed values and exact defaults/bounds from the design document.
- [ ] Implement strict JSON object load, independent field defaults, unknown root-key retention, and nested `thumbnailSize` width/height updates.
- [ ] Persist each successful change with `QSaveFile`; watch both the file and parent directory with a debounce; deduplicate identical content and never write during reload.
- [ ] Run the focused Settings unit test target and confirm nonzero executed cases.
- [ ] Commit the shared model and its tests.

### Task 2: Add the reusable rounded screenshot visual

**Files:** Create `shared/qml/ScreenshotVisual.qml`; add the source to `Settings/qml/CMakeLists.txt` and `Shell/CMakeLists.txt`; add a focused QML visual test fixture.

**Interface:** `ScreenshotVisual.qml` has properties `source`, `cornerRadius`, `borderEnabled`, `borderWidth`, `borderColor`, `shadowEnabled`, and `shadowStrength`. It preserves aspect ratio and allows shadow output beyond image bounds.

- [ ] Write QML tests for rounded alpha-mask wiring, radius zero/large radius, border alignment, shadow on/off, and portrait/landscape/panoramic source sizing.
- [ ] Run the visual test and confirm the intended new bindings are initially absent.
- [ ] Implement the rounded mask using `QtQuick.Effects.MultiEffect`, a hidden rounded mask item, and the Wallpaper QML mask thresholds; do not introduce a shader or Wayland child window.
- [ ] Apply the border over the same rounded frame and keep the component root unclipped.
- [ ] Check the effect texture size is limited to the visual image rectangle and Qt Quick logical sizing remains device-pixel-ratio aware.
- [ ] Run the focused QML test and commit the shared visual and resource registration.

### Task 3: Wire Shell behavior and own animation lifecycle

**Files:** Modify `Shell/screenshot/ScreenshotController.hpp/.cpp`, `Shell/screenshot/ScreenshotThumbnail.qml`, `Shell/screenshot/ScreenshotPreview.qml`, and focused Shell tests/CMake registration.

**Interface:** `ScreenshotController.configuration` returns its shared `ScreenshotConfiguration`. The thumbnail uses explicit lifecycle states `Hidden`, `Entering`, `Visible`, and `Dismissing`; native drag is a separate interaction state.

- [ ] Add controller tests showing default auto-copy, configured no-copy, hidden thumbnail, and configuration ownership without changing saved PNG output.
- [ ] Add QML lifecycle tests for timeout animation, visible-through-exit, natural completion only, interrupted exit, new screenshot token invalidation, disabled animation, hover/press countdown pause, all four slide directions, and repeated quick captures.
- [ ] Add drag tests proving snapshot payload stability, frozen input-region geometry, canceled drag restoration, successful-drop dismissal, and no drag when disabled.
- [ ] Run these tests first and confirm failures target the missing behavior.
- [ ] Wire the model into Shell's controller. Preserve canonical `m_imagePngData`, clipboard PNG bytes, and file URI/path.
- [ ] Replace thumbnail hardcoded frame values with model bindings and the shared visual. Keep its card as the same full-screen Window child and preserve the close-button removal.
- [ ] Add generation-owned explicit entry/exit runs. Use `onFinished` with lifecycle token and screenshot generation checks; never dismiss from `onStopped`.
- [ ] Pause/resume the idle deadline across hover, press, and native drag. Snapshot drag mime data and image source before native DnD, and defer geometry/input updates until drag end.
- [ ] Apply click action, show/copy, location, edge clearance, and aspect-preserving configured size.
- [ ] Replace preview image/card internals with the shared visual and bind its configured maximum scale.
- [ ] Run all focused Shell screenshot tests and commit Shell integration.

### Task 4: Expose settings, route, and localization

**Files:** Modify `Settings/core/SettingsController.hpp/.cpp`, `Settings/core/navigation/SettingsNavigationCatalog.cpp`, `Settings/qml/CMakeLists.txt`, `Settings/assets/i18n/en_US.json`, navigation tests.

**Interface:** `SettingsController.screenshots` is a constant pointer to the owned `ScreenshotConfiguration`, with its property setters persisting validated edits.

- [ ] Add navigation assertions for route ID `screenshots`, parent `customization`, and its resource URL.
- [ ] Add a QML route smoke assertion for `Settings/qml/pages/appearance/Screenshots.qml` and localization-key resolution.
- [ ] Run those tests and confirm the route/resource is absent before implementation.
- [ ] Expose the model from SettingsController and register the Screenshots page in the customization catalog/resource list.
- [ ] Add English strings for every label, option, status, replay action, and reset action.
- [ ] Run Settings navigation and QML smoke tests and commit controller/route/localization wiring.

### Task 5: Build the live Settings page and shared-style preview

**Files:** Create `Settings/qml/pages/appearance/Screenshots.qml`; create a small vector mock screenshot asset if needed; add Settings integration tests.

**Interface:** The page uses `SettingsController.screenshots`, the existing Form and Controls components, and the same `ScreenshotVisual.qml` as Shell. Preview binds position, dimensions, radius, border, shadow, animation styles, and durations immediately.

- [ ] Add a QML integration test that loads the page, changes the main fields, observes mock preview geometry/style updates, replays animations, checks status, and invokes reset.
- [ ] Run the new integration test and confirm it fails before the page exists.
- [ ] Implement the compact mock desktop, four-corner placement, sliders/selectors/toggles, native color selection, entrance/exit replay, reset, and localized status.
- [ ] Ensure the Settings page only writes screenshot JSON and does not access Typhon Material Program settings.
- [ ] Run the integration test and the Settings component smoke tests; commit the page and tests.

### Task 6: Validate integration and adversarial cases

**Files:** No new product files unless a focused regression is found.

- [ ] Verify `/mnt/Aether/Desktop/GitHub/Eclipse-build` exists as the intended output directory and inspect `CMakeCache.txt` for its binary/source paths before building.
- [ ] Reconfigure and build incrementally with `cmake -S /home/agony/GitHub/Eclipse -B /mnt/Aether/Desktop/GitHub/Eclipse-build`; run build/test commands with all temporary compilation outputs explicitly under that tree.
- [ ] Run focused Shell screenshot tests, Settings configuration/navigation/QML tests, and report actual executed case counts.
- [ ] Review generated test output for warnings and detect zero-case or skipped targets.
- [ ] Audit shadow extents at screen edges, animation token ownership, drag payload snapshot, input-region stability, file replacement loops, config bounds, and global Typhon settings isolation.
- [ ] Check both git statuses, stage only Eclipse screenshot task files, commit any final focused fix, and report remaining real-desktop acceptance checks without claiming those live cases were verified.
