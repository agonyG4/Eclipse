# Settings Astrea Slider and Control Taxonomy Implementation Plan

> **For inline execution:** This plan is executed in the current approved Eclipse `main` checkout. No sub agents, branches, worktrees, or temporary branches are used.

**Goal:** Add the reusable Astrea Settings slider, migrate Dock and affected page consumers, move interactive controls into `components/controls/`, and verify the module/docs architecture without changing behavior.

**Architecture:** `controls/Slider.qml` derives from `QQC2.Slider` and owns only visual delegates and centralized theme-derived geometry. Existing Dock handlers remain page-owned, while the moved ToggleSwitch, SelectButton, and SearchField retain their source and public type behavior. The registered `Astrea.Settings` module remains the single production QML target and grows from 39 to 40 files.

**Tech Stack:** Qt 6.8, Qt Quick/QML, Qt Quick Controls 2 Basic style, CMake/Ninja, Qt Test, CTest, and `rtk` command wrappers.

## Global Constraints

- Work directly on `main`; do not create a branch, worktree, or temporary branch.
- Keep builds in `build/settings-slider-debug` and `build/settings-slider-release` inside this checkout.
- Preserve `QQuickStyle::setStyle("Basic")` and all existing Dock ranges, handlers, object names, enabled dependencies, and flush semantics.
- Use `visualPosition` for every slider progress and handle-position calculation.
- Do not add backend state, persistence logic, global Qt style, raw slider `MouseArea`, blur, Liquid Glass, `MultiEffect`, `ShaderEffect`, `Canvas`, or screenshot automation.
- `controls/` owns reusable interactive primitives; `form/` owns Settings-specific structural/layout composition.
- Use the existing registered `astrea-settings-ui` target in integration tests; do not compile production sources again.

## File map

- Create: `Settings/qml/components/controls/Slider.qml`
- Move unchanged: `Settings/qml/components/form/{ToggleSwitch,SelectButton,SearchField}.qml` to `Settings/qml/components/controls/`
- Modify registration: `Settings/qml/CMakeLists.txt`
- Modify consumers: `Settings/qml/pages/appearance/{Dock,Wallpaper}.qml`, `Settings/qml/pages/system/Compositor.qml`
- Modify tests: `Settings/tests/integration/SettingsComponentSmokeTest.cpp`, `Settings/tests/integration/SettingsQmlSmokeTest.cpp`, `Settings/tests/static/SettingsStructureTest.cmake`
- Modify docs: `Settings/docs/{COMPONENT_CATALOG,STRUCTURE,ARCHITECTURE,TESTING}.md`
- Design/plan workflow records: the two dated files in `docs/superpowers/{specs,plans}/`

### Task 1: Extend contract tests first

