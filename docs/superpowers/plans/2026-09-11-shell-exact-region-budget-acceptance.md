# Astrea Shell Exact-Region Budget Acceptance Implementation Plan

> **For agentic workers:** Execute this plan inline in the current session. Do not dispatch subagents.

**Goal:** Make Eclipse's 96-rectangle client budget a hard invariant with deterministic safe refusal, preserve same-frame animation synchronization, and complete fresh Eclipse/Typhon/native acceptance.

**Architecture:** Keep rectangle generation and deterministic segment-density reduction inside `AstreaBackdropEffectRegions::resolvedRegion()`. When the minimum rounded decomposition still cannot fit, return an empty region; the existing `sync()` path then disables the effect for that update without sending an oversized protocol region. No Typhon policy path changes are planned.

**Tech Stack:** C++/Qt 6/QML, CMake/CTest, Rust/Cargo, Wayland protocol integration, existing native Astrea Shell runtime.

## Global Constraints

- Preserve ordinary invalidation `scheduleSync() -> queued sync(true)`.
- Preserve `QQuickWindow::afterAnimating -> Qt::DirectConnection -> syncForCurrentAnimationFrame() -> sync(false)`.
- Never send more than 96 aggregate client rectangles.
- Preserve separate normal multi-card regions and do not drop arbitrary cards, merge into a giant bounding rectangle, or convert to fullscreen blur.
- Reuse `/home/agony/GitHub/Eclipse/build` and `/home/agony/GitHub/Typhon/target`; do not create alternate build trees.
- Use `rtk` for all requested Rust commands.
- Preserve unrelated existing worktree changes and stage only task-owned files in commits.

### Task 1: Add and observe the impossible-budget regression

**Files:**
- Modify: `shared/tests/BackdropEffectRegionsTest.cpp`

**Interfaces:**
- Consumes: existing `TestBackdropEffectRegions`, `AstreaBackdropRegion`, `resolvedRegion()`, and Qt Quick test fixture.
- Produces: a regression named `aggregateRegionBudgetRefusesUnrepresentableLayout` proving a non-deduplicating 33-card rounded layout refuses rather than exposing an oversized vector.

- [ ] **Step 1: Write the failing test**

Add this private slot after the existing four-card budget test:

```cpp
void aggregateRegionBudgetRefusesUnrepresentableLayout()
{
    QQuickWindow window;
    window.resize(720, 240);
    window.show();
    auto *descriptors = new TestBackdropEffectRegions(window.contentItem());

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 11; ++column) {
            auto *item = new QQuickItem(window.contentItem());
            item->setSize({40.0, 40.0});
            item->setPosition({20.0 + column * 60.0, 20.0 + row * 60.0});
            auto *region = new AstreaBackdropRegion(descriptors);
            region->setItem(item);
            region->setRadius(20.0);
            auto list = descriptors->regions();
            list.append(&list, region);
        }
    }

    descriptors->completeForTest();
    drainEvents();

    const auto rectangles = descriptors->resolvedRegion();
    QVERIFY(rectangles.size() <= 96);
    QVERIFY(rectangles.isEmpty());
}
```

- [ ] **Step 2: Build and run the focused test to verify RED**

Run:

```bash
cmake --build /home/agony/GitHub/Eclipse/build --target backdrop-effect-regions-test
ctest --test-dir /home/agony/GitHub/Eclipse/build -R '^backdrop-effect-regions-test$' --output-on-failure
```

Expected: the test executable builds, then the new test fails because the current implementation returns 99 rectangles after every descriptor reaches `segments = 1`.

- [ ] **Step 3: Commit the test-only change**

```bash
git add shared/tests/BackdropEffectRegionsTest.cpp
git commit -m "test: cover unrepresentable backdrop region budgets"
```

### Task 2: Enforce the hard budget with safe refusal

**Files:**
- Modify: `shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp`
- Test: `shared/tests/BackdropEffectRegionsTest.cpp`

