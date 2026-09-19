# Settings Themes Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a production Settings Themes page that selects installed Freedesktop icon themes through a Rust/CXX-Qt backend, persists `system_icon_theme`, and feeds the existing shared Qt icon invalidation pipeline.

**Architecture:** Rust owns theme discovery, metadata, explicit preview lookup, selection validation, persistence, errors, and one bounded worker. `SettingsThemesController` is a thin generated CXX-Qt projection composed into `SettingsController.themes`; QML only presents projected data. Shared C++ keeps QIcon rendering and adds the persisted preference to the existing precedence chain while preserving `ThemeController` ownership boundaries.

**Tech Stack:** Rust 2024, standard library plus existing `serde_json`, CXX-Qt 0.10, Qt 6.8/CMake/Ninja, Qt Quick/QML, Qt Test, CTest, and `rtk` command proxy.

## Global Constraints

- Work directly on `main` in the current worktree; do not create a branch or worktree.
- Compile in the repository/build folder to avoid unnecessary SSD wear.
- Use the existing `Settings/backend` crate; do not add a second Rust crate.
- QML must remain presentation/interaction only and must not access files, environment variables, processes, IPC, or parse theme files.
- Rust owns the Themes domain; CXX-Qt is a thin QObject projection; shared C++ owns Qt/QIcon rendering integration only.
- `system_icon_theme` is canonical and intentionally distinct from legacy Settings-only `icon_theme`.
- Environment overrides retain precedence over persisted Settings state.
- Preview lookup must never mutate process-global `QIcon::themeName()`.
- Use one bounded worker and queue results onto the QObject Qt thread.
- Keep `ThemeController.iconAppearance` and its `default`, `monochrome`, and `tinted` semantics unchanged.
- Do not add Cursor, Sound, System, marketplace, download, install, remove, or import features.
- Preserve unrelated valid JSON keys and use atomic same-directory replacement for Rust writes.
- Commit each independently testable task, and use `rtk` for shell commands.

---

### Task 1: Establish Rust theme-domain test seams and catalog/config behavior

**Files:**
- Create: `Settings/backend/src/themes/mod.rs`
- Create: `Settings/backend/src/themes/catalog.rs`
- Create: `Settings/backend/src/themes/config.rs`
- Create: `Settings/backend/src/themes/state.rs`
- Modify: `Settings/backend/src/lib.rs`
- Test: Rust unit tests in the new modules

**Interfaces:**
- `catalog::ThemeCatalog::discover(search_roots: &[PathBuf]) -> Result<ThemeCatalog, ThemeError>` returns stable descriptors deduplicated by directory ID, with hidden themes excluded and split roots merged in search priority order.
- `catalog::ThemeDescriptor` contains `id`, `name`, `comment`, `inherits`, ordered directory metadata, merged roots, and user/system source information.
- `config::ThemePreferenceStore::load() -> Result<Option<String>, ConfigError>` reads only `system_icon_theme`; `save_selected(&str)` replaces only that key; `clear_selected()` removes only that key.
- `state::ThemeSelection::select(&mut self, theme_id: &str) -> Result<(), SelectionError>` accepts only a discovered visible non-`hicolor` theme; `select_system_default()` clears the selection.

- [ ] **Step 1: Write failing catalog tests**

Create temporary roots containing `index.theme` fixtures and assert:

```rust
#[test]
fn catalog_prefers_earlier_root_and_merges_split_content() { /* user metadata plus system icon directory */ }

#[test]
fn catalog_deduplicates_ids_and_hides_hidden_themes() { /* same ID in two roots, Hidden=true */ }

#[test]
fn catalog_parses_name_comment_inherits_directories_scaled_and_example() { /* exact descriptor fields */ }
```

Also add tests for missing/malformed metadata being skipped with a bounded error result and `hicolor` not appearing in user-facing descriptors.

- [ ] **Step 2: Run the focused tests and verify the expected missing-module failure**

Run: `rtk cargo test --manifest-path Settings/backend/Cargo.toml themes::catalog`

Expected: FAIL because the new module and test functions do not exist yet.

- [ ] **Step 3: Implement the minimal catalog and metadata parser**

Use `std::fs`, `PathBuf`, and a small INI-section parser. Search roots must preserve the shared pipeline order: `$HOME/.icons`, each XDG generic data location `/icons`, user Flatpak exports, and `/var/lib/flatpak/exports/share/icons`. Discover only direct child directories containing `index.theme`; merge metadata/content roots for duplicate IDs while retaining the first root’s metadata priority.

- [ ] **Step 4: Write failing config and selection tests**

Add tests for valid-key preservation, missing files, malformed existing JSON, atomic replacement, invalid selection rejection, and System Default:

