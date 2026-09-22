# Settings Icon Theme No-Op Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make explicit System Default selection clear an unavailable persisted icon-theme preference while preserving optimistic selection and asynchronous persistence semantics.

**Architecture:** Add a private Rust-controller predicate that treats a pending request as the desired persisted intent, and otherwise requires both the configured preference and committed `ThemeSelection` to match before skipping. Exercise the state transition in Rust controller tests and the actual file removal/reappearance behavior through the existing Qt controller test target.

**Tech Stack:** Rust, CXX-Qt, Qt 6, Qt Test, Cargo, CMake/CTest.

## Global Constraints

- Do not redesign the Themes page, worker, persistence architecture, Freedesktop resolver, CXX-Qt bridge, or optimistic selection model.
- Keep `PendingSelection` generation semantics unchanged.
- Keep persistence asynchronous and preserve newest-request-wins, stale-completion, rollback, duplicate-signal, refresh, and loading behavior.
- All compilation, tests, benchmarks, generated build artifacts, and temporary build output must use `/mnt/Aether/Desktop/GitHub`.
- Before compiling, verify the effective output directory is under `/mnt/Aether/Desktop/GitHub` and not under `/home/agony/GitHub`.
- Do not address speculative transient in-flight theme writes or change `theme.json.lock`, atomic persistence, icon-provider invalidation, theme resolution, QML layout, navigation, environment precedence, or ownership boundaries.

---

### Task 1: Add the failing controller regression

**Files:**
- Modify: `Settings/tests/unit/SettingsThemesControllerTest.cpp`
- Modify: `Settings/backend/src/themes/qobject.rs` test module only, adding the Rust state-sequence test before production changes

**Interfaces:**
- Consumes: Existing `SettingsThemesController`, `ThemeTestEnvironment`, `ThemeSelection`, `PendingSelection`, `WorkerResult`, and refresh helpers.
- Produces: A reproducible failing regression for unavailable configured theme clearing and a Rust assertion of the intended controller state transition.

- [ ] **Step 1: Add the Qt end-to-end regression test declaration and body.**

Add `unavailableConfiguredThemeCanBeReplacedWithSystemDefault()` to
`SettingsThemesControllerTest` and implement this sequence:

