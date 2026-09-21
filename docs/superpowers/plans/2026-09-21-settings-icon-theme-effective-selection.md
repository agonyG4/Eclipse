# Settings Icon Theme Effective Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Project the newest pending icon-theme selection immediately in QML while preserving committed-state rollback and newest-request-wins persistence.

**Architecture:** Keep `ThemeSelection` and `configured_selection` committed until a matching persistence success. Add an `effective_selected()` projection that prefers `PendingSelection.selected`; use it for the QML getter and all notification comparisons. Add a `refreshing` property derived from `refresh_busy`, leaving combined `busy` behavior intact while changing Themes.qml loading visibility to catalog refresh only.

**Tech Stack:** Rust, CXX-Qt, Qt Quick/QML, Qt Test, Cargo, CMake/CTest.

## Global Constraints

- Use the existing `PendingSelection` and generation machinery; do not redesign the worker, resolver, persistence locking, or QIcon integration.
- Preserve shared `theme.json.lock`, atomic persistence, malformed/unknown-key handling, split roots, metadata, lookup, inheritance, previews, revision, environment precedence, boundedness, reconciliation, and QML/backend separation.
- Never build inside `/home/agony/GitHub/Eclipse`; explicitly use `/mnt/Aether/Desktop/GitHub` for all build/test artifacts and verify the effective output directory first.
- Preserve unrelated user changes already present in the checkout.

---

### Task 1: Add failing Rust state-machine tests

**Files:**
- Modify: `Settings/backend/src/themes/qobject.rs` test module

**Interfaces:**
- Consume the existing `SettingsThemesControllerRust`, `PendingSelection`, `WorkerResult`, and `ControllerEffects` test seam.
- Produce executable assertions for effective projection, stale completion, rollback, signal-effect counts, and no-op selection.

- [ ] **Step 1: Add a test that a pending theme projects immediately and a newer pending value replaces it.**

Set a committed `theme-a`, enqueue pending `theme-b` and then `theme-c` through the controller state seam, and assert the effective helper/getter-equivalent values and one selected-change effect per change.

- [ ] **Step 2: Add a stale completion test whose stale success cannot replace the projected newest selection.**

Use committed `theme-a`, pending generation 3 `theme-c`, then deliver generation 1 `theme-b`; assert committed `theme-a`, projected `theme-c`, no selected-change effect, and pending generation 3.

- [ ] **Step 3: Add a matching failure test for rollback and error reporting.**

Deliver a matching `WorkerResult::Persistence` error while pending `theme-c`; assert pending is cleared, committed/effective selection returns to `theme-a`, selected-change and error effects are true, and `last_error` contains the persistence error.

- [ ] **Step 4: Add matching-success/no-op assertions.**

Assert a successful completion keeps the projected value without a duplicate selected-change effect, and selecting the already-effective value does not create another pending request.

- [ ] **Step 5: Run the focused Rust test before production changes.**

Run with an explicitly verified target directory:

```bash
test "$(realpath /mnt/Aether/Desktop/GitHub/Eclipse-target)" = /mnt/Aether/Desktop/GitHub/Eclipse-target
CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-target cargo test --manifest-path Settings/backend/Cargo.toml themes::qobject::tests
```

Expected: the new tests fail because QML/effective selection still reads only committed `ThemeSelection` and stale completion behavior is not yet corrected.

### Task 2: Implement effective projection and refresh distinction

**Files:**
- Modify: `Settings/backend/src/themes/qobject.rs`
- Modify: `Settings/qml/pages/appearance/Themes.qml`

**Interfaces:**
- Add CXX-Qt `refreshing` read-only property and `refreshingChanged()` signal backed by `refresh_busy`.
- Keep `selectedIconTheme`, `busy`, `lastError`, `setIconTheme`, and `useSystemDefault` signatures unchanged.

- [ ] **Step 1: Add `effective_selected()` and make `selected_icon_theme()` return the pending projection first.**

Return `PendingSelection.selected.as_deref()` when pending exists, otherwise `ThemeSelection::selected()`.

- [ ] **Step 2: Compare effective values around refresh and persistence completion.**

