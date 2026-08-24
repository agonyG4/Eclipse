# P0 Spotlight Layer-Shell Crash Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove and fix the actual cause of Spotlight's full layer-shell remap and Astrea-menu Search crash paths, then qualify the real lifecycle without disturbing unrelated Typhon work.

**Architecture:** Use bounded runtime and Wayland-wire evidence to select the responsible authority. Keep protocol lifetime state in Typhon's existing `LayerSurfaceRole` authority, keep activation/readiness state at Eclipse's shared LayerShellQt/QML boundary, and serialize only the specific popup-to-Exclusive handoff if the trace proves it necessary. Add regressions at the boundary that fails, not speculative workarounds in both repositories.

**Tech Stack:** C++20, Qt 6/QML, LayerShellQt 6.4.5 minimum, Wayland client test fixtures, Rust stable, Cargo locked tests, CMake/Ninja, RTK command wrapper.

## Global Constraints

- Preserve every pre-existing dirty Typhon change; do not reset, restore, stash, clean, overwrite, or create a replacement worktree.
- Eclipse baseline is `eb9d7e47d80feaa86bc91365753a7c56458df084`; Typhon baseline is `9d3fb34b45f6ce4ffc4582c3231e220b3643e959`.
- Do not permanently keep Spotlight mapped invisibly, disable focus-loss closing, swallow Wayland protocol errors, add arbitrary sleeps/timers, recreate the window blindly, or broaden scope to Dock, Control Center, StatusNotifier/DBusMenu, Dwindle, KMS/O1, or direct scanout.
- Do not modify production code before a regression for the proven defect has failed against the current implementation.
- Use the existing Eclipse build directories and Typhon Cargo target cache; never run `cargo clean` or delete build directories.
- All new documentation must be English and must explicitly mark unavailable live qualification as `Not Run`.
- Before each commit, inspect `git status --short`, `git diff --stat`, `git diff --cached --stat`, `git diff --cached --name-only`, and `git diff --cached --check`; stage only P0 files and hunks.

---

### Task 1: Freeze repository state and map the current lifecycle

**Files:**
- Read: `/home/agony/GitHub/Eclipse/Spotlight/core/SpotlightController.{hpp,cpp}`
- Read: `/home/agony/GitHub/Eclipse/Spotlight/qml/Main.qml`
- Read: `/home/agony/GitHub/Eclipse/Spotlight/platform/wayland/LayerShellSurface.{hpp,cpp}`
- Read: `/home/agony/GitHub/Eclipse/shared/platform/wayland/LayerShellHelper.{hpp,cpp}`
- Read: `/home/agony/GitHub/Eclipse/AltTab/qml/Main.qml`
- Read: `/home/agony/GitHub/Eclipse/AltTab/core/AltTabController.cpp`
- Read: `/home/agony/GitHub/Eclipse/Bar/core/{BarController,BarPopupController}.{hpp,cpp}`
- Read: `/home/agony/GitHub/Eclipse/Bar/qml/{AstreaMenu,PopupOverlaySurface}.qml`
- Read: `/home/agony/GitHub/Eclipse/Bar/platform/wayland/BarSurfaceBundle.{hpp,cpp}`
- Read: `/home/agony/GitHub/Typhon/src/compositor/layer_shell.rs`
- Read: `/home/agony/GitHub/Typhon/src/compositor/protocols/layer_shell.rs`
- Read: `/home/agony/GitHub/Typhon/src/compositor/state/{input_resources,surface_focus,surface_commits}.rs`
- Read: `/home/agony/GitHub/Typhon/src/compositor/tests/layer_shell.rs`
- Read: `/home/agony/GitHub/Typhon/src/compositor/tests/layer_shell_lifecycle.rs`

- [x] Re-record both baselines with `rtk git -C <repo> rev-parse HEAD`, `status --short`, `diff --stat`, and `log --oneline -15`; save the output outside both repositories if needed.
- [x] Use the codebase graph for exact symbol lookup and call traces, then call coverage for every source path relied on; read any reported partial ranges directly.
- [x] Write a lifecycle table for each surface: initial commit, configure serials, ACK, buffer admission, mapped state, NULL-buffer unmap, remap, destroy, and keyboard focus eligibility.
- [x] Confirm whether the existing Typhon dirty hunks overlap the intended P0 lines; if they do, preserve the current hunk and make a minimal adjacent edit only after recording its exact pre-edit diff.

**Verification:** No files are modified in this task. The output is a baseline/evidence note used to select exactly one implementation branch.

### Task 2: Reproduce the crash and capture bounded causal evidence

