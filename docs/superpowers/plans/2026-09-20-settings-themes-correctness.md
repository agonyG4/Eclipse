# Settings Themes Correctness Pass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the focused Settings Themes correctness fixes while preserving the accepted QML/Rust/C++ architecture.

**Architecture:** QML remains presentation-only; SettingsThemesController remains a thin projection; Rust retains theme discovery, lookup, selection, and system icon-theme persistence; shared C++ retains Qt/QIcon integration. Rust and C++ coordinate theme.json read-modify-write transactions through the same sibling `theme.json.lock` advisory lock.

**Tech Stack:** Rust (`libc`, serde_json, tempfile), C++ Qt (`QSaveFile`, POSIX `flock`), QML, CMake/CTest.

## Global Constraints

- Preserve the current split-root catalog, first-valid-index authority, exact-before-closest lookup, directory/root ordering, extension ordering, independent Size/Scale matching, Example support, refresh reconciliation, sorting, and recoverable workers.
- Preserve `system_icon_theme` separately from legacy `icon_theme` and retain the established icon theme precedence.
- Run build and test commands from `/mnt/Aether/Desktop/GitHub`; keep generated output in its existing build folders.
- Use `rtk` for shell commands and do not use subagents.
- Commit the completed changes because this checkout is a Git repository.

## Implementation Tasks

### Task 1: Lock and atomically persist theme.json

**Files:** `Settings/backend/src/themes/config.rs`, `Settings/backend/src/themes/mod.rs`, `shared/theme/ThemeController.cpp`, `Settings/tests/unit/ThemeControllerTest.cpp`.

- [x] Add Rust tests for preserving all owned and unknown keys under ordered competing Rust/C++-style transactions, malformed JSON preservation, and lock errors.
- [x] Add C++ tests for atomic replacement, malformed JSON preservation, and blocking on the shared `.lock` path while another transaction owns it.
- [x] Implement Rust exclusive `flock` around read, JSON object validation, key patch/removal, same-directory temp write, file sync, rename, and parent sync.
- [x] Implement C++ exclusive `flock` on the same `<theme.json>.lock` path around latest-document read and owned-key patch; commit with `QSaveFile`.
- [x] Run focused Rust and C++ regression tests.

### Task 2: Tighten Freedesktop metadata and inheritance

**Files:** `Settings/backend/src/themes/catalog.rs`, `Settings/backend/src/themes/icon_lookup.rs`, `Settings/backend/src/themes/mod.rs`.

- [x] Add tests for ASCII internal-name validation, required `Size` on Scalable directories, optional metadata defaults, and explicit hicolor inheritance ordering.
- [x] Reject invalid theme IDs, require `Size` for all declared directories, and traverse explicit hicolor through the existing visited set before implicit fallback.
- [x] Run focused Rust catalog and resolver tests.

### Task 3: Refresh System Default preview URLs

**Files:** `Settings/qml/pages/appearance/Themes.qml`, `Settings/app/SettingsApplication.cpp`, `Settings/tests/integration/SettingsQmlSmokeTest.cpp`.

- [x] Add a QML smoke assertion that the System Default preview URL changes when the shared icon provider's `themeRevision` changes.
- [x] Bind the preview URL query string to `AstreaIconProvider.themeRevision` without changing global image caching.
- [x] Run the focused Settings QML smoke test.

### Task 4: Full requested verification and commit

**Files:** Existing Settings/shared CMake build and test configuration only if required to expose the requested existing tests.

- [x] Run `cargo fmt --check`, the backend Cargo tests, and backend Clippy with warnings denied.
- [x] Build Settings/shared and run the relevant complete CTest suite, including the named Rust, controller, provider, navigation, QML smoke, and structure coverage.
- [x] Review the final Rust/C++ transaction paths for a read-modify-write race, inspect the Git diff, and commit the changes.
