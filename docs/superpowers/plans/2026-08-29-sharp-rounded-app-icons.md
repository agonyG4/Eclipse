# Sharp Rounded App Icons Implementation Plan

> **For agentic workers:** Execute inline in this session; subagents are explicitly disabled for this task. Steps use checkbox syntax for tracking.

**Goal:** Replace the proven rounded-icon OpacityMask bottleneck with a source-preserving rounded path and qualify it at real window DPRs.

**Architecture:** Keep `AstreaAppIcon`'s existing resolution-aware `Image` and source URL unchanged. A Loader-created analytic `ShaderEffect` samples that Image directly only for rounded icons. A correctly configured MultiEffect was measured first and rejected because it matched the legacy quality loss.

**Tech Stack:** C++20, Qt 6.8+ QML, Qt Quick ShaderEffect with Qt `.qsb` compilation, Qt Quick scene-graph captures, QImage metrics, CMake/CTest, rtk.

## Global Constraints

- Preserve `QIcon::fromTheme()` and `QIcon::pixmap(logicalSize, devicePixelRatio, ...)` selection.
- Preserve logical-size × DPR source sizing, maximum-presentation preloading, bounded requests/caches, theme inheritance, Threshold, Scale, scalable SVG, hicolor fallback, AltTab, Spotlight, and stable hover source requests.
- Do not use `QIcon::availableSizes()` or manual XDG representation matching.
- Do not modify Typhon, Dock magnification geometry, Layer Shell, input regions, reorder, context menus, or personalization behavior.
- Keep all Markdown in English and keep unrelated dirty worktree edits unstaged.

---

### Task 1: Make the validation tests truthful and failing

**Files:**
- Modify: `shared/tests/AstreaAppIconQmlTest.cpp`
- Modify: `shared/tests/AstreaIconProviderTest.cpp`
- Modify: `docs/superpowers/qualifications/2026-08-29-resolution-aware-icon-pipeline-final-validation.md`

**Interfaces:**
- The QML test will expose no new production provider interface; it will use the existing `AstreaAppIcon` properties and an inline test-only legacy component.
- The provider tests will expose `applyPreservesSplitThemeMetadataPriority()` and a separate duplicate-content selection test using production `AstreaIconTheme::apply()`.

- [x] **Step 1: Add the DPR-aware three-path capture test.**

  Remove `devicePixelRatioOverride = 1.0` from the real mask capture. Assert screen DPR, `QQuickWindow::devicePixelRatio()`, `QQuickWindow::effectiveDevicePixelRatio()`, and QML `effectiveDevicePixelRatio` agree before computing expected source pixels. Capture direct Image, inline legacy OpacityMask, and production rounded path using identical source, geometry, scale, and URL. Keep `maximumInteriorDifference` and contrast metrics, add edge-alpha checks, `hasProxySource`, and a `0.98` new/unmasked threshold plus required improvement over legacy.

- [x] **Step 2: Add light and dark deterministic source cases.**

  Run the same capture for dark high-contrast and light high-contrast SVG artwork. Keep the 12-pixel interior exclusion and report source pixels, all DPR values, all three contrast values/ratios, maximum differences, edge behavior, and proxy state.

- [x] **Step 3: Split the theme tests.**

  Rename the differing-metadata test to metadata priority and leave its user-only probe. Add a separate identical-`48x48/apps` metadata test with green user and red system duplicate content, invoke `AstreaIconTheme::apply()` and the provider, and report/assert the actual Qt-selected root without claiming both contracts are proven.

- [x] **Step 4: Run the new tests against the current implementation.**

  Run:

  ```bash
  QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wayland-1 \
    build/debug/shared/astrea-app-icon-qml-test opacityMaskPreservesInteriorDetailAtMaximumScale
  QT_QPA_PLATFORM=offscreen build/debug/shared/astrea-icon-provider-test
  ```

  Result: the DPR assertions exposed the expected source extents, the
  legacy-backed production rounded path failed the improvement/quality
  criterion, and the split tests identified Qt's actual duplicate-content
  behavior.

### Task 2: Evaluate the minimal MultiEffect replacement

**Files:**
- Modify: `shared/qml/AstreaAppIcon.qml` (evaluation only; not the final path)

**Interfaces:**
- Preserve every existing `AstreaAppIcon` property and resolved-source expression.
- Add only internal object names/diagnostic structure needed by the test; no provider or Dock API changes.

- [x] **Step 1: Configure direct Image and rounded mask texture.**

  A direct-Image MultiEffect configuration was evaluated while keeping the
  source Image's existing `source`, dynamic `sourceSize`, smoothing,
  mipmapping, retry, and readiness behavior. The final path does not need a
  mask texture.

