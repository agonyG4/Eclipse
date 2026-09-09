# Wallpaper-Backed Material Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor Appearance and Interface Style previews onto one wallpaper-backed `MaterialPreview` component while preserving existing Settings state ownership and leaving compositor rendering to a future Typhon backend.

**Architecture:** `Appearance.qml` will keep selection, interaction, and Theme mutation responsibilities while delegating preview visuals to `MaterialPreview.qml`. The page will read `SettingsController.wallpaper.effectivePreviewUrl`, `effectiveFit`, `stateName`, and `busy`; it will call only `refresh()` when no usable snapshot is present and no request is active. `MaterialPreview` will expose a renderer replacement seam and contain the transitional local fallback entirely.

**Tech Stack:** Qt Quick, Qt Quick Layouts, QML Image, Astrea Settings QML module, Settings wallpaper controller, existing fake Paper local-socket test harness, CMake/CTest, qmllint.

## Global Constraints

- Reuse the existing `build` directory and compile with `cmake --build build --parallel 4`.
- Do not create a wallpaper controller, filesystem lookup, polling timer, watcher, IPC endpoint, compositor API, blur shader, or Typhon protocol.
- Use `SettingsController.wallpaper.effectivePreviewUrl` as the primary preview source; do not use `effectiveSource` for normal preview rendering.
- Preserve Paper ownership of wallpaper identity/content/persistence and ThemeController ownership of theme state.
- Keep App Icon previews on `Shared.AstreaAppIcon` with the existing production renderer.
- Keep stable ChoiceCard object names and all current interactions.
- Do not use subagents; preserve unrelated dirty worktree changes.

### Task 1: Add failing MaterialPreview and Appearance wallpaper contracts

**Files:**
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`
- Modify: `Settings/qml/pages/appearance/Appearance.qml`

- [ ] Add tests for stable MaterialPreview object names, all Appearance/Interface cards sharing the same preview URL/fit, lightweight `wallpaper get` refresh, existing-snapshot reuse, busy suppression, live snapshot propagation, and fallback usability.
- [ ] Run the focused Settings QML test and confirm it fails because the MaterialPreview contract is not yet present.

### Task 2: Create the reusable MaterialPreview component

**Files:**
- Create: `Settings/qml/pages/appearance/MaterialPreview.qml`
- Modify: `Settings/qml/CMakeLists.txt`

- [ ] Expose wallpaper source/fit, theme variant, string material ID, optional renderer preview source/ready properties, and `usingRendererPreview`.
- [ ] Render one asynchronously loaded, cached wallpaper image with thumbnail-sized `sourceSize`, retaining the previous valid image while a new image loads.
- [ ] Map `cover`, `contain`, `stretch`, `center`, and `tile` consistently with Paper's existing fit semantics.
- [ ] Add quiet fallback layers and a centered showcase surface with correct Light/Dark or material-specific presentation.
- [ ] Keep all Transparent/Frosted approximation logic inside this component and document it as transitional pending Typhon ownership.

### Task 3: Refactor Appearance.qml onto MaterialPreview

**Files:**
- Modify: `Settings/qml/pages/appearance/Appearance.qml`

- [ ] Add a read-only wallpaper-controller projection and call `refresh()` only on first load when `stateName` is empty and `busy` is false.
- [ ] Replace embedded Appearance and Interface Style preview rectangles with `MaterialPreview` instances.
- [ ] Pass `auto`, `light`, and `dark` variants for Appearance and `default`, `transparent`, and `frosted` material IDs for Interface Style.
- [ ] Preserve App Icon preview rows, ChoiceCard layout/selection/keyboard behavior, and Theme mutation functions unchanged.

### Task 4: Document the future renderer seam

**Files:**
- Modify: `Settings/docs/ARCHITECTURE.md` or create a focused Settings Appearance architecture note.

- [ ] Document that Paper owns wallpaper source/state, MaterialPreview owns presentation structure, the QML fallback is transitional, and Typhon will eventually own canonical material rendering.
- [ ] State that no compositor preview transport is defined by this task and that Liquid Glass remains future-only.

### Task 5: Verify, audit, and commit the scoped changes

**Files:**
- Verify: changed Settings QML, tests, CMake registration, and documentation

- [ ] Run the required focused CTest regex, Settings QML lint, full CTest, documented Settings source audits, and `git diff --check`.
- [ ] Confirm the existing build directory was reused and report the Settings executable path plus remaining Typhon integration requirements.
- [ ] Commit only the files changed for this refactor; leave unrelated pre-existing dirty worktree changes untouched.
