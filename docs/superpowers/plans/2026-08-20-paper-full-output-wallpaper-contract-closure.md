# Paper Full-Output Wallpaper Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every Paper wallpaper surface request the complete physical output while preserving Topbar/Dock usable-area reservations.

**Architecture:** Add a small `WallpaperSurfacePolicy` that returns the complete Background layer-shell contract, including explicit `exclusiveZone = -1`; make the real bundle consume it. Prove the client contract with a deterministic Qt test and prove Typhon's generic zone semantics with protocol tests, without changing Paper state ownership or shared helper defaults.

**Tech Stack:** C++20, Qt 6, QTest, CMake, wlr-layer-shell protocol, Typhon Rust integration harness.

## Global Constraints

- Preserve unrelated dirty work, build directories, caches, partial repaint, buffer age, fullscreen, Direct Scanout, and presentation history.
- Do not change Paper API/catalog/persistence or add per-output selection.
- Do not change generic Typhon layer-shell zone semantics.
- All documents are English; no native visual qualification is claimed without an Astrea session.

---

### Task E1: Add the failing Paper policy contract test

**Files:**
- Create: `Paper/tests/WallpaperSurfacePolicyTest.cpp`
- Modify: `Paper/CMakeLists.txt`

**Interfaces:**
- Test consumes `Paper::WallpaperSurfacePolicy::background(QScreen *)`.
- Test produces an exact field-by-field contract for Task E2.

- [ ] **Step 1: Write the failing test**

Assert `scope`, `Background`, `None`, four anchors, `exclusiveZone == -1`, `QMargins()`, and the supplied screen pointer.

- [ ] **Step 2: Run the test to verify it fails**

Run: `rtk run -c 'cmake --build build/no-layer-shell --target paper-surface-policy-test -j2'`  
Expected: FAIL to configure/compile because the policy and target do not exist.

- [ ] **Step 3: Implement only test registration**

Register the test target and source in `Paper/CMakeLists.txt`; do not add production behavior yet.

- [ ] **Step 4: Run the test to verify the intended failure**

Run: `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R '^paper-surface-policy-test$' --output-on-failure`  
Expected: compile failure naming the missing `WallpaperSurfacePolicy` interface.

- [ ] **Step 5: Commit boundary**

Task-owned commit, if commits are requested: `test(paper): lock full-output wallpaper layer policy`.

### Task E2: Add the minimal explicit WallpaperSurfacePolicy

**Files:**
- Create: `Paper/platform/wayland/WallpaperSurfacePolicy.hpp`
- Create: `Paper/platform/wayland/WallpaperSurfacePolicy.cpp`
- Modify: `Paper/CMakeLists.txt`
- Test: `Paper/tests/WallpaperSurfacePolicyTest.cpp`

**Interfaces:**
- Produces `static AstreaLayerShellConfig WallpaperSurfacePolicy::background(QScreen *screen = nullptr)`.

- [ ] **Step 1: Implement the exact policy**

Initialize scope, Background layer, None keyboard interactivity, all anchors, `exclusiveZone = -1`, zero margins, and the supplied screen.

- [ ] **Step 2: Run the focused test**

Run: `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R '^paper-surface-policy-test$' --output-on-failure`  
Expected: PASS.

- [ ] **Step 3: Commit boundary**

Task-owned commit, if commits are requested: `feat(paper): add explicit full-output wallpaper policy`.

### Task E3: Make WallpaperSurfaceBundle consume the policy

**Files:**
- Modify: `Paper/platform/wayland/WallpaperSurfaceBundle.cpp`
- Test: `Paper/tests/WallpaperSurfacePolicyTest.cpp`
- Test: `Paper/tests/WallpaperSurfaceManagerTest.cpp`

**Interfaces:**
- `WallpaperSurfaceBundle::initialize()` consumes `WallpaperSurfacePolicy::background(m_screen.data())`.

- [ ] **Step 1: Replace ad-hoc assignments**

Remove the local layer-shell field reconstruction and pass the policy result to `AstreaLayerShellHelper::configure()`.

