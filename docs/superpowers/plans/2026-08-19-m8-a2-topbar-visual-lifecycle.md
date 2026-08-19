# M8-A.2 TopBar Visual and Lifecycle Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the accepted Eclipse TopBar match the repository's Borealis semantic palette, restore popup enter animation without weakening close races, and make `BarSurfaceManager::shutdown()` terminal.

**Architecture:** Keep `astrea-shell`, `BarSurfaceManager`, one `BarSurfaceBundle` and one `BarPopupController` per output, `BarLayoutMetrics`, native `BarClockService`, and shared `ThemeController`. Palette values remain in production QML but are copied exactly from `Settings/qml/theme/Shell.qml` and `Apps.qml`; QML smoke tests instantiate production QML and assert semantic values. Manager lifecycle gains one terminal flag and disconnects application/controller signals during shutdown.

**Tech Stack:** C++20, Qt 6.6+, Qt Quick/QML, Qt Test, CMake, existing release build directory, offscreen QPA for deterministic QML tests.

## Global Constraints

- Preserve one Bar bundle and one popup controller per `QScreen`.
- Preserve `BarLayoutMetrics` as the only production geometry authority.
- Preserve native `BarClockService` and the horizontal date/separator/time layout.
- Do not add M8-B services, workspace sources, Quickshell, compositor CLI, Python, or JSON status bridges.
- Use exact Borealis values from `Settings/qml/theme/Shell.qml` and `Settings/qml/theme/Apps.qml`; do not invent palette values.
- Use production QML/QRC and production `BarSurfaceManager` lifecycle paths in tests.
- Preserve unrelated working-tree changes and commit only M8-A.2 implementation/report/plan files.

---

### Task 1: Add failing semantic palette and BarSegment tests

**Files:**
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `Bar/qml/components/ShellBarTheme.qml`
- Modify: `Bar/qml/components/BarSegment.qml`

**Interfaces:**
- Production `ShellBarTheme.qml` exposes the existing `shell*` properties plus `isDefault`, `shellSurfaceElevated`, and `shellBorderHover`.
- Test helper sets `ThemeController.themeMode` and `ThemeController.shellStyle`, then reads instantiated production QML properties.

- [ ] **Step 1: Write the failing test**

Add a table-driven QML smoke test for all six `(themeMode, shellStyle)` pairs. Assert exact `QColor` values for `shellBackground`, `shellSurface`, `shellBorder`, `shellBorderHover`, `shellHover`, `shellTextMain`, `shellTextSecondary`, and `shellSeparator` using the literal results from the existing Borealis QML formulas. Add a component test that instantiates `BarSegment`, reads its child Rectangle, and verifies normal fill/border, hover fill/border, pressed fill, and active fill through real mouse/property state.

- [ ] **Step 2: Run the focused QML test to verify it fails**

Run:

```bash
cmake --build build/release --target bar-qml-smoke-test -j2
ctest --test-dir build/release -R '^bar-qml-smoke-test$' --output-on-failure
```

Expected: failure from the current approximate palette and/or normal transparent BarSegment border.

- [ ] **Step 3: Implement the minimal semantic palette/component correction**

Port the exact six-combination formulas from `Settings/qml/theme/Shell.qml`, map the elevated surface to the existing `Apps.qml` popup background formula, and make `BarSegment` use `shellSurface`/`shellBorder` normally, `shellHover`/`shellBorderHover` on hover, `shellPressed` while pressed, and `shellActive` while active.

- [ ] **Step 4: Run the focused QML test to verify it passes**

Run the same build and CTest command; expect all palette and component assertions to pass.

---

### Task 2: Add failing popup enter/race tests

**Files:**
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `Bar/tests/BarCoreTest.cpp`
- Modify: `Bar/qml/PopupOverlaySurface.qml`

**Interfaces:**
- `PopupOverlaySurface.qml` exposes production-only test-readable properties `astreaEnterRunning` and `clockEnterRunning`, or equivalent object names/properties, without introducing a test implementation.
- `BarPopupController` retains `open`, `close`, `completeClose`, `surfaceRequired`, `closing`, and output-local ownership semantics.

- [ ] **Step 1: Write the failing tests**

Add QML assertions that opening a popup leaves the selected card at opacity `0` and scale `0.97` before the enter transition completes, then reaches opacity `1` and scale `1`. Add close-then-reopen-before-exit-completion and close-Clock-then-open-Astrea tests, asserting the new popup remains required after waiting beyond the old exit duration. Add a core test for output removal while a local popup is closing.

- [ ] **Step 2: Run the focused tests to verify they fail**

Run:

```bash
cmake --build build/release --target bar-core-test bar-qml-smoke-test -j2
ctest --test-dir build/release -R '^(bar-core-test|bar-qml-smoke-test)$' --output-on-failure
```

Expected: enter-state assertions fail because current open handling immediately sets opacity/scale to final values; race coverage identifies any stale completion behavior.

- [ ] **Step 3: Implement the minimal enter transition and stale-completion guard**

Add separate `ParallelAnimation` enter tracks for Astrea menu and Clock with initial opacity `0`, scale `0.97`, smooth easing, and durations aligned with existing `animationPopover`. On open, stop exit tracks, apply initial values, and restart only the selected enter track. On close, stop enter tracks and restart the selected exit track. Keep completion guarded by current `closing` and `kind`, so stopped/superseded animations cannot settle a newly opened popup.

