# Legacy Wallpaper Settings Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the remaining functional, persistence, architecture, i18n, documentation, and test gaps in the native Eclipse port of the legacy Astrea Wallpaper Settings page without changing its approved visual hierarchy.

**Architecture:** Paper owns validation, managed-image publication, catalog metadata, persistence, and wallpaper selection. The Paper control socket exposes distinct catalog-only add and import-and-select actions. Settings owns only the native file/name dialogs, presentation state, and request serialization; QML never scans the filesystem or stores authoritative catalog data.

**Tech Stack:** C++20, Qt 6 Core/Gui/Network/Test, Qt Quick Controls 2, QML, QLocalSocket/QLocalServer, QSaveFile, JSON sidecar metadata, CMake/CTest.

## Global Constraints

- Work directly on the existing `main` branch and current worktree; do not create a branch or worktree.
- Preserve the approved Wallpaper visual geometry: content margin 28, 180×112 preview, 14 px preview radius, 12 px card radius, 3-column grids, 8 px gaps, and legacy section order.
- Paper remains the wallpaper and catalog authority; do not add Quickshell, QML `Process`, shell commands, `python3`, `zenity`, Typhon, Hyprland, awww compatibility, or QML-owned persistence.
- Preserve existing `wallpaper import` behavior and add a separate `wallpaper add` catalog-only operation.
- Keep stable content-addressed logical IDs and private digest filenames; display names are separate Paper-owned metadata.
- Do not implement renderer transitions, blur, per-workspace wallpaper, Screensaver, Lockscreen, dynamic execution, or the historical landscape asset migration.

---

### Task 1: Persist Paper user-wallpaper display metadata

**Files:**
- Modify: `Paper/core/WallpaperCatalog.hpp`
- Modify: `Paper/core/WallpaperCatalog.cpp`
- Test: `Paper/tests/WallpaperCatalogTest.cpp`

**Interfaces:**
- `WallpaperCatalog::importWallpaper(source, displayName, errorMessage)` accepts an optional user-facing name while retaining the existing source-only overload for compatibility.
- Metadata is stored as a versioned `<digest>.json` sidecar beside the managed image, written atomically with validated bounded text.

- [ ] Add name normalization/validation with a documented 128-character bound, Unicode preservation, and rejection of control characters.
- [ ] Read valid sidecars during refresh; use a deterministic localized-safe native fallback such as `Wallpaper` when metadata is missing or malformed, never the digest filename.
- [ ] Publish image and metadata transactionally enough that a failed metadata write leaves no new catalog entry.
- [ ] Preserve non-empty existing metadata on duplicate imports without a name, and update metadata deterministically when an explicit name is supplied without creating a second image.
- [ ] Add tests for refresh durability, reconstruction durability, duplicate content/name behavior, malformed metadata, display-name fallback, and failed metadata publication.
- [ ] Run `paper-catalog-test` and commit the Paper catalog change.

### Task 2: Add Paper catalog-only add semantics and transport

**Files:**
- Modify: `Paper/core/WallpaperService.hpp`
- Modify: `Paper/core/WallpaperService.cpp`
- Modify: `Paper/platform/ipc/WallpaperControlServer.cpp`
- Test: `Paper/tests/WallpaperServiceTest.cpp`
- Test: `Paper/tests/WallpaperControlServerTest.cpp`

**Interfaces:**
- Add `WallpaperService::addWallpaper(source, displayName)` returning a normal operation ID but never changing configured/effective wallpaper or generation.
- Extend import transport with optional `displayName` while keeping the existing import-and-select semantics.
- Add `wallpaper add {"path": ..., "displayName": ...}` with the same bounded command limits and operation completion behavior as existing mutations.

- [ ] Make catalog-only add refresh/reconcile the catalog and return the unchanged snapshot.
- [ ] Include the refreshed wallpaper list in mutation responses so Settings can update User Wallpapers without inventing state.
- [ ] Prove `wallpaper add` leaves configured ID, effective ID, and generation unchanged, while a subsequent stable-ID `wallpaper set` still selects normally.
- [ ] Prove `wallpaper import` still imports and selects and now transmits an optional display name.
- [ ] Add transport validation tests for Unicode, empty-name fallback, bounded names, and rejected control characters.
- [ ] Run `paper-service-test` and `paper-control-server-test`, then commit the Paper service/IPC change.