**Files:**
- Modify: `Settings/tests/integration/SettingsComponentSmokeTest.cpp`
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`

- [ ] Add a `Settings.Slider` fixture instance with object name `componentSmokeSlider`, `from: 0`, `to: 10`, `stepSize: 2`, and `value: 4`; assert the created object exposes those inherited properties and, when invoking `increase()` succeeds, its value becomes 6.
- [ ] Add a Dock source invariant that requires `import "../../components/controls" as Controls` and `Controls.Slider`, `Controls.ToggleSwitch`, and `Controls.SelectButton`, while rejecting direct unqualified `Slider {` blocks.
- [ ] Add structure invariants requiring `components/controls/Slider.qml`, `components/controls/ToggleSwitch.qml`, `components/controls/SelectButton.qml`, and `components/controls/SearchField.qml` in the registered list and rejecting the three old form registrations and existing files.
- [ ] Reconfigure/build the affected integration and static tests with the old production source and run the focused tests; the new fixture/structure assertions must fail because Slider and moved paths do not yet exist.
- [ ] Commit the red contract tests with `git add` and `git commit -m "test(settings): specify slider and control ownership contracts"`.

### Task 2: Implement the reusable Slider

**Files:**
- Create: `Settings/qml/components/controls/Slider.qml`

- [ ] Import `QtQuick`, `QtQuick.Controls as QQC2`, and `../..` as `Components`; make the root `QQC2.Slider` with implicit size near 216 x 32 and a small `showEndpointGlyphs` property only if needed by the visual contract.
- [ ] Replace `background` with a compact item containing neutral rounded track, accent progress from the track start to `visualPosition`, and simple optional endpoint rectangles; derive colors, opacity, dimensions, and animation durations from `Components.Theme`.
- [ ] Replace `handle` with a compact solid rounded capsule and cheap offset shadow; position its center from `visualPosition` and the track geometry, without a position animation and without assigning an unnecessary delegate `id`.
- [ ] Ensure disabled opacity/colors are legible, hover/pressed changes are restrained, RTL uses `visualPosition`, and no raw pointer state or forbidden effect appears.
- [ ] Build the module/lint target and run the contract tests; they must pass before refactoring consumers.
- [ ] Commit with `git add Settings/qml/components/controls/Slider.qml && git commit -m "feat(settings): add Astrea slider control"`.

### Task 3: Register and migrate the control taxonomy

**Files:**
- Move: the three form control files into `Settings/qml/components/controls/`
- Modify: `Settings/qml/CMakeLists.txt`
- Modify: `Settings/qml/pages/appearance/Dock.qml`
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml`
- Modify: `Settings/qml/pages/system/Compositor.qml`

- [ ] Move the three files without changing their contents or public names.
- [ ] Replace the three old CMake entries with the three new paths and add `components/controls/Slider.qml`, yielding exactly 40 registered QML files.
- [ ] In Dock, add the Controls alias, remove the unused unqualified Controls import, and change only the nine `Slider` types plus existing ToggleSwitch/SelectButton types to `Controls.*`; preserve every existing binding and handler.
- [ ] In Wallpaper and Compositor, add the Controls alias and migrate only existing ToggleSwitch/SelectButton uses; leave Wallpaper stock Buttons and all other behavior unchanged.
- [ ] Run the focused structure, component, and Dock route tests, then source audits for stale form references/paths and raw unqualified Dock Slider blocks.
- [ ] Commit with `git add Settings/qml && git commit -m "refactor(settings): organize reusable controls"`.

### Task 4: Make documentation truthful

**Files:**
- Modify: `Settings/docs/COMPONENT_CATALOG.md`
- Modify: `Settings/docs/STRUCTURE.md`
- Modify: `Settings/docs/ARCHITECTURE.md`
- Modify: `Settings/docs/TESTING.md`

- [ ] Change the authoritative module count from 39 to 40 and update the catalog rows for Slider and the three moved controls.
- [ ] Document the explicit `controls/` and `form/` ownership rule in the directory, architecture, and catalog guidance.
- [ ] Document the preferred Qt Quick Controls derivation/delegate replacement rule and preserve the existing Basic-style and manual-visual-testing guidance.
- [ ] Replace stale 39-file/build directory references in Testing with the slider verification directory names and commands from the approved brief.
- [ ] Run the structure test and inspect the documentation diff for unrelated rewrites.
- [ ] Commit with `git add Settings/docs && git commit -m "docs(settings): describe slider control architecture"`.

### Task 5: Fresh full verification and review

**Files:**
- Review: all changed files and the final Git diff

- [ ] Configure/build/test Debug in `build/settings-slider-debug` with Ninja, Settings tests enabled, and LayerShell disabled.
- [ ] Build `astrea-settings-ui_qmllint` from the Debug directory and record registered/linted counts and warnings/errors.
- [ ] Configure/build/test Release in `build/settings-slider-release` with the same test options.
- [ ] Run the exact source audits: stale `Form.*` control references, stale old control paths, and raw Dock `Slider {` search; all must be empty for current source.
- [ ] Inspect the full diff as a reviewer for preserved Dock behavior, absence of persistence in Slider, no duplicate styling in Dock, no forbidden effects/input reimplementation, no unused imports, and correct count/docs.
- [ ] Provide the exact built `astrea-settings` executable path and the manual light/dark, two-accent, min/middle/max, hover/pressed, disabled, live-preview, alignment, and minimum-window checklist; do not claim visual parity from automation.
- [ ] Commit any review corrections separately, then run the affected verification command again before the final completion report.