**Files:**
- Read: existing Eclipse build outputs under `/home/agony/GitHub/Eclipse/build/`
- Read: existing Typhon binaries and test support under `/home/agony/GitHub/Typhon/target/`
- Create temporarily outside either repository: bounded trace/log files under a `mktemp -d` directory
- Modify: `/home/agony/GitHub/Eclipse/docs/P0_SPOTLIGHT_LAYER_SHELL_CRASH_REPORT.md` only after the causal result is known

- [x] Locate the existing `astrea-shell`, `astrea-spotlight`, and Typhon test/runtime binaries without rebuilding or deleting caches.
- [x] Run Case A through the existing Spotlight IPC: show, wait for active/mapped state, hide past the 190 ms close interval, prove the surface is unmapped, and show again; record shell PID, exit status, stderr, Typhon logs, and supervisor behavior.
- [ ] Run Case B with the Astrea popup Search action and record `surfaceRequired`, `closing`, `completeClose`, Spotlight map, focus ownership, and process lifetime in one monotonic timeline. **Not Run live:** no stable scoped input driver was available.
- [x] Run controls for Alt+Tab IPC and empty query/no-result open/close; record reopen-during-close, Bar popup, and weather-disabled live limitations as `Not Run` where unavailable.
- [x] Run one bounded `WAYLAND_DEBUG=client` reproduction when a real Wayland session is available; extract only object IDs, configure/ACK serials, buffer/null-buffer commits, keyboard enter/leave, and any protocol error preceding termination.
- [x] Inspect `coredumpctl`/`journalctl` or a bounded debugger backtrace if termination is SIGSEGV/SIGABRT; otherwise identify the precise protocol/disconnect/explicit-exit cause.

**Verification:** The report must state one exact terminal cause and whether Alt+Tab, Bar popup, weather-disabled, and search-backend controls reproduce it. If no live session exists, mark those checks `Not Run` instead of inferring behavior.

### Task 3: Add the failing regression at the proven authority

**Decision checkpoint:** Select exactly one branch from 3A, 3B, or 3C using Task 2 evidence. Do not add tests for an unproven branch merely because the code looks suspicious.

#### Task 3A: Typhon protocol/focus regression

**Files:**
- Modify: `/home/agony/GitHub/Typhon/src/compositor/tests/layer_shell_lifecycle.rs` or the existing focused layer-shell test module selected by the current fixture structure
- Modify: `/home/agony/GitHub/Typhon/src/compositor/tests/layer_shell.rs` only if an existing real-client helper must expose configure/map/focus state

- [ ] Add a real Wayland client test that maps an Overlay/Exclusive layer surface, commits NULL, remaps through a fresh bufferless commit/configure/ACK/buffer sequence, and repeats 100 cycles.
- [ ] Add negative assertions for buffer-before-new-configure, stale pre-unmap ACK, destroy while awaiting configure, and mapped Exclusive client disconnect when the failing trace involves those transitions.
- [ ] Add the two-surface Overlay/None then Overlay/Exclusive handoff and assert no stale focus owner, invalid enter/leave, client termination, or stale layer entry.
- [ ] Run the new tests before changing production code and confirm they fail for the reproduced defect rather than because of fixture setup.

**Run:** `rtk cargo test --locked layer_shell_lifecycle -- --exact layer_surface_remaps_after_null_buffer_unmap_for_one_hundred_cycles`, then run the same command with `exclusive_layer_surface_focus_clears_on_unmap_and_returns_on_remap` and `overlay_none_and_overlay_exclusive_handoff_has_no_stale_focus`. Expected: at least one regression fails against the current behavior when Typhon is responsible.

#### Task 3B: Eclipse Spotlight lifecycle/focus regression

**Files:**
- Modify: `/home/agony/GitHub/Eclipse/Spotlight/tests/static/LayerShellActivationStructureTest.cmake`
- Modify: `/home/agony/GitHub/Eclipse/Spotlight/CMakeLists.txt`
- Modify: `/home/agony/GitHub/Eclipse/Spotlight/qml/Main.qml` only after the structural regression is red
- Modify: `/home/agony/GitHub/Eclipse/AltTab/qml/Main.qml` and `/home/agony/GitHub/Eclipse/shared/platform/wayland/LayerShellHelper.cpp` for the same proven activation authority

- [x] Add a static regression for the proven duplicate/pre-ready activation behavior, enforcing one activation route, frame readiness, and lifetime-scoped activation for both Exclusive surfaces.
- [x] Run the structural regression before implementation and capture the red result (`found 2` activation routes) before changing production code.
- [x] Run the release focused tests after implementation; the existing controller close-timer behavior remained unchanged and passed.

#### Task 3C: Eclipse popup handoff regression

