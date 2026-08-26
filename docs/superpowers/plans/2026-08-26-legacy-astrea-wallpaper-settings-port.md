# Legacy Astrea Wallpaper Settings Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the approved legacy AstreaOS Wallpaper settings page into Eclipse's native Settings shell without changing the Paper contract.

**Architecture:** Add native, read-only presentation projections to `SettingsWallpaperController`, derived from Paper's descriptor JSON. Rebuild the page in QML using the existing Settings theme and form components; supported image selection continues through Paper IPC while legacy-only controls remain visual and isolated.

**Tech Stack:** Qt 6 QML, Qt Quick Layouts/Controls/Effects, C++/Qt Test, CMake, Ninja, CTest.

## Global Constraints

- Work directly on `main`; do not create a branch, worktree, or temporary branch.
- QML owns presentation and interaction; C++ owns backend state, IPC, models, filesystem/system access, and projections.
- Do not add Quickshell, Quickshell.Io, QML `Process`, `python3`, `zenity`, shell commands, direct QML filesystem scans, legacy path construction, Typhon interaction, Hyprland behavior, awww, or QML-owned persistence.
- Keep Paper as the only wallpaper source of truth and do not modify its renderer or IPC contract.
- Preserve the legacy page hierarchy, copy, geometry, and control placement from `src/Apps/Settings/pages/paper/Wallpaper.qml`.
- Preserve unrelated local edits in `/home/agony/GitHub/Eclipse`; stage only files named by each task.

---

### Task 1: Add native wallpaper presentation projections