- [x] **Step 2: Load MultiEffect only for rounded icons.**

  The attempted Loader-created MultiEffect was active only when the Image was
  ready and `iconRadius > 0`, with masking as its only effect. It reported no
  proxy source but did not meet the quality threshold, so the production
  Loader now creates the analytic ShaderEffect instead.

- [x] **Step 3: Preserve fallback and source invariants.**

  Keep fallback visibility tied to Image readiness, ensure the direct path has
  no active effect item, ensure rounded mode does not double-render, and
  confirm changing `scale` leaves `resolvedSource` and
  `effectiveSourcePixelSize` unchanged.

- [x] **Step 4: Run the focused QML test.**

  ```bash
  cmake --build build/debug --target astrea-app-icon-qml-test -j2
  QT_QPA_PLATFORM=wayland WAYLAND_DISPLAY=wayland-1 \
    build/debug/shared/astrea-app-icon-qml-test opacityMaskPreservesInteriorDetailAtMaximumScale
  ```

  Result: `hasProxySource` was false, but the MultiEffect output matched the
  legacy OpacityMask loss (`0.912873` contrast ratio at DPR 1), so it did not
  clear the quality threshold and was not retained.

### Task 3: Use the measured direct-sampling analytic shader

**Files:**
- Modify: `shared/qml/AstreaAppIcon.qml`
- Modify: `shared/tests/AstreaAppIconQmlTest.cpp`
- Modify: relevant CMake/QML shader resource declarations only if required by the existing Qt shader pipeline

**Interfaces:**
- Keep the same `AstreaAppIcon` properties and Image source.
- The fallback shader samples the existing high-resolution texture and multiplies source alpha by analytic rounded-rectangle alpha.

- [x] **Step 1: Confirm MultiEffect failure with the real capture.**

  The correctly configured MultiEffect was captured at DPR 1 for both source
  themes and remained at the legacy contrast ratio (`0.912873` for the dark
  case). The final ShaderEffect was then captured at DPR 1, 1.5, and 2 for
  both themes.

- [x] **Step 2: Implement the smallest direct-sampling shader.**

  Use the repository's Qt 6 shader build pipeline, premultiplied-alpha output, UV-space rounded-rectangle coverage, and no unrelated color/effect operations. Account for the source texture's atlas coordinates using the existing ShaderEffect conventions.

- [x] **Step 3: Extend the same metrics and edge assertions.**

  Require the shader path to meet the same threshold and source-request invariants. Do not add Qt Quick Effect Maker output unless it is necessary for the existing build.

### Task 4: Final verification and documentation

**Files:**
- Modify: `docs/superpowers/specs/2026-08-29-sharp-rounded-app-icons-design.md`
- Modify: `docs/superpowers/qualifications/2026-08-29-resolution-aware-icon-pipeline-final-validation.md`

- [x] **Step 1: Rebuild Debug and Release targets from the final tree.**

  ```bash
  cmake --build build/debug --target astrea-icon-provider-test astrea-app-icon-qml-test dock-hover-qml-test -j2
  cmake --build build/release --target astrea-icon-provider-test astrea-app-icon-qml-test dock-hover-qml-test -j2
  ```

- [x] **Step 2: Run focused, affected, and high-DPI tests.**

  Run the three named tests and affected Dock/AltTab/Spotlight tests at offscreen DPR 1, 1.5, and 2. Run the real mask test on Wayland at all three factors and record every requested metric.

- [x] **Step 3: Run QML lint/cache and inspect effect state.**

  ```bash
  cmake --build build/debug --target astrea-shell_qmllint astrea-settings-ui_qmllint -j2
  ```

  Confirm existing warnings are distinguished from errors, record
  `hasProxySource == false` for the rejected MultiEffect trial, and treat it
  as not applicable to the final ShaderEffect. No filesystem/provider/effect
  shader work is driven by hover frames in the tested source path; explicit
  filesystem/provider allocation counters were not instrumented.

- [x] **Step 4: Regenerate the qualification record.**

  Replace stale claims about `surfacePlacement()` with fresh final-tree results. Separate metadata priority from duplicate content-root behavior, include measured before/after mask numbers, record unavailable live-session checks honestly, and keep all prose in English.

- [x] **Step 5: Check and commit only scoped files.**

  ```bash
  git diff --check
  git add shared/qml/AstreaAppIcon.qml shared/tests/AstreaAppIconQmlTest.cpp \
    shared/tests/AstreaIconProviderTest.cpp docs/superpowers/specs \
    docs/superpowers/qualifications
  git commit -m "fix: replace rounded icon mask bottleneck"
  ```
