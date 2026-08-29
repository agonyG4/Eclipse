# Resolution-Aware Icon Pipeline Correctness Closure Implementation Plan

> **For agentic workers:** Execute this plan inline in the current Eclipse worktree. Do not dispatch subagents.

**Goal:** Close the review findings without changing the established resolution-aware icon architecture: let Qt own Freedesktop representation selection, prove its metadata semantics, make theme lookup priority correct, and document only verified runtime behavior.

**Architecture:** `AstreaIconProvider` will pass named icons directly to `QIcon::fromTheme()` and preserve the pixmap returned by Qt. A narrow recursive mutex will serialize Astrea-owned QIcon global theme configuration and named-icon lookup, while cache locks remain separate from slow rendering. `AstreaAppIcon` will use a per-window DPR bridge only if a real window-vs-screen mismatch is reproduced.

**Tech Stack:** Qt 6 `QIcon`/Freedesktop icon engine, `QQuickImageProvider`, Qt Quick/QML, Qt Test, CMake/CTest, Wayland, `rtk`.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse`; do not modify Typhon.
- Preserve the existing IconRenderRequest, shared QML policy, consumer behavior, bounded caches, revision invalidation, direct-file compatibility, and Dock geometry/input/reorder architecture.
- Do not use `QIcon::availableSizes()` to select named-theme representations.
- Preserve honest smaller-raster behavior; never fabricate detail by upscaling.
- Preserve existing user/concurrent worktree edits and stage only closure files.
- Use `rtk` for repository and verification commands; all new or modified Markdown is English.

---

### Task 1: Capture the primary Qt-selection regression

**Files:**
- Modify: `shared/tests/AstreaIconProviderTest.cpp`
- Modify: `shared/icons/AstreaIconProvider.cpp` only after the RED gate

**Interfaces:**
- The test calls `AstreaIconProvider::requestPixmap()` with `logicalSize`, `dpr`, and legacy `size` query fields.
- The provider returns raw physical pixels with deterministic provider-boundary metadata.

- [ ] **Step 1: Add a failing named-theme `Scale=2` test.**

Extend the fixture writer with `ScaledDirectories=48x48@2/apps`, explicit `Scale=1` and `Scale=2` sections, and red 48×48 / green 96×96 `scale-test.png` assets. Request `scale-test?logicalSize=48&dpr=1` and `scale-test?logicalSize=48&dpr=2`; assert red/48×48 and green/96×96 respectively.

- [ ] **Step 2: Run the focused test and confirm the expected RED failure.**

Run:

```bash
QT_QPA_PLATFORM=offscreen rtk run ctest --test-dir build/debug -R astrea-icon-provider-test --output-on-failure
```

Expected: the new `Scale=2` assertion fails because the current `availableSizes()` override collapses the nominal 48-pixel directories.

- [ ] **Step 3: Remove the post-resolution `availableSizes()` selection.**

Keep the initial `icon.pixmap(QSize(logical, logical), dpr, Normal, Off)` call, remove the metadata-blind candidate loop and manual downscale, and only set returned pixmap DPR to `1.0` at the provider boundary.

- [ ] **Step 4: Run the focused test GREEN.**

Build and run `astrea-icon-provider-test`; confirm the Scale=2 test and the existing smaller-raster test both pass.

### Task 2: Prove Qt-owned scalable, threshold, inheritance, and fallback semantics

**Files:**
- Modify: `shared/tests/AstreaIconProviderTest.cpp`
- Modify: `shared/docs/*` only if an existing English test/design document makes unsupported claims

**Interfaces:**
- Fixture helpers create valid XDG `index.theme` files and distinguishable raster/SVG assets.
- Tests use only `QIcon::fromTheme()` through the provider; no test helper resolves theme files manually.

- [ ] **Step 1: Add named scalable SVG coverage and run RED.**

Create a theme containing only `scalable/apps` with `MinSize=1`, `MaxSize=256`, and high-frequency SVG content. Assert `logicalSize=96,dpr=1` and `logicalSize=48,dpr=2` both return approximately 96×96 raw pixels. Run the focused provider test before changing production code; the test must fail against the current manual override.

- [ ] **Step 2: Add a Threshold fixture and assertions.**

Create neighboring fixed directories with `Type=Threshold`, `Threshold=4`, `Scale=1`, and distinguishable assets. Request inside and outside the threshold window and assert the representation chosen by Qt, without implementing a threshold algorithm in Eclipse.

- [ ] **Step 3: Add inheritance and current-theme-preference assertions.**

Create a child theme with an absent icon and `Inherits=parent`, proving the parent icon resolves. Create a current theme with a non-exact representation and a parent with an exact representation, proving the current theme wins once it contains the icon.

- [ ] **Step 4: Add real hicolor fallback coverage.**

Use a current theme that lacks the requested icon and a separate hicolor theme containing a distinguishable icon. Assert the provider returns the hicolor pixels through Qt fallback search paths.

- [ ] **Step 5: Run the provider test GREEN and inspect warnings.**

Run the focused provider test, then the existing Spotlight icon-theme tests. Fix fixture validity rather than weakening assertions if Qt rejects an index file.

### Task 3: Correct search-root priority and preserve Qt paths

**Files:**
- Modify: `shared/icons/AstreaIconTheme.cpp`
- Modify: `shared/tests/AstreaIconProviderTest.cpp`

**Interfaces:**
- `AstreaIconTheme::searchPathsFor(dataLocations, homePath)` remains deterministic and testable.
- `AstreaIconTheme::apply()` continues to merge Astrea roots with existing Qt theme/fallback paths.

- [ ] **Step 1: Add a failing priority test.**

Create the same theme/icon under the fake home `~/.icons` and an XDG data `icons` root, with different pixels. Assert `searchPathsFor()` lists `~/.icons` first and the provider resolves the user pixel.

- [ ] **Step 2: Run the focused test and confirm RED.**

Run `astrea-icon-provider-test`; the priority assertion must fail with the current standard-data-first order.

- [ ] **Step 3: Put `~/.icons` before XDG data roots.**

Retain `~/.local/share/icons`, all standard XDG data locations, user/system Flatpak exports, exact deduplication, and `apply()` preservation of native/resource paths.

- [ ] **Step 4: Run theme/provider regressions GREEN.**

Run `astrea-icon-provider-test` and the focused Spotlight provider-switch/inheritance tests.

### Task 4: Serialize Astrea-owned QIcon global state

**Files:**
- Modify: `shared/icons/AstreaIconTheme.hpp`
- Modify: `shared/icons/AstreaIconTheme.cpp`
- Modify: `shared/icons/AstreaIconProvider.cpp`
- Modify: `shared/tests/AstreaIconProviderTest.cpp`

**Interfaces:**
- A single shared synchronization boundary protects `AstreaIconTheme::apply()` and provider named-icon `QIcon::fromTheme()`/`pixmap()` calls.
- Cache locks are never held while acquiring the QIcon lock or doing filesystem/icon rendering.

- [ ] **Step 1: Add a deterministic concurrent stress test.**

Use a valid present icon and two valid themes. Run bounded parallel requests while the main test thread repeatedly changes the theme and clears the provider cache. Assert no crash, no corrupted returned image, and that each post-invalidation result matches the active theme.

- [ ] **Step 2: Run the stress test before synchronization and record its behavior.**

Run the provider test repeatedly or with its bounded iteration count. If it remains green, retain it as the regression guard; do not infer that unsynchronized QIcon mutation is safe from one run.

- [ ] **Step 3: Add the narrow shared mutex boundary.**

Expose a private/shared helper or mutex owned by `AstreaIconTheme`; lock only around `QIcon::setThemeName`, fallback/search-path mutation, and named-icon creation/rendering. Do not hold cache locks across this boundary.

- [ ] **Step 4: Run provider, Spotlight, and relevant QML tests GREEN.**

Confirm serialized lookups do not change current-theme, fallback, Scale=2, or scalable output.

### Task 5: Verify the DPR boundary and animation/mask invariants

**Files:**
- Modify: `shared/tests/AstreaAppIconQmlTest.cpp`
- Modify: `Dock/tests/DockHoverQmlTest.cpp` without staging the concurrent timeout hunk
- Add a narrow C++/QML test helper only if window DPR cannot be observed in the existing harness
- Modify relevant English docs only after measurements

**Interfaces:**
- QML source quality remains based on maximum logical extent and effective per-window DPR.
- Dock visual magnification changes transforms only; it never enters the provider request identity.

- [ ] **Step 1: Add a window-vs-screen DPR observation test.**

At the actual QQuickWindow boundary record QML `Screen.devicePixelRatio`, `QQuickWindow::devicePixelRatio()`, and `QQuickWindow::effectiveDevicePixelRatio()` at 1×, 2×, and supported fractional scale. Use the existing offscreen/window harness first; do not introduce a process-global DPR.

- [ ] **Step 2: Run the observation test and choose the smallest compatible policy.**

If screen and effective window DPR match for the supported path, document the measured result and retain the QML policy. If they differ, expose a narrow per-window DPR property and bind `AstreaAppIcon` to it.

- [ ] **Step 3: Strengthen the Dock test with actual hover traversal.**

Move through rest, neighbor influence, maximum center magnification, next icon, and exit. At each settled state assert `resolvedSource` and `effectiveSourcePixelSize` remain unchanged while `magnificationScale` changes. Preserve the user’s unrelated timeout edit unstaged.

- [ ] **Step 4: Perform the OpacityMask A/B check.**

Use the same high-resolution source at maximum magnification with `iconRadius=0` and the normal rounded mask. Compare interior detail, edge detail, and corner antialiasing. Keep `OpacityMask` unless measured interior degradation requires a targeted replacement.

### Task 6: Make documentation and verification truthful

**Files:**
- Modify: `Dock/docs/ARCHITECTURE.md`, `Dock/docs/RUNTIME_FLOW.md`, `Dock/docs/TESTING.md`
- Modify: `AltTab/docs/ARCHITECTURE.md`, `AltTab/docs/RUNTIME_FLOW.md`
- Modify: `Spotlight/docs/RUNTIME_FLOW.md`
- Modify: the relevant design/plan document only where its coverage claims are stale

**Interfaces:**
- Documentation describes Qt-owned representation selection, logical/physical sizing, cache invalidation, theme priority, and animation separation.
- Unexecuted live or platform-specific checks are marked designed/pending rather than verified.

- [ ] **Step 1: Update coverage claims after tests exist.**

List Scale=2, scalable SVG, Threshold, inheritance, current-theme preference, hicolor fallback, Flatpak search roots, DPR boundary, and animation checks only with their actual test names/results.

- [ ] **Step 2: Run source searches.**

Run separate `rtk run rg` commands proving production icon code has no `availableSizes()` selection and no old fixed source-size policy in Dock/AltTab/Spotlight.

- [ ] **Step 3: Run the complete feasible verification matrix.**

Run Debug and Release builds, focused provider/QML/Dock/AltTab/Spotlight tests, QML lint, 1×/1.5×/2× scale runs, and the broader CTest suite. Record unrelated failures separately, including the known Shell socket-environment failure if it persists.

- [ ] **Step 4: Review and commit only closure files.**

Use path-specific staging and interactive staging for `Dock/tests/DockHoverQmlTest.cpp`. Verify no concurrent statusnotifier, Bar, Settings, or Typhon changes are staged. Commit with:

```bash
rtk git commit -m "fix: close icon pipeline correctness gaps"
```

## Self-review checklist

- [ ] No production `availableSizes()`-based representation selection remains.
- [ ] Scale=2, scalable named SVG, Threshold, inheritance, current-theme preference, and hicolor fallback are real provider tests.
- [ ] `~/.icons` priority is tested and `~/.local/share/icons` remains included.
- [ ] QIcon global mutation/lookup synchronization is narrow and lock ordering is documented by structure.
- [ ] Smaller-only assets remain smaller; direct files still work.
- [ ] Window DPR behavior and mask A/B results are measured, not assumed.
- [ ] Documentation matches executed commands.
- [ ] Only Eclipse closure files are committed; concurrent user edits remain intact.