~~~cpp
void SettingsThemesControllerTest::unavailableConfiguredThemeCanBeReplacedWithSystemDefault()
{
    ThemeTestEnvironment environment;
    QVERIFY(environment.isValid());
    QVERIFY(environment.createThemes());

    const QString configPath = environment.configPath();
    QVERIFY(QDir().mkpath(QFileInfo(configPath).path()));
    QFile config(configPath);
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(config.write(QJsonDocument(QJsonObject{
                            {QStringLiteral("system_icon_theme"), QStringLiteral("theme-b")}
                        })
                            .toJson())
            > 0);
    config.close();

    SettingsThemesController themes;
    QTRY_COMPARE_WITH_TIMEOUT(themes.property("selectedIconTheme").toString(),
                              QStringLiteral("theme-b"),
                              5000);

    QVERIFY(QDir(QDir(environment.home()).filePath(QStringLiteral(".icons/theme-b")))
                .removeRecursively());
    QVERIFY(QMetaObject::invokeMethod(&themes, "refresh", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("refreshing").toBool(), 5000);
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());

    QSignalSpy selectedThemeSpy(&themes, SIGNAL(selectedIconThemeChanged()));
    QVERIFY(selectedThemeSpy.isValid());
    QVERIFY(QMetaObject::invokeMethod(&themes, "useSystemDefault", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("busy").toBool(), 5000);
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());
    QCOMPARE(selectedThemeSpy.count(), 1);

    QVERIFY(config.open(QIODevice::ReadOnly));
    const QJsonObject persisted = QJsonDocument::fromJson(config.readAll()).object();
    QVERIFY(!persisted.contains(QStringLiteral("system_icon_theme")));

    const QString themeDirectory = QDir(environment.home()).filePath(QStringLiteral(".icons/theme-b"));
    QVERIFY(QDir().mkpath(themeDirectory));
    QFile index(QDir(themeDirectory).filePath(QStringLiteral("index.theme")));
    QVERIFY(index.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(index.write("[Icon Theme]\nName=theme-b\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n") > 0);
    index.close();
    QVERIFY(QMetaObject::invokeMethod(&themes, "refresh", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(!themes.property("refreshing").toBool(), 5000);
    QVERIFY(themes.property("selectedIconTheme").toString().isEmpty());
}
~~~

- [ ] **Step 2: Add the Rust controller state regression before changing production code.**

Add a test named
`unavailable_configured_selection_can_be_explicitly_cleared_without_reappearing`
near the existing refresh reconciliation tests. It must set configured
`theme-b`, refresh with `theme-b` present, remove `theme-b`, refresh with
the same configured preference, assert committed selection is `None` while
the configured preference remains `Some("theme-b")`, assert the no-op
predicate would not skip `None`, feed a successful
`WorkerResult::Persistence` for `selected: None`, then re-add `theme-b`
and refresh with `configured_selection: Ok(None)`, asserting the committed
and configured values remain `None`.

- [ ] **Step 3: Build the already-configured Settings test target in Aether and run the new test.**

Use an explicit, verified build directory:

~~~bash
test "$(realpath /mnt/Aether/Desktop/GitHub/Eclipse-build)" != "$(realpath /home/agony/GitHub/Eclipse)"
cmake -S /home/agony/GitHub/Eclipse -B /mnt/Aether/Desktop/GitHub/Eclipse-build -DASTREA_BUILD_TESTS=ON
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target settings-themes-controller-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^settings-themes-controller-test$' --output-on-failure
~~~

Expected result before the production correction: the new unavailable-theme
test fails because the System Default request is currently treated as a
no-op. The Rust test may also fail to compile until the predicate is added;
that is the intended red state for the missing production interface.

### Task 2: Refine only the no-op predicate

**Files:**
- Modify: `Settings/backend/src/themes/qobject.rs:119-125,381-385`

**Interfaces:**
- Consumes: `pending_selection`, `configured_selection`, and committed `ThemeSelection::selected()`.
- Produces: `selection_request_is_satisfied(Option<&str>) -> bool`, used only by `queue_selection_persistence()`.

- [ ] **Step 1: Add the minimal predicate implementation.**

Add this method to `impl SettingsThemesControllerRust`:

~~~rust
fn selection_request_is_satisfied(&self, selected: Option<&str>) -> bool {
    if let Some(pending) = self.pending_selection.as_ref() {
        return pending.selected.as_deref() == selected;
    }
    self.configured_selection.as_deref() == selected
        && self.selection.selected() == selected
}
~~~

Replace the existing effective-value comparison at the top of
`queue_selection_persistence()` with:

~~~rust
if self
    .rust()
    .selection_request_is_satisfied(selected.as_deref())
{
    return;
}
~~~

Leave generation allocation, `PendingSelection`, busy/signal calls, worker
submission, synchronous failure cleanup, and error handling unchanged.

- [ ] **Step 2: Run the focused Rust and Qt controller tests.**

~~~bash
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target cargo test --manifest-path Settings/backend/Cargo.toml themes::qobject::tests
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target settings-themes-controller-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^settings-themes-controller-test$' --output-on-failure
~~~

Expected result: all Rust controller tests and all Qt controller tests pass,
including the new unavailable configured-theme regression and the existing
true System Default no-op assertion.

- [ ] **Step 3: Review the diff for scope and formatting.**

Run:

~~~bash
git diff --check
git diff -- Settings/backend/src/themes/qobject.rs Settings/tests/unit/SettingsThemesControllerTest.cpp
~~~

Confirm no QML, worker, persistence, resolver, bridge, or generation logic was
changed.

### Task 3: Run complete validation and commit the logical code change

**Files:**
- Modify: `Settings/backend/src/themes/qobject.rs`
- Modify: `Settings/tests/unit/SettingsThemesControllerTest.cpp`

**Interfaces:**
- Consumes: The corrected predicate and focused regressions from Tasks 1–2.
- Produces: A verified Settings Icon Theme v1 semantic correction.

- [ ] **Step 1: Run the complete Rust Themes validation exactly as requested.**

Verify target placement first, then run:

~~~bash
test "$(realpath /mnt/Aether/Desktop/GitHub/Eclipse-settings-target)" != "$(realpath /home/agony/GitHub/Eclipse)"
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target cargo fmt --check --manifest-path Settings/backend/Cargo.toml
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target cargo test --manifest-path Settings/backend/Cargo.toml
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-settings-target cargo clippy --manifest-path Settings/backend/Cargo.toml --all-targets -- -D warnings
~~~

- [ ] **Step 2: Configure and build Settings/shared targets only into Aether.**

~~~bash
test "$(realpath /mnt/Aether/Desktop/GitHub/Eclipse-build)" != "$(realpath /home/agony/GitHub/Eclipse)"
cmake -S /home/agony/GitHub/Eclipse -B /mnt/Aether/Desktop/GitHub/Eclipse-build -DASTREA_BUILD_TESTS=ON
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target astrea-settings-core astrea-shared-core settings-themes-controller-test settings-qml-smoke-test theme-controller-test
~~~

- [ ] **Step 3: Run the relevant CTest suites.**

~~~bash
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^(settings-themes-controller-test|settings-qml-smoke-test|theme-controller-test|settings-structure-test)$' --output-on-failure
~~~

- [ ] **Step 4: Verify the worktree and commit only the code/test files for this correction.**

~~~bash
git diff --check
git status --short
git add Settings/backend/src/themes/qobject.rs Settings/tests/unit/SettingsThemesControllerTest.cpp
git commit -m "fix(settings): distinguish configured icon theme intent"
~~~

Do not stage the pre-existing unrelated modifications in the worktree.