**Files:**
- Modify: `/home/agony/GitHub/Eclipse/Bar/tests/BarCoreTest.cpp`
- Modify: `/home/agony/GitHub/Eclipse/Bar/tests/BarQmlSmokeTest.cpp` only if the visual close-complete boundary is covered there
- Modify: `/home/agony/GitHub/Eclipse/Bar/core/{BarController,BarPopupController}.{hpp,cpp}` or the narrow QML/native handoff boundary only after the test is red

- [ ] Add a test that Search requests popup close, keeps the popup surface required while closing, and starts Spotlight only after native popup close completion. **Not selected:** direct Spotlight independently reproduced the crash.
- [x] Assert Settings was not changed or serialized.
- [ ] Run the focused Bar core/QML tests and confirm the handoff regression is red before implementation. **Not selected:** no popup defect was evidenced.

### Task 4: Implement the smallest causal fix

**Files:**
- Modify only the production files selected by Task 2 and the matching red regression
- Possible Typhon authority: `/home/agony/GitHub/Typhon/src/compositor/layer_shell.rs` and the directly-owned focus/commit module identified by the trace
- Possible Eclipse authority: `/home/agony/GitHub/Eclipse/Spotlight/core/SpotlightController.{hpp,cpp}`, `/home/agony/GitHub/Eclipse/Spotlight/qml/Main.qml`, and/or the narrow Bar handoff files

- [x] Typhon was not selected: its existing `LayerSurfaceRole` state and the live configure/ACK/buffer wire sequence were correct.
- [x] For the Eclipse defect, remove visibility-time and remap activation, disable LayerShellQt automatic activation, wait for `frameSwapped`, and issue one activation per window lifetime while preserving focus-loss close.
- [x] No popup serialization was added because the direct Spotlight path independently reproduced the crash.
- [x] Keep diagnostics bounded and outside the repository; no protocol errors are swallowed.
- [x] Run the focused red regressions and existing Spotlight tests successfully.

### Task 5: Add the English closure report and inspect the fixed trace

**Files:**
- Create/modify: `/home/agony/GitHub/Eclipse/docs/P0_SPOTLIGHT_LAYER_SHELL_CRASH_REPORT.md`
- Modify: `/home/agony/GitHub/Eclipse/docs/superpowers/specs/2026-08-24-p0-spotlight-layer-shell-crash-closure-design.md` only if the final evidence invalidates a stated design assumption

- [x] Document exact Eclipse/Typhon baselines, user reproductions, terminal crash cause, bounded wire sequence, Alt+Tab/Bar/weather/search controls, proven root cause, responsible layer, changed files, regressions, stress results, real-session status, and remaining limitations.
- [x] Independently compare the failing and fixed lifecycle traces; show the corrected configure/ACK/buffer and activation transition.
- [x] Review the final diff for no permanent invisible Exclusive surface, no arbitrary sleep, no duplicate state authority, no swallowed protocol errors, no disabled focus-loss UX, no stale-generation callback, and no unrelated refactor.

### Task 6: Run focused and broad verification

**Files:**
- No new source changes unless a verification failure identifies a regression covered by a new failing test

- [x] Eclipse: run Spotlight and activation-structure tests in the existing Release build; Alt+Tab IPC control remained alive. Broader unrelated suites were not required for this narrow Eclipse fix.
- [ ] Eclipse: build/test the changed targets in existing Debug, Clang, no-Typhon, no-layer-shell, ASan, and UBSan trees where configured; these additional matrices were not run in this closure.
- [ ] Typhon when modified: run `rtk cargo fmt --check`, `rtk cargo check --locked --all-targets`, `rtk cargo clippy --locked --all-targets -- -D warnings`, focused layer-shell/protocol/focus tests, `rtk cargo test --locked` when practical, `./bin/check-source-layout`, and `rtk git diff --check`.
- [x] Run the real Eclipse/Typhon 100-cycle Spotlight IPC stress when a runnable environment exists; verify the same shell PID survives, protocol errors are zero, and no stale Exclusive owner remains.
- [ ] Run repeated Astrea popup Search, Bar popup, reopen-during-close, and weather-disabled live controls; record unavailable paths as `Not Run`.
- [ ] Use `git diff --check` in both repositories and confirm the only staged paths are P0 paths before committing.

### Task 7: Commit only isolated P0 changes

**Files:**
- Stage only the final P0 paths in the responsible repository/repositories

- [x] Only Eclipse changed, so the existing design-doc commit was amended into one coherent Eclipse P0 commit with the final report, fix, tests, and plan docs; no unrelated file was staged.
- [x] No Typhon commit was warranted because no Typhon P0 file changed.
- [x] A second repository commit was not created because there was no independent Typhon defect.
- [x] After the commit, run `git show --format= --check HEAD`, `git status --short`, and inspect the remaining dirty Typhon paths to prove they are pre-existing and untouched.