**Interfaces:**
- Consumes: existing `buildRegion()` result and `segmentLimits` reduction loop.
- Produces: `resolvedRegion()` that returns either a vector with `size() <= 96` or an empty vector when the minimum decomposition is still oversized; one bounded warning per process for the refusal state.

- [ ] **Step 1: Add the minimal refusal branch**

In the existing `while (result.size() > maxClientRegionRectangles)` loop, keep the deterministic selection/decrement logic unchanged. Replace the current `break` when `selected < 0` with a one-time warning and `return {}`. Add only the required include(s) for the warning and one-time guard. The branch must not alter `m_enabled`, the animation connections, `requestFrame`, or ordinary successful geometry.

The resulting shape is:

```cpp
if (selected < 0) {
    static std::once_flag warningOnce;
    std::call_once(warningOnce, [&] {
        qWarning() << "Backdrop effect region budget cannot represent"
                   << result.size() << "rectangles; disabling this update";
    });
    return {};
}
```

- [ ] **Step 2: Run the focused test to verify GREEN**

Run:

```bash
cmake --build /home/agony/GitHub/Eclipse/build --target backdrop-effect-regions-test
ctest --test-dir /home/agony/GitHub/Eclipse/build -R '^backdrop-effect-regions-test$' --output-on-failure
```

Expected: all `backdrop-effect-regions-test` cases pass, including separate four-card coverage, scale/placement synchronization, unchanged-frame deduplication, and the new empty refusal case.

- [ ] **Step 3: Check the animation-path diff**

Run:

```bash
git diff HEAD~1 -- shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp
```

Expected: only the overflow branch and its includes/diagnostic are new; `Qt::DirectConnection`, `syncForCurrentAnimationFrame()`, and `sync(false)` remain unchanged.

- [ ] **Step 4: Commit the production fix**

```bash
git add shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp
git commit -m "fix: refuse oversized backdrop region updates"
```

### Task 3: Fresh Eclipse deterministic acceptance

**Files:**
- Verify: `/home/agony/GitHub/Eclipse/build`
- Verify: `shared/platform/wayland/effects`

**Interfaces:**
- Consumes: the existing configured Eclipse build directory and CTest registration.
- Produces: fresh build, full CTest output, focused test results, and source-policy evidence.

- [ ] **Step 1: Build the configured Eclipse tree**

Run:

```bash
cmake --build /home/agony/GitHub/Eclipse/build
```

Expected: exit code 0; no alternate build directory is created.

- [ ] **Step 2: Run the full CTest suite**

Run:

```bash
ctest --test-dir /home/agony/GitHub/Eclipse/build --output-on-failure
```

Record the exact passed/failed/skipped counts from this checkout.

- [ ] **Step 3: Run the focused Eclipse gates**

Run:

```bash
ctest --test-dir /home/agony/GitHub/Eclipse/build --output-on-failure -R '^(wayland-effects-state-test|backdrop-effect-regions-test|rounded-effect-region-test|dock-.*blur.*|bar-.*blur.*|spotlight-.*blur.*|alt-tab-.*blur.*|context-menu-.*blur.*|settings-.*effect.*)$'
```

If a name is not registered by the configured tree, list it with `ctest --test-dir /home/agony/GitHub/Eclipse/build -N`, run the exact registered target, and report the mapping rather than inventing a result.

- [ ] **Step 4: Run source-policy checks**

Run:

```bash
if rg -n 'wl_surface_commit\\s*\\(' shared/platform/wayland/effects; then exit 1; fi
if rg -n 'wl_display_connect\\s*\\(' shared/platform/wayland/effects; then exit 1; fi
```

Run the focused executable and record the refusal bound from the regression; do not infer a bound from the constant alone.

### Task 4: Fresh Typhon deterministic acceptance

**Files:**
- Verify: `/home/agony/GitHub/Typhon/target`
- Verify: current Typhon source and current committed/working-tree policy tests