- [ ] **Step 2: Run Paper surface tests**

Run: `rtk run -c 'cmake --build build/no-layer-shell --target paper-surface-policy-test paper-surface-manager-test -j2'`  
Then: `rtk run -c 'QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R "paper-surface-(policy|manager)-test" --output-on-failure'`  
Expected: PASS; manager still creates one bundle per current screen.

- [ ] **Step 3: Commit boundary**

Task-owned commit, if commits are requested: `fix(paper): extend wallpaper below exclusive shell surfaces`.

### Task E4: Add the generic Typhon full-output layer-shell regression

**Files:**
- Modify: `src/compositor/tests/layer_shell.rs`

**Interfaces:**
- Uses existing test-server helpers and generic layer-shell client protocol objects.
- Produces assertions for 1280x800 full Background geometry, 45px/72px positive reservations, creation-order independence, and 1600x900 resize.

- [ ] **Step 1: Write the failing regression**

Create a test helper that maps Topbar, Dock, and Background surfaces in both required orders. Set Background to all anchors and zone `-1`; assert its configure is full output while `capture_usable_output_geometry()` is reduced.

- [ ] **Step 2: Run the regression before any Typhon semantic change**

Run: `rtk cargo test --locked layer_shell --lib`  
Expected: the test passes against the already-correct generic semantics; if it does not, diagnose the test harness rather than altering zone semantics.

- [ ] **Step 3: Add resize assertions**

Send `SetOutputSize { width: 1600, height: 900 }` and assert the Background configure becomes 1600x900 while positive reservations remain workspace-only.

- [ ] **Step 4: Commit boundary**

Task-owned commit, if commits are requested: `test(layer-shell): cover full-output background beneath reservations`.

### Task E5: Strengthen Eclipse CI/source defense and documentation

**Files:**
- Modify: `tools/ci/tests/test_layer_shell_contract.py` only if its existing style can assert the policy source without duplicating the behavioral test.
- Modify: `docs/superpowers/specs/2026-08-20-paper-full-output-wallpaper-contract-design.md`
- Modify: `REPORT-2026-08-20-paper-full-output-wallpaper-contract-closure.md`

**Interfaces:**
- Behavioral proof remains the C++ policy test; source-contract CI is secondary.

- [ ] **Step 1: Add a narrow source check if justified**

Check for the policy’s explicit `exclusiveZone = -1` or registered policy symbol. Do not assert unrelated helper defaults.

- [ ] **Step 2: Run the CI check**

Run: `rtk run -c 'python3 tools/ci/tests/test_layer_shell_contract.py'`  
Expected: PASS or a documented pre-existing baseline failure.

- [ ] **Step 3: Record status and rejected alternatives**

Document the old config, final config, geometry distinction, tests, baseline, native-session limitation, and commit status.

- [ ] **Step 4: Commit boundary**

Task-owned docs/CI commit, if commits are requested: `docs(paper): record full-output wallpaper contract`.

### Task E6: Final Eclipse validation

**Files:**
- Validate all Task E1–E5 files and the existing Paper/Shell integration targets.

- [ ] **Step 1: Build focused Paper and shell targets**

Run: `rtk run -c 'cmake --build build/no-layer-shell --target paper-surface-policy-test paper-surface-manager-test astrea-shell astrea-settings -j2'`.

- [ ] **Step 2: Run focused CTest and diff checks**

Run: `rtk run -c 'QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R "paper-|settings-wallpaper" --output-on-failure'` and `rtk git diff --check`.

- [ ] **Step 3: Report native qualification honestly**

Only claim native full-output coverage if a real Astrea session supplies configure/visual evidence; otherwise record it as blocked.

## Execution record (2026-08-20)

- [x] E1–E3 implemented; the policy and surface-manager tests pass.
- [x] E4 implemented; the Typhon generic layer-shell regression passes in both creation orders and after resize.
- [x] E5 source defense passes **10/10** checks.
- [x] E6 focused Eclipse validation passes **9/9** CTest cases and all requested build targets.
- [ ] Native Astrea session qualification remains pending; no visual claim is made.