```rust
#[test]
fn config_update_preserves_unrelated_keys() { /* retain legacy and unknown JSON members */ }

#[test]
fn config_update_is_atomic_and_clears_only_system_icon_theme() { /* no temp artifact after success */ }

#[test]
fn selection_rejects_undiscovered_theme_and_accepts_system_default() { /* explicit errors */ }
```

- [ ] **Step 5: Run the new config tests to verify RED**

Run: `rtk cargo test --manifest-path Settings/backend/Cargo.toml themes::config themes::state`

Expected: FAIL because persistence and selection APIs are not implemented.

- [ ] **Step 6: Implement conservative atomic persistence and selection state**

Parse a valid existing object and mutate only `system_icon_theme`. For a missing file start from an empty object; for malformed existing content return an error without overwriting it. Write a uniquely named file in the same directory, flush/close it, then rename it over the target. Clear the key for System Default. Keep canonical `hicolor` internal and reject it as a user selection.

- [ ] **Step 7: Run catalog/config/state tests and commit**

Run: `rtk cargo test --manifest-path Settings/backend/Cargo.toml themes`

Commit: `git add Settings/backend/src/themes Settings/backend/src/lib.rs && git commit -m "feat: add rust icon theme catalog and persistence"`

### Task 2: Add explicit preview lookup and bounded worker

**Files:**
- Create: `Settings/backend/src/themes/icon_lookup.rs`
- Create: `Settings/backend/src/themes/worker.rs`
- Modify: `Settings/backend/src/themes/mod.rs`
- Test: Rust unit tests in `icon_lookup.rs` and `worker.rs`

**Interfaces:**
- `icon_lookup::PreviewResolver::resolve(theme_id: &str, icon_name: &str, logical_size: u32) -> Option<PathBuf>` returns a local file path without changing global Qt state.
- `worker::ThemeWorker::new()`, `request_refresh()`, and `try_receive()` implement one bounded latest-refresh queue; repeated requests collapse deterministically and never create an unbounded thread set.

- [ ] **Step 1: Write failing preview tests**

Cover current-theme preference over inherited size, recursive inheritance, cycles, hicolor fallback, Fixed/Scalable/Threshold directories, `ScaledDirectories`, PNG/SVG/XPM, ordered base roots, and missing icons returning `None`.

```rust
#[test]
fn resolver_matches_fixed_scalable_threshold_and_scaled_directories() { /* representative files */ }

#[test]
fn resolver_follows_inheritance_and_hicolor_without_looping_on_cycles() { /* child -> parent -> child */ }
```

- [ ] **Step 2: Run the preview tests and verify RED**

Run: `rtk cargo test --manifest-path Settings/backend/Cargo.toml themes::icon_lookup`

Expected: FAIL because the resolver is absent.

- [ ] **Step 3: Implement the bounded explicit resolver**

Read only descriptor roots already discovered by Rust, traverse current theme before inherited themes, cap inheritance depth and visited IDs, match directory metadata according to Freedesktop type/size rules, and fall back to `hicolor`. Return only existing local paths. Do not call Qt or read process-global icon state.

- [ ] **Step 4: Write a failing worker bound test**

Submit many refresh requests and assert the queue retains at most one pending refresh and worker completion count is bounded by the initial active request plus the latest pending request.

- [ ] **Step 5: Implement one worker with deterministic latest-request coalescing**

Use one Rust thread and channels already available in the standard library. The worker performs catalog scan and preview preparation off the GUI thread, coalesces repeated refresh requests, and returns one result object per accepted generation.

- [ ] **Step 6: Run preview and worker tests and commit**

Run: `rtk cargo test --manifest-path Settings/backend/Cargo.toml themes::icon_lookup themes::worker`

Commit: `git add Settings/backend/src/themes && git commit -m "feat: resolve icon theme previews off the gui thread"`

### Task 3: Project the Rust backend through CXX-Qt and compose it

**Files:**
- Create: `Settings/backend/src/themes/qobject.rs`
- Modify: `Settings/backend/build.rs`
- Modify: `Settings/backend/src/lib.rs`
- Modify: `Settings/core/SettingsController.hpp`
- Modify: `Settings/core/SettingsController.cpp`
- Modify: `Settings/core/CMakeLists.txt`
- Test: `Settings/tests/unit/SettingsThemesControllerTest.cpp`
- Modify: `Settings/tests/CMakeLists.txt`

**Interfaces:**
- Generated type: `SettingsThemesController`.
- Properties: `QList_QVariant themes`, `QString selectedIconTheme`, `bool busy`, `QString lastError`.
- Invokables: `refresh()`, `setIconTheme(const QString &)`, `useSystemDefault()`.
- Rust projection descriptor keys: `id`, `name`, `comment`, `previewUrls`, `source`.