Capture the effective value before catalog replacement or pending removal; ignore stale completions before mutating committed state; on matching success commit and clear pending; on matching failure clear pending without mutating committed state; emit `selectedIconThemeChanged` only when effective values differ.

- [ ] **Step 3: Emit the immediate enqueue transition and rollback transition.**

Make `queue_selection_persistence` no-op when the requested optional ID equals the effective value. Otherwise install the pending request, emit the immediate selected change, and if request submission fails synchronously clear it and emit the rollback.

- [ ] **Step 4: Add `refreshing` property plumbing.**

Expose `refresh_busy` as `refreshing`, add its signal, and emit it exactly when refresh starts or finishes. Keep `busy` as `refresh_busy || pending_selection.is_some()`.

- [ ] **Step 5: Bind Themes.qml catalog copy and empty/error states to `refreshing`.**

Use `root.controller.refreshing` for “Loading installed themes…” and for hiding catalog/error states during actual refresh. Do not disable the page or selection cards for persistence.

- [ ] **Step 6: Run focused Rust tests and inspect the diff.**

Run the Task 1 command and `rtk diff`; expected result is the focused Rust tests pass and only the effective-selection/property paths are changed.

### Task 3: Update CXX-Qt and QML regression coverage

**Files:**
- Modify: `Settings/tests/unit/SettingsThemesControllerTest.cpp`
- Modify: `Settings/tests/integration/SettingsQmlSmokeTest.cpp` only if the existing Themes page fixture can assert the new `refreshing` state without duplicating backend coverage

**Interfaces:**
- Existing Qt property/method/signal names remain valid; add `refreshing`/`refreshingChanged` metadata assertions.

- [ ] **Step 1: Update the blocked persistence test for immediate theme projection.**

While the FIFO persistence operation is blocked, assert `selectedIconTheme == theme-c`, `busy == true`, `refreshing == false`, and exactly one selection signal; after success assert the value remains `theme-c` and no duplicate signal was emitted.

- [ ] **Step 2: Assert immediate System Default projection.**

Invoke `useSystemDefault()` and assert the selected property becomes empty and the signal count increments before waiting for persistence completion.

- [ ] **Step 3: Update rapid-request coverage for each newest projection and stale completion.**

Assert theme-a, then theme-b, then theme-c immediately after each invocation while the first persistence is blocked; after release assert theme-c remains selected, the persisted JSON is theme-c, and no stale completion adds a signal.

- [ ] **Step 4: Add a persistence-failure rollback assertion.**

Use the existing invalid-parent persistence setup or the Rust result seam to assert a pending theme is visible first, then the committed previous theme is restored with `lastError` populated.

- [ ] **Step 5: Add a QML loading-state assertion.**

During blocked selection persistence, assert the Themes page’s loading text is not visible while the controller remains busy; assert it is visible only during catalog refresh.

### Task 4: Verify all requested suites and commit

**Files:**
- No additional source files; update only tests/docs if verification requires a narrowly scoped correction.

- [ ] **Step 1: Resolve the existing Aether build/test entry points and print the verified output directories.**

Use existing CMake/build configuration under `/mnt/Aether/Desktop/GitHub`; do not create output under the checkout.

- [ ] **Step 2: Run the complete Rust, CXX-Qt, Settings QML, ThemeController, AstreaIconProvider, navigation, and structure suites.**

Record each exact command and its exit/result counts.

- [ ] **Step 3: Run final repository status/diff checks.**

Confirm unrelated pre-existing modifications remain untouched and the implementation/doc/test changes are limited to this task.

- [ ] **Step 4: Commit the completed logical change.**

```bash
git add docs/superpowers/specs/2026-09-21-settings-icon-theme-effective-selection-design.md \
        docs/superpowers/plans/2026-09-21-settings-icon-theme-effective-selection.md \
        Settings/backend/src/themes/qobject.rs \
        Settings/qml/pages/appearance/Themes.qml \
        Settings/tests/unit/SettingsThemesControllerTest.cpp \
        Settings/tests/integration/SettingsQmlSmokeTest.cpp
git commit -m "fix(settings): project pending icon theme selection"
```