**Interfaces:**
- Consumes: the current Typhon checkout and existing Cargo target directory.
- Produces: fresh formatter, compiler, clippy, focused test, full test, and qualification results.

- [ ] **Step 1: Run the requested Rust gates without changing policy code first**

Run exactly:

```bash
rtk cargo fmt --all -- --check
rtk cargo check --locked --all-targets
rtk cargo clippy --locked --all-targets -- -D warnings
rtk cargo test --locked effects
rtk cargo test --locked egl_renderer
rtk cargo test --locked native_output
rtk cargo test --locked
rtk bin/qualify-presentation --dry-run
```

Record fresh exit codes and test counts. Keep the current `target` directory.

- [ ] **Step 2: Run the focused Blur Policy/assignment regressions**

Use the current test names discovered from the checkout for Wayland Auto/RulesOnly/Disabled, XWayland RulesOnly/Disabled, Layer ClientOnly/Disabled, exact backend matching, client/rule precedence, partial opaque subtraction, Client Surface, Window/Auto VisualGroup, Layer Surface, effective `blur.status` counts, real Direct Scanout transition, startup invalid-config fallback, and transactional reload. Run the smallest Cargo filters that cover each named regression and record exact fresh results.

- [ ] **Step 3: Only if a current regression fails, trace and fix its root cause**

Do not alter Blur Policy semantics for a passing gate. If a fresh failure occurs, use the failing test and source data flow to identify the root cause, add or update the failing regression first, implement one minimal fix, rerun the focused gate, then rerun all Task 4 commands.

### Task 5: Native Frosted Shell acceptance

**Files:**
- Verify: current Typhon and Eclipse Shell binaries/runtime state
- Temporary diagnostics only if needed: the single Dock surface boundary logs named in the acceptance brief

**Interfaces:**
- Consumes: the current built Typhon compositor and Eclipse Shell with `shellStyle = Frosted`.
- Produces: explicit PASS/FAIL evidence for surface existence, exact geometry, same-frame animation, and state transitions.

- [ ] **Step 1: Start the current native stack in the existing runtime environment**

Set `shellStyle = Frosted`, start the current Typhon and Eclipse Shell, and verify the five visible surfaces: Dock, Bar, Spotlight, AltTab, and ContextMenu.

- [ ] **Step 2: Verify exact geometry**

Record PASS/FAIL for dockChrome only, magnification headroom unblurred, exact launcher/status pills, unblurred Bar fullscreen popup envelope, active popup card only, Spotlight panel only, AltTab panel only, ContextMenu cards only, and separate submenu/cascade cards.

- [ ] **Step 3: Verify animation and state transitions**

Record PASS/FAIL for Spotlight scale, AltTab scale, ContextMenu placement/scale, Default clear, Transparent clear, Frosted reapply, and hide/show recreate/reapply.

- [ ] **Step 4: Instrument only the first failing boundary if blur is completely absent**

Prefer Dock and record the bounded Eclipse values, Typhon protocol values, assignment values, and effects values from the acceptance brief. Reproduce once, identify the first boundary where expected state disappears, then remove or gate noisy temporary diagnostics and add a focused regression/fix only at that boundary.

- [ ] **Step 5: Commit any native-boundary repair and rerun affected gates**

Stage only the diagnostic cleanup/regression/fix files, commit them, and rerun the relevant deterministic and native checks before reporting acceptance.

## Final verification checklist

- [ ] Fresh Eclipse normal build exits 0.
- [ ] Fresh Eclipse full CTest output and requested focused tests are recorded.
- [ ] `shared/platform/wayland/effects` has no raw `wl_surface_commit()` or `wl_display_connect()`.
- [ ] 33-card regression proves `resolvedRegion().size() <= 96` and safe refusal.
- [ ] Fresh Typhon `rtk` gates and focused policy/assignment regressions are recorded.
- [ ] Native Frosted Shell exact-region, animation, and state checks are explicitly PASS/FAIL.
- [ ] No claim of full Liquid Glass support or 1080p165 performance qualification is made.