- [ ] **Step 1: Add failing backend projection tests**

Test default projection, initial refresh completion, selected card state, valid selection persistence, invalid selection error, System Default clearing, and that repeated refresh requests do not replace an active result nondeterministically.

- [ ] **Step 2: Run the focused C++ test and verify RED**

Run: `rtk ctest --test-dir build-settings -R settings-themes-controller-test --output-on-failure`

Expected: FAIL because the generated header/type and composition do not exist.

- [ ] **Step 3: Implement the thin CXX-Qt QObject**

Follow `Settings/backend/src/animation/qobject.rs`: declare the generated QObject with CXX-Qt properties/signals/invokables, keep Rust state in a focused struct, start one worker in `Initialize`, and use `self.qt_thread().queue(...)` for catalog results. The QObject must only convert Rust descriptors to `QList<QVariant>`/`QVariantMap`, emit notify signals, and surface bounded error text.

- [ ] **Step 4: Register the new Rust file and compose it into SettingsController**

Add `src/themes/qobject.rs` to `CxxQtBuilder::files`. Include the generated header from `SettingsController.hpp`, add `Q_PROPERTY(SettingsThemesController *themes READ themes CONSTANT)`, store a `std::unique_ptr`, and construct it alongside wallpaper, dock, and animations. Keep application composition native and do not expose filesystem details to QML.

- [ ] **Step 5: Add the C++ test target and run it**

Register `settings-themes-controller-test` using the existing reusable `astrea-settings-core` target. Run: `rtk ctest --test-dir build-settings -R settings-themes-controller-test --output-on-failure`.

- [ ] **Step 6: Commit the CXX-Qt integration**

Commit: `git add Settings/backend Settings/core Settings/tests && git commit -m "feat: expose settings themes controller"`

### Task 4: Preserve config ownership and add shared icon precedence

**Files:**
- Modify: `shared/theme/ThemeController.cpp`
- Modify: `shared/icons/AstreaIconTheme.cpp`
- Test: `Settings/tests/unit/ThemeControllerTest.cpp`
- Modify: `shared/tests/AstreaIconProviderTest.cpp`
- Modify any shared test CMake file needed for existing test registration

**Interfaces:**
- `ThemeController::save()` replaces only the keys owned by ThemeController in a valid existing JSON object and retains `system_icon_theme` plus unknown keys.
- `AstreaIconTheme::resolveWithSourceUnlocked()` resolves in order `ASTREA_ICON_THEME`, `QS_ICON_THEME`, persisted `system_icon_theme`, qt6ct, valid Qt/platform, WhiteSur-dark compatibility, hicolor.

- [ ] **Step 1: Write failing regression tests**

Add a ThemeController test that starts with `system_icon_theme` and an unknown key, saves unrelated Appearance state, and asserts both remain. Add shared tests for persisted resolution, ASTREA/QS overrides, invalid persisted fallthrough, and provider invalidation after replacing `theme.json`.

- [ ] **Step 2: Run the affected tests and verify RED**

Run: `rtk ctest --test-dir build-settings -R "theme-controller-test|astrea-icon-provider-test" --output-on-failure`

Expected: the preservation test fails because current `save()` reconstructs the object and the persisted precedence tests fail because the new branch is absent.

- [ ] **Step 3: Implement preservation in ThemeController::save()**

Read an existing valid object before writing, insert/replace the owned theme keys, and keep all other members. If the existing file is malformed, do not replace it with fabricated defaults; return without writing. Keep current reload semantics for complete valid replacements.

- [ ] **Step 4: Implement persisted precedence in AstreaIconTheme**

Read `system_icon_theme` from the valid JSON configuration path, validate it with the existing search roots, and add it after QS. Keep all environment and Qt/platform behavior intact. Do not alter `icon_theme` handling.

- [ ] **Step 5: Reuse the existing provider watcher and verify invalidation**

Ensure no new broadcast is introduced. The existing watched file/directory path still calls `apply()`, `clearCache()`, and increments `themeRevision` when Rust atomically replaces the file.

- [ ] **Step 6: Run affected tests and commit**

Run: `rtk ctest --test-dir build-settings -R "theme-controller-test|astrea-icon-provider-test" --output-on-failure`.

Commit: `git add shared/theme shared/icons Settings/tests && git commit -m "fix: preserve and consume persisted system icon theme"`

### Task 5: Add the native Themes route and QML page