- [ ] **Step 4: Run the focused tests to verify they pass**

Run the same focused build and CTest command; expect enter, close, reopen, kind-switch, and output-removal assertions to pass.

---

### Task 3: Add failing terminal-shutdown manager tests

**Files:**
- Modify: `Bar/tests/BarCoreTest.cpp`
- Modify: `Bar/platform/wayland/BarSurfaceManager.hpp`
- Modify: `Bar/platform/wayland/BarSurfaceManager.cpp`

**Interfaces:**
- `BarSurfaceManager::initialize()` remains idempotent before shutdown and returns `false` after terminal shutdown with a useful error.
- `BarSurfaceManager::shutdown()` is idempotent and transitions the manager into a terminal state.
- Application `screenAdded`, `screenRemoved`, geometry, and BarController enablement events cannot mutate a terminal manager.

- [ ] **Step 1: Write the failing post-shutdown tests**

Extend the production factory-seam lifecycle test: initialize, call shutdown, record bundle/signal state, emit `screenAdded`, emit `screenRemoved`, emit `geometryChanged`, toggle Bar enablement, call shutdown again, and assert no new bundle, no destruction, no geometry update, no signal count change, and no crash. Assert re-initialize is rejected after shutdown.

- [ ] **Step 2: Run the focused core test to verify it fails**

Run:

```bash
cmake --build build/release --target bar-core-test -j2
ctest --test-dir build/release -R '^bar-core-test$' --output-on-failure
```

Expected: current direct `screenAdded` after shutdown recreates a bundle, or re-initialization is incorrectly accepted.

- [ ] **Step 3: Implement terminal lifecycle state**

Add `m_stopped`. Set it before teardown, disconnect application signals and BarController enablement, clear/disconnect geometry callbacks, delete bundles, emit terminal state notifications once, and reject `initialize`, `addScreen`, `removeScreen`, geometry handling, and enablement synchronization after stop. Keep destructor shutdown safe.

- [ ] **Step 4: Run the focused core test to verify it passes**

Run the same build and CTest command; expect post-shutdown events and repeated shutdown to be harmless.

---

### Task 4: Clock fidelity cleanup and integrated validation

**Files:**
- Modify: `Bar/qml/components/Clock.qml`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `docs/M8-A.1_IMPLEMENTATION_REPORT.md`
- Modify: `docs/NATIVE_TOPBAR_M8A.md`
- Create: `docs/M8-A.2_IMPLEMENTATION_REPORT.md`

**Interfaces:**
- Clock remains native and horizontal; only low-risk Borealis typography/spacing/fade details are adjusted.
- Existing QRC, legacy guard, geometry, Spotlight, Settings, Shell runtime, and lifecycle tests remain unchanged except for required strengthened assertions.

- [ ] **Step 1: Add semantic Clock assertions before any visual adjustment**

Assert production Clock remains horizontal, date/time use the Borealis fallback font family where available, the date is secondary, the time is stronger, separator is 1x16, and minute transition uses the shared animation token.

- [ ] **Step 2: Run the affected suite**

Run:

```bash
cmake --build build/release --target astrea-shell bar-core-test bar-qml-smoke-test \
  shell-runtime-test theme-controller-test settings-qml-smoke-test \
  settings-component-smoke-test -j2
ctest --test-dir build/release -R \
  '^(bar-core-test|bar-qml-smoke-test|bar-qml-legacy-guard|shell-runtime-test|\
theme-controller-test|settings-qml-smoke-test|settings-component-smoke-test|spotlight-tests)$' \
  --output-on-failure
```

- [ ] **Step 3: Update the implementation report**

Document root causes, exact Borealis corrections, popup enter/close/race behavior, terminal shutdown, tests, commands/results, live-compositor limitations, and any unrelated build blockers without claiming unrun validation.

- [ ] **Step 4: Run repository-wide validation where configuration permits**

Run:

```bash
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure
git diff --check
```

If the existing uncommitted Paper work blocks CMake regeneration, record the exact missing source or target and run only direct generated-target validation as a limitation; do not modify Paper or claim a clean source checkout.

- [ ] **Step 5: Inspect and commit once**

Verify no Paper, Bench, unrelated root CMake, or unrelated planning files are staged. Stage only M8-A.2 implementation/tests/docs and commit:

```bash
git add Bar Shell Settings Spotlight shared docs/M8-A.2_IMPLEMENTATION_REPORT.md \
  docs/NATIVE_TOPBAR_M8A.md \
  docs/superpowers/plans/2026-08-19-m8-a2-topbar-visual-lifecycle.md
git diff --cached --check
git commit -m "fix(shell): close topbar visual and lifecycle gaps"
```

## Self-review checklist

- Palette formulas match the current Eclipse Borealis source exactly for all six combinations.
- No duplicate theme controller or geometry authority is introduced.
- Enter animations are separate from exit animations and current-kind/closing guards block stale completion.
- Manager shutdown disconnects future application/controller signals and rejects all mutation paths.
- Tests exercise production QML/QRC and the production manager factory seam.
- No M8-B behavior or unrelated workspace code is staged.