### Task 3: Close the Settings controller boundary

**Files:**
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.hpp`
- Modify: `Settings/services/wallpaper/SettingsWallpaperController.cpp`
- Test: `Settings/tests/unit/SettingsWallpaperControllerTest.cpp`

**Interfaces:**
- Add read-only `selectionFit` with precedence `configuredFit`, then `effectiveFit`, then `cover`.
- Add `importAndSelectWallpaper(path, displayName, fit)` and `addUserWallpaper(path, displayName)` invokables.

- [ ] Serialize the two operations to distinct Paper actions and transmit display names as JSON data.
- [ ] Reject duplicate requests while busy and surface a typed controller error without replacing the current snapshot.
- [ ] Remove the hardcoded C++ `Wallpaper` current-name fallback; an absent native name remains empty for QML localization.
- [ ] Keep semantic category projections based only on native descriptor metadata.
- [ ] Add tests for fit precedence/fallback, import/add request serialization, busy rejection, current-name precedence, empty native fallback, stable IDs, and list refresh payloads.
- [ ] Run `settings-wallpaper-controller-test` and commit the controller change.

### Task 4: Wire the approved QML page without visual redesign

**Files:**
- Modify: `Settings/qml/pages/appearance/Wallpaper.qml`
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp`
- Modify: `Settings/tests/static/SettingsStructureTest.cmake`

- [ ] Replace the dead import signal with a native `QtQuick.Dialogs.FileDialog`, preserving the preview and User Wallpapers visual affordances.
- [ ] Add the legacy 320×14 radius naming dialog with compact overlay, input, and button geometry; send the entered name through the controller, never retain it as authority.
- [ ] Route Change to import-and-select and `+` to catalog-only add; keep Screensaver/Lockscreen visibly present but non-clickable until routes exist.
- [ ] Disable conflicting mutation controls while busy and show compact in-page error/busy feedback using controller-authoritative `errorMessage`.
- [ ] Use canonical `apps.settings.pages.paper.wallpaper.*` keys for existing copy and remove the duplicate `appearance.wallpaper` namespace usage.
- [ ] Select library tiles with `controller.selectionFit`, never only `effectiveFit`; never fall back to logical IDs for labels.
- [ ] Add stable object names for picker, name dialog, name input, and add control; extend offscreen smoke assertions without opening a graphical portal.
- [ ] Extend the static source contract to reject every prohibited legacy mechanism listed in the brief.
- [ ] Run `settings-qml-smoke-test` and `settings-structure-test`, then commit the QML/test change.

### Task 5: Correct Settings navigation tests and documentation

**Files:**
- Modify: `Settings/tests/unit/SettingsNavigationModelTest.cpp`
- Modify: `Settings/README.md`
- Modify: `Settings/docs/MIGRATION_NOTES.md`
- Modify: `Settings/docs/COMPONENT_CATALOG.md`

- [ ] Update the navigation contract to expect the real Wallpaper route alongside the Compositor visual/local preview route.
- [ ] Document that Wallpaper is a native route backed by `SettingsWallpaperController` and Paper.
- [ ] Record deferred renderer/features and the separate full-landscape-library follow-up without broad documentation rewrites.
- [ ] Run `settings-navigation-model-test`, then commit the docs/navigation change.

### Task 6: Final scoped verification and visual self-review

**Files:**
- Review only the scoped diff from Tasks 1–5.

- [ ] Build the resolved Paper catalog/service/control targets, Settings controller/QML targets, and `astrea-settings` with the existing build tree.
- [ ] Run the focused Paper and Settings CTest subset and the repository static/source-layout checks.
- [ ] Run `git diff --check` and verify unrelated Bar/status-notifier worktree edits remain unstaged and unchanged.
- [ ] Compare the final QML against the canonical legacy source and re-check every approved geometry invariant.
- [ ] Report any pre-existing unrelated full-suite failures separately from the focused closure results.