**Files:**
- Create: `Settings/qml/pages/appearance/Themes.qml`
- Modify: `Settings/core/navigation/SettingsNavigationCatalog.cpp`
- Modify: `Settings/qml/CMakeLists.txt`
- Modify: `Settings/assets/i18n/en_US.json` if new translation keys are needed
- Test: `Settings/tests/unit/SettingsNavigationModelTest.cpp`
- Test: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`

**Interfaces:**
- Route: `themes`, parent `customization`, `sidebarVisible=false`, source `qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Themes.qml`.
- QML reads only `SettingsController.themes` and uses `Controls.SearchField`, `Form.ScrollPage`, `Form.FormCard`, and `Form.SectionHeader`.

- [ ] **Step 1: Extend navigation tests first**

Assert the new route source, child order `[appearance, themes, wallpaper, dock, animations]`, `parentId`, `sidebarVisible=false`, `containsNavigableId("themes")`, `sidebarAncestorForId("themes") == "customization"`, and Back/Forward behavior through the new destination.

- [ ] **Step 2: Run navigation tests and verify RED**

Run: `rtk ctest --test-dir build-settings -R settings-navigation-model-test --output-on-failure`

Expected: FAIL on child count/order and missing route.

- [ ] **Step 3: Add the catalogue entry and QML registration**

Insert Themes beside the other Customization children, retain stable IDs and native metadata, set `sidebarVisible=false`, and add `pages/appearance/Themes.qml` to `ASTREA_SETTINGS_QML_RELATIVE_FILES`.

- [ ] **Step 4: Build the page from existing visual primitives**

Use an approximately 900 px `Form.ScrollPage`. Render a Themes heading, Icon Theme section, search field, System Default card first, and filtered Rust descriptors in a responsive two-column grid. Each `FocusScope` card must expose an accessible button name, selected/focus/hover/pressed states, activate on click/Space/Enter, call `setIconTheme()` or `useSystemDefault()`, and never call filesystem or QIcon APIs. Render preview URLs through `Image` with empty slots tolerated.

- [ ] **Step 5: Add QML smoke coverage before declaring the page complete**

Test registration, route construction, System Default selected state, a discovered theme card selection, Space/Enter activation, search filtering, and bounded busy/error/empty UI. Use a fixture-friendly controller projection or the real controller with a temporary test environment; do not weaken existing smoke tests.

- [ ] **Step 6: Run navigation and QML tests and commit**

Run: `rtk ctest --test-dir build-settings -R "settings-navigation-model-test|settings-qml-smoke-test|settings-structure-test" --output-on-failure`.

Commit: `git add Settings/core/navigation Settings/qml Settings/assets/i18n Settings/tests && git commit -m "feat: add settings themes page"`

### Task 6: Update architecture documentation and complete validation

**Files:**
- Modify: `Settings/docs/ARCHITECTURE.md`
- Modify: `Settings/docs/MIGRATION_NOTES.md`
- Modify: `Settings/docs/TESTING.md`
- Modify: `Settings/README.md` if current route/backend inventory needs updating
- Test/verify: repository structural/static Settings tests

- [ ] **Step 1: Document the ownership split**

State explicitly that QML is presentation/interaction, Rust owns the Themes domain/backend, CXX-Qt is a thin QObject projection, and shared C++ is Qt/QIcon rendering integration. Document that legacy `icon_theme` remains Settings-only and `system_icon_theme` is canonical for the system icon pack.

- [ ] **Step 2: Run formatting and Rust validation**

Run:

```bash
rtk cargo fmt --check --manifest-path Settings/backend/Cargo.toml
rtk cargo test --manifest-path Settings/backend/Cargo.toml
rtk cargo clippy --manifest-path Settings/backend/Cargo.toml --all-targets -- -D warnings
```

Expected: all commands exit 0.

- [ ] **Step 3: Configure/build in the repository build directory**

Use the supported in-repository build directory and existing preset/options, for example:

```bash
rtk run "cmake -S . -B build-settings -G Ninja -DASTREA_BUILD_TESTS=ON -DASTREA_SETTINGS_BUILD_TESTS=ON"
rtk run "cmake --build build-settings --parallel"
```

- [ ] **Step 4: Run complete relevant CTest and structural suites**

Run: `rtk ctest --test-dir build-settings --output-on-failure`.

Review every failure rather than weakening a test. Confirm the Settings structural/static tests include the new QML file and route.

- [ ] **Step 5: Perform the explicit final review**

Check the diff and source for: Appearance saves preserving `system_icon_theme`; ASTREA/QS precedence; preview code not mutating QIcon; malformed/disappearing themes degrading safely; provider-based Shell/Dock/AltTab invalidation; expensive filesystem work staying off the GUI thread; no backend/system logic in QML; and unchanged legacy `icon_theme` behavior.

- [ ] **Step 6: Commit documentation and final verified state**

Run `rtk git diff --check` and `rtk git status --short`, then commit:

```bash
git add Settings/docs Settings/README.md
git commit -m "docs: document settings themes ownership"
```

After the commit, rerun the final status and report exact files changed, architectural decisions, commands/results, and any remaining limitations.
