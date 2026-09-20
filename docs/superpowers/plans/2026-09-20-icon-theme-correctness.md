# Settings Icon Theme Correctness Implementation Plan

> **For agentic workers:** Follow this plan task by task. This task is being executed in the current session without subagents, as required by the repository instructions.

**Goal:** Close Freedesktop correctness and reliability gaps in Settings icon-theme discovery, previews, worker refresh, and page states.

**Architecture:** Keep the accepted Rust/CXX-Qt ownership model. Rust catalogs and resolves themes off the GUI thread; CXX-Qt projects catalog, preview, error, and selected-preference state; QML presents the existing two-column card page and active QIcon preview path.

**Tech Stack:** Rust 2024, CXX-Qt, Qt 6/QML, CMake/CTest.

## Global Constraints

- Preserve the accepted architecture and do not redesign the feature.
- Keep `system_icon_theme` distinct from legacy `icon_theme`.
- Preserve precedence `ASTREA_ICON_THEME > QS_ICON_THEME > system_icon_theme > qt6ct > platform > compatibility > hicolor`.
- Preserve atomic Rust writes, unknown-key preservation, and `ThemeController::save()` behavior.
- Preserve the AstreaIconProvider watcher/cache invalidation pipeline and keep Rust catalog filesystem work off the GUI thread.
- Preserve `iconAppearance`, immediate card selection, keyboard accessibility, responsive two-column layout, navigation, and existing tests.
- Do not spawn subagents.
- Use `rtk`; run compile/test commands from `/mnt/Aether/Desktop/GitHub`.

---

### Task 1: Freedesktop catalog discovery and ordering

**Files:**
- Modify `Settings/backend/src/themes/catalog.rs`
- Test `Settings/backend/src/themes/mod.rs`

- [ ] Add red tests for an unindexed first root, first-index authority, and first-root icon override while retaining authoritative metadata.
- [ ] Run focused Rust tests and confirm the regressions fail.
- [ ] Collect every theme root in priority order, select only the first usable index metadata, reject themes with no usable index, default missing `Type` to Threshold, and return installed themes deterministically sorted.
- [ ] Run focused Rust tests again.

### Task 2: Two-phase icon lookup with size and scale

**Files:**
- Modify `Settings/backend/src/themes/icon_lookup.rs`
- Test `Settings/backend/src/themes/icon_lookup.rs`

- [ ] Add red tests for directory-first/root-second exact lookup, exact before closest, scale-specific matching, all directory distance boundaries, extension ordering, and safe icon names.
- [ ] Run focused Rust tests and confirm they fail.
- [ ] Implement exact lookup across roots, then closest-size distance with directory/root-order tie breaks; pass nominal size and scale separately through local lookup and inheritance.
- [ ] Run focused Rust tests again.

### Task 3: Worker preview examples, preferences, and startup failure

**Files:**
- Modify `Settings/backend/src/themes/worker.rs`
- Modify `Settings/backend/src/themes/qobject.rs`
- Test `Settings/backend/src/themes/worker.rs`
- Exercise controller refresh through `Settings/tests/integration/SettingsQmlSmokeTest.cpp`

- [ ] Add red tests for Example use/fallback, worker thread startup failure, and persisted preference changes across refreshes including disappearance, reappearance, and external System Default.
- [ ] Run focused Rust/QML tests and confirm the regressions fail.
- [ ] Read persisted selection in the worker, include it in worker snapshots, reconcile selection after each successful refresh, and return worker startup failures for safe `lastError` projection.
- [ ] Run focused Rust/QML tests again.

### Task 4: Themes page catalog states and default preview

**Files:**
- Modify `Settings/qml/pages/appearance/Themes.qml`
- Modify `Settings/assets/i18n/en_US.json`
- Test `Settings/tests/integration/SettingsQmlSmokeTest.cpp`

- [ ] Add an observable installed-catalog empty state and a representative default-card preview through `image://astrea-icon/`.
- [ ] Exercise empty-catalog and no-search-results visibility in the real loaded QML page; verify the default preview uses the active icon provider.
- [ ] Run the Settings QML integration test.

### Task 5: Full validation and commit

**Files:**
- All task files above

- [ ] Run Rust `fmt --check`, all backend tests, and all-target Clippy with warnings denied.
- [ ] Build and run the relevant Eclipse Settings and shared CTest suites using the repository-supported Qt/CXX-Qt configuration from `/mnt/Aether/Desktop/GitHub`.
- [ ] Rerun Settings static/structure tests and compare resolver behavior against the Freedesktop specification.
- [ ] Commit the verified implementation and report exact files and command results.