**Files:**
- Modify: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp` (test declaration and new socket-backed projection test)
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp` (four read-only properties and backing fields)
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.cpp` (response projection)

**Interfaces:**
- Consumes: Paper `snapshot.effective.displayName` and `wallpapers[]` descriptor fields `logicalId`, `kind`, `origin`, `displayName`, `resolvedSource`, and `source`.
- Produces: `QString currentDisplayName() const`, `QVariantList dynamicWallpapers() const`, `QVariantList userWallpapers() const`, and `QVariantList landscapeWallpapers() const`, each notifying through `snapshotChanged`.

- [ ] **Step 1: Write the failing test**

Add `projectsNativePresentationMetadata()` to the test class. The fake `wallpaper list` response must contain an effective descriptor named `Night Drive` and three catalog entries: one `kind=dynamic`, one `origin=user`, and one `origin=system, kind=image`. After `refreshLibrary()`, assert that `currentDisplayName()` is `Night Drive`, the three projection lists each contain one descriptor, and the projected user entry retains its `logicalId` and `resolvedSource`.

```cpp
void SettingsWallpaperControllerTest::projectsNativePresentationMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            auto state = snapshot(QStringLiteral("/tmp/night-drive.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            state.insert(QStringLiteral("effective"),
                         QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/night-drive")},
                                     {QStringLiteral("displayName"), QStringLiteral("Night Drive")},
                                     {QStringLiteral("source"), QStringLiteral("/tmp/night-drive.png")},
                                     {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/night-drive.png")},
                                     {QStringLiteral("fit"), QStringLiteral("cover")}});
            const QJsonArray entries{
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/dynamic")},
                            {QStringLiteral("kind"), QStringLiteral("dynamic")},
                            {QStringLiteral("origin"), QStringLiteral("system")},
                            {QStringLiteral("displayName"), QStringLiteral("Dynamic")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/dynamic.png")}},
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/user/blue")},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("user")},
                            {QStringLiteral("displayName"), QStringLiteral("Blue")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/blue.png")}},
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/mountain")},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("system")},
                            {QStringLiteral("displayName"), QStringLiteral("Mountain")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/mountain.png")}},
            };
            socket->write(QJsonDocument(QJsonObject{{QStringLiteral("ok"), true},
                                                    {QStringLiteral("completed"), true},
                                                    {QStringLiteral("snapshot"), state},
                                                    {QStringLiteral("wallpapers"), entries}})
                              .toJson(QJsonDocument::Compact)
                          + '\n');
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refreshLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.currentDisplayName(), QStringLiteral("Night Drive"));
    QCOMPARE(controller.dynamicWallpapers().size(), 1);
    QCOMPARE(controller.userWallpapers().size(), 1);
    QCOMPARE(controller.landscapeWallpapers().size(), 1);
    const auto user = controller.userWallpapers().constFirst().toMap();
    QCOMPARE(user.value(QStringLiteral("logicalId")).toString(), QStringLiteral("astrea://wallpaper/user/blue"));
    QCOMPARE(user.value(QStringLiteral("resolvedSource")).toString(), QStringLiteral("/tmp/blue.png"));
}
```

- [ ] **Step 2: Run the focused test and verify it fails for the missing projection**

Run: `rtk test cmake --build build-settings --target settings-wallpaper-controller-test && build-settings/Settings/tests/settings-wallpaper-controller-test projectsNativePresentationMetadata`

Expected: the build/test fails because the controller does not yet expose the new presentation properties.

- [ ] **Step 3: Implement the minimal native projection**

Add the four `Q_PROPERTY` entries and accessors. In `applyResponse`, parse the effective `displayName`, preserve the existing complete `m_wallpapers` list, then partition it into the three `QVariantList` fields using descriptor metadata. Use this classification order: `origin == "user"` → user list; otherwise `kind == "dynamic"` → dynamic list; otherwise `origin == "system" && kind == "image"` → landscape list. Resolve `currentDisplayName` from the effective descriptor, then the matching catalog descriptor, then `QStringLiteral("Wallpaper")`; leave it empty only when there is no effective descriptor so QML can use its localized `My Wallpaper` fallback.

- [ ] **Step 4: Run the focused test and verify it passes**

Run: `rtk test cmake --build build-settings --target settings-wallpaper-controller-test && build-settings/Settings/tests/settings-wallpaper-controller-test projectsNativePresentationMetadata`

Expected: the focused test exits 0.

- [ ] **Step 5: Commit the native projection**

```bash
rtk git add Settings/services/wallpaper/SettingsWallpaperController.hpp Settings/services/wallpaper/SettingsWallpaperController.cpp Settings/tests/unit/SettingsWallpaperControllerTest.cpp
rtk git commit -m "feat(settings): project wallpaper catalog categories"
```

---

### Task 2: Add the QML hierarchy regression test

**Files:**
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp` (wallpaper route assertions)

**Interfaces:**
- Consumes: the stable `wallpaperPage` route and `ScrollPage.contentMargins` property.
- Produces: object names `wallpaperScrollPage`, `currentWallpaperCard`, `wallpaperPreview`, `transitionCard`, `wallpaperLibraryCard`, `dynamicWallpapersSection`, `userWallpapersSection`, and `landscapesSection` for structural smoke coverage.

- [ ] **Step 1: Write the failing test**

After selecting the wallpaper route, assert the named page descendants exist and that `wallpaperScrollPage.contentMargins` equals `28`.

```cpp
QObject *page = root->findChild<QObject *>(QStringLiteral("wallpaperPage"));
QVERIFY(page != nullptr);
QObject *scroll = page->findChild<QObject *>(QStringLiteral("wallpaperScrollPage"));
QVERIFY(scroll != nullptr);
QCOMPARE(scroll->property("contentMargins").toInt(), 28);
for (const auto name : {"currentWallpaperCard", "wallpaperPreview", "transitionCard",
                        "wallpaperLibraryCard", "dynamicWallpapersSection",
                        "userWallpapersSection", "landscapesSection"}) {
    QVERIFY2(page->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
}
```

- [ ] **Step 2: Run the focused QML smoke test and verify it fails**

Run: `rtk test cmake --build build-settings --target settings-qml-smoke-test && build-settings/Settings/tests/settings-qml-smoke-test loadsWallpaperRouteOffscreen`

Expected: the test fails because the current generic page does not contain the legacy hierarchy names or 28 px margin.

- [ ] **Step 3: Keep the failing test uncommitted until the page implementation is ready**

The test intentionally stays with the working tree so `main` does not receive a committed red test. Task 3 will make the test pass and commit the test together with the restored page.

```bash
git status --short Settings/tests/integration/SettingsQmlSmokeTest.cpp
```

---

### Task 3: Rebuild the native Wallpaper page to legacy geometry

**Files:**
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml` (replace the temporary generic page)
- Modify: `Settings/assets/i18n/en_US.json` only if an existing translation key is required by the established controller; otherwise use `I18n.tr` fallbacks in the page

**Interfaces:**
- Consumes: `SettingsController.wallpaper.currentDisplayName`, `effectiveSource`, `effectiveId`, `busy`, `errorMessage`, `effectiveFit`, `dynamicWallpapers`, `userWallpapers`, `landscapeWallpapers`, and `selectWallpaper(logicalId, fit)`.
- Produces: the legacy current card, transition card, library card, three collapsible grids, preview hover state, and stable future-integration signals without legacy runtime dependencies.

- [ ] **Step 1: Replace the temporary generic page with the approved hierarchy**

Use `Form.ScrollPage` with `contentMargins: 28` and no arbitrary max width. Build:

```text
CURRENT
current card: 180x112 preview + details/toggles/buttons
transition card: SettingRow + 140 px SelectButton
1 px divider
WALLPAPER LIBRARY
library card: Dynamic / User / Landscapes sections with 3-column grids
```

Use `QtQuick.Effects.MultiEffect` masking for the 14 px preview/tile corners. Preserve the exact transition strings `Simple`, `Fade`, `Left`, `Right`, `Top`, `Bottom`, `Wipe`, `Wave`, `Grow`, `Center`, `Outer`, `Any`, `Random`; keep the selector local-only because Paper has no transition contract.

- [ ] **Step 2: Keep unsupported legacy controls truthful**

Render both toggles and both navigation buttons in their legacy positions. The all-workspaces toggle remains visually on and does not persist; the blur toggle remains visually off and does not persist. Preview Change and User Wallpapers `+` emit a local `wallpaperImportRequested()` signal rather than spawning a picker. Screensaver and Lockscreen emit `futurePageRequested("screensaver")` or `futurePageRequested("lockscreen")`; no numeric navigation or fake route is added.

- [ ] **Step 3: Connect supported library selection to Paper**

Each catalog tile uses its descriptor `logicalId` and `resolvedSource`. Only image descriptors are selectable through `controller.selectWallpaper(logicalId, root.controller.effectiveFit || "cover")`; no QML code derives categories or paths. User and system image entries remain visible even when their source is unavailable, showing the existing `No wallpapers found` message only for an empty category.

- [ ] **Step 4: Run the focused Settings tests**

Run: `rtk test cmake --build build-settings --target settings-wallpaper-controller-test settings-qml-smoke-test && ctest --test-dir build-settings -R 'settings-(wallpaper-controller|qml-smoke)' --output-on-failure`

Expected: both targeted tests pass with no QML warnings.

- [ ] **Step 5: Commit the page port**

```bash
rtk git add Settings/qml/pages/appearance/Wallpaper.qml
rtk git commit -m "feat(settings): restore legacy wallpaper page"
```

---

### Task 4: Run the full verification and review the diff

**Files:**
- Verify: `Settings/qml/pages/appearance/Wallpaper.qml`, controller files, and the two Settings tests

**Interfaces:**
- Consumes: all committed changes from Tasks 1–3.
- Produces: fresh build/test evidence and a scoped diff with no unrelated files staged.

- [ ] **Step 1: Build the configured Eclipse test targets**

Run: `rtk test cmake --build build-settings --parallel`

Expected: exit 0.

- [ ] **Step 2: Run the complete configured test suite**

Run: `rtk test ctest --test-dir build-settings --output-on-failure`

Expected: exit 0 with zero failed tests.

- [ ] **Step 3: Run static source checks for forbidden legacy mechanisms**

Run: `rtk rg -n 'Quickshell|Process|python3|zenity|wallpaper_manager|ASTREA_ROOT|XDG_|awww|Hyprland|Typhon' Settings/qml/pages/appearance/Wallpaper.qml`

Expected: no matches.

- [ ] **Step 4: Inspect the scoped diff and staged file list**

Run: `rtk git diff HEAD~3..HEAD -- Settings/qml/pages/appearance/Wallpaper.qml Settings/services/wallpaper/SettingsWallpaperController.hpp Settings/services/wallpaper/SettingsWallpaperController.cpp Settings/tests/unit/SettingsWallpaperControllerTest.cpp Settings/tests/integration/SettingsQmlSmokeTest.cpp`

Expected: only the wallpaper port, its native projections, and focused tests are present in the task commits; unrelated pre-existing changes remain unstaged.
