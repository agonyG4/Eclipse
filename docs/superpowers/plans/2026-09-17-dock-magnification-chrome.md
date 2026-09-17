# Eclipse Dock Magnification Chrome Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore animated primary-axis growth of the visible Dock chrome while the preallocated Layer Shell surface remains fixed.

**Architecture:** Keep `DockPanel`'s root `surfaceWidth`/`surfaceHeight` envelope stable. Publish the existing `updateHoverEffect()` `totalExtra` through one mutable `magnificationExtraPrimary` property, and bind only the real `dockChrome` primary dimension to that value. Existing delegate transforms, pointer geometry, backdrop targeting, input-region publication, material palette, and drag coordinates remain unchanged.

**Tech Stack:** Qt 6 QML, C++ QtTest/QQuickWindow tests, CMake/Ninja release build, `rtk` command wrapper.

## Global Constraints

- Do not modify `/home/agony/GitHub/Typhon`.
- Do not modify Dock runtime task-identity behavior.
- Do not resize the outer Layer Shell surface during pointer movement, animation, mode changes, or drag transitions.
- Use exactly one authoritative current primary-axis chrome expansion value, assigned from `totalExtra` in `updateHoverEffect()`.
- Do not change `primaryExtent()`, pointer coordinate helpers, delegate resting-center calculations, or `updateDelegateTransforms()`.
- Keep `Behavior on width` and `Behavior on height` with the current duration/easing policy.
- Keep `BackdropRegion { item: dockChrome; enabled: dockChrome.visible; radius: dockChrome.radius }` and do not modify shared-effects production code unless a failing test proves it is required.
- Keep shared Shell material bindings and do not restore legacy hard-coded material colors or overlays.
- Build in the existing repository-local `build/release` directory.
- Use strict RED → GREEN and verify exact test outcomes before claiming completion.
- Commit changes in the Eclipse Git repository; do not include unrelated existing history or Typhon changes.

## Files and responsibilities

- Modify `Dock/tests/DockHoverQmlTest.cpp`: encode endpoint semantics for horizontal/vertical chrome growth, backdrop tracking, input masking, drag collapse, capacity, and non-magnification modes/configuration refreshes.
- Modify `Dock/qml/components/DockPanel.qml`: add the single primary-axis expansion state, assign it from `totalExtra`, and bind `dockChrome` dimensions by orientation.
- Inspect only `shared/platform/wayland/effects/AstreaBackdropEffectRegions.cpp` and `.hpp`: confirm item geometry/change and after-animation synchronization already support dynamic `dockChrome`; do not edit unless RED tests expose a real capability gap.
- Create and retain `docs/superpowers/specs/2026-09-17-dock-magnification-chrome-design.md` and this plan as the documented design/implementation record.

### Task 1: Add the failing chrome/backdrop/input/transition assertions

**Files:**
- Modify: `Dock/tests/DockHoverQmlTest.cpp` in the existing hover, backdrop, input, drag, mode, and vertical tests.

**Interfaces:**
- Consumes: existing `DockPanel` properties/functions and existing `DockInputRegionBridge`, `FakeInputRegionBridge`, and backdrop-region test helpers.
- Produces: assertions that fail against the current static-chrome implementation but pass once `magnificationExtraPrimary` drives the real chrome.

- [ ] **Step 1: Update the horizontal surface test as a RED regression.**

  In `surfaceEnvelopeRemainsStableDuringMagnification()`, keep the initial
  envelope assertions, replace the current hover assertion that waits for
  `chrome->width() == restingWidth` with an endpoint assertion that waits for
  `panel->property("magnificationExtraPrimary") > 0` and then waits for
  `chrome->width() > restingWidth`. Assert `chrome->height()` remains the
  resting height, `chrome->width() <= panel->width()`, and both panel
  dimensions are unchanged. After `setPointerInside(false)`, wait for
  expansion to return to zero and chrome width to return to resting width,
  while asserting panel dimensions remain unchanged.

  The core new checks should be equivalent to:

  ```cpp
  QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0,
                           1500);
  QTRY_VERIFY_WITH_TIMEOUT(chrome->width() > restingWidth, 1500);
  QCOMPARE(chrome->height(), panel->property("restingHeight").toReal());
  QVERIFY(chrome->width() <= panel->width());
  QVERIFY(qAbs(panel->width() - initialSurfaceWidth) < 0.1);
  QVERIFY(qAbs(panel->height() - initialSurfaceHeight) < 0.1);
  ```

- [ ] **Step 2: Update vertical left/right coverage.**

  In `verticalPositionsReusePrimaryAxisGeometry()`, capture fixed panel
  dimensions and resting chrome dimensions before hover. After the center
  hover, wait for `magnificationExtraPrimary > 0` and `chrome->height() >
  restingHeight`; assert chrome width is still resting width and panel width
  and height are unchanged. Keep all existing left/right indicator-side
  assertions. After clearing pointer hover, wait for zero expansion and
  resting chrome height.

- [ ] **Step 3: Replace the backdrop regression expectation.**

  Rename the test declaration/definition to
  `backdropRegionTracksMagnifiedChromeInsideStableSurface()`. Capture resting
  chrome geometry, fixed surface geometry, and the resolved region. Hover the
  center delegate, wait for positive expansion and settled chrome growth, then
  assert the resolved region differs from resting, its bounding rectangle
  follows the chrome's mapped geometry, it is contained by the panel surface,
  and it remains non-empty. Retain a rounded-region semantic check by ensuring
  a point in the surface's unused corner is absent from the resolved region.
  Clear hover, wait for zero expansion/resting chrome dimensions, and assert
  the resolved region returns to its captured resting value and the surface
  remains unchanged. Do not assert any exact intermediate animation frame.

- [ ] **Step 4: Correct dynamic input-mask expectations.**

  In `inputMaskTracksCenteredChromeAndMagnifiedIcon()`, after hover wait for
  chrome width to exceed resting width and use the post-hover mapped chrome
  rectangle for the center-interaction assertion. Keep the magnified icon
  interaction assertion, assert the expanded chrome remains centered in the
  fixed surface, and keep the transparent top-left/envelope-corner negative
  assertion. Remove the stale requirement that chrome width equals resting
  width.

- [ ] **Step 5: Correct drag-collapse sequencing.**

  In `dragGeometryRemainsStableDuringMagnificationCollapse()`, after hover
  wait for positive expansion and `chrome->width() > restingWidth` before the
  mouse press. Immediately after drag begins, assert the delegate is dragging
  and wait for `magnificationExtraPrimary == 0` plus chrome return to resting
  width. Keep the existing strict surface-dimension, dragged-center, target
  index, and reorder assertions. For vertical drag coverage, make the same
  endpoint expectation if its current setup observes chrome dimensions.

- [ ] **Step 6: Strengthen mode/configuration/capacity coverage without changing
  delegate math.**

  Update `hoverModesAndTransitions()` so `lift` and `none` assert zero
  `magnificationExtraPrimary` and resting primary chrome size, and add the
  active-pointer switch from magnification to lift/none followed by the same
  zero/resting endpoint. In the existing capacity/headroom coverage, assert
  `magnificationExtraPrimary <= maximumMagnificationExtraPrimary` whenever
  magnification is active and assert the mapped chrome rectangle is contained
  in the fixed panel rectangle for representative icon-size/radius/scale
  combinations in horizontal and vertical configurations. If current model
  refresh coverage already triggers `updateHoverEffect()`, add only the
  property endpoint assertion needed to prove the expansion is recomputed
  rather than accumulated.

- [ ] **Step 7: Update declarations and use semantic test names.**

  Change only the affected private-slot declarations and names needed by the
  updated tests. Do not add tests that inspect task identity, Typhon state, or
  application-specific behavior.

### Task 2: Verify RED and commit the regression tests

**Files:**
- Test: `Dock/tests/DockHoverQmlTest.cpp`

- [ ] **Step 1: Build the focused test target against the unchanged production
  QML.**

  Run:

  ```bash
  rtk run cmake --build build/release --target dock-hover-qml-test
  ```

  Expected: compile succeeds, or any pre-existing build failure is recorded
  without changing production code.

- [ ] **Step 2: Run the focused test target and verify the expected RED.**

  Run:

  ```bash
  rtk ctest --test-dir build/release -R 'dock-hover-qml-test' --output-on-failure
  ```

  Expected: the newly changed tests fail because `dockChrome` remains at its
  resting primary size; failures must identify the new chrome-growth/backdrop
  expectations rather than compile errors, identity assertions, or unrelated
  tests. Record the exact failed/passed counts.

- [ ] **Step 3: Inspect the test diff and commit only the RED tests.**

  Run:

  ```bash
  rtk git -C /home/agony/GitHub/Eclipse diff --check
  rtk git -C /home/agony/GitHub/Eclipse diff -- Dock/tests/DockHoverQmlTest.cpp
  rtk git -C /home/agony/GitHub/Eclipse add Dock/tests/DockHoverQmlTest.cpp
  rtk git -C /home/agony/GitHub/Eclipse commit -m "test: cover dock magnification chrome geometry"
  ```

### Task 3: Implement the minimal production fix

**Files:**
- Modify: `Dock/qml/components/DockPanel.qml` near the existing hover state and `dockChrome` dimensions.

**Interfaces:**
- Consumes: `totalExtra` calculated by `updateHoverEffect()` and existing `magnificationActive` state.
- Produces: `magnificationExtraPrimary`, a current primary-axis chrome expansion value used only by `dockChrome` dimensions.

- [ ] **Step 1: Add the single mutable expansion property.**

  Add one property beside the existing hover/transform state:

  ```qml
  property real magnificationExtraPrimary: 0
  ```

  Do not add orientation-specific width/height state.

- [ ] **Step 2: Bind the real chrome dimensions by orientation.**

  Replace only the two resting-size bindings with:

  ```qml
  width: root.vertical
      ? root.restingWidth
      : root.restingWidth + root.magnificationExtraPrimary
  height: root.vertical
      ? root.restingHeight + root.magnificationExtraPrimary
      : root.restingHeight
  ```

  Keep the current x/y centering, radius, shared material bindings, backdrop
  region, width/height Behaviors, and geometry-change input hooks unchanged.

- [ ] **Step 3: Publish the existing authoritative total.**

  In `updateHoverEffect()`, immediately after the delegate loop has completed
  and before/alongside the existing delegate update call, assign:

  ```qml
  magnificationExtraPrimary = magnificationActive ? totalExtra : 0
  ```

  Do not alter `totalExtra`, scale, prefix, offset, `primaryExtent()`, pointer
  calculations, or `updateDelegateTransforms()`.

- [ ] **Step 4: Review the production diff for scope.**

  Confirm no changes exist in Typhon, Dock runtime task-identity files,
  shared-effects sources, legacy color literals, root width/height bindings,
  pointer geometry functions, or delegate transform math.

### Task 4: Verify GREEN and run the requested focused suites

**Files:**
- Test: `Dock/tests/DockHoverQmlTest.cpp`
- Modify: `Dock/qml/components/DockPanel.qml`

- [ ] **Step 1: Build the requested Dock targets in the existing release
  directory.**

  Run:

  ```bash
  rtk run cmake --build build/release --target dock-hover-qml-test astrea-shell
  ```

- [ ] **Step 2: Run the full focused Dock hover test.**

  Run:

  ```bash
  rtk ctest --test-dir build/release -R 'dock-hover-qml-test' --output-on-failure
  ```

  Expected: all registered Dock hover tests pass; report exact pass/failure
  counts rather than a qualitative summary.

- [ ] **Step 3: Build and run shared backdrop geometry tests.**

  Run:

  ```bash
  rtk run cmake --build build/release --target backdrop-effect-regions-test
  rtk ctest --test-dir build/release -R 'backdrop-effect-regions-test' --output-on-failure
  ```

  Expected: all registered shared backdrop tests pass. If a backdrop test
  fails, inspect whether it demonstrates a real dynamic-item capability gap
  before considering any shared-effects change; do not alter shared effects
  for a stale Dock expectation.

- [ ] **Step 4: Run available broader Dock/Shell verification.**

  Use the existing CTest listing to identify the repository's full Dock hover
  and focused Shell targets, then run only the available relevant tests in
  `build/release`, preserving and reporting unrelated pre-existing failures.

### Task 5: Final invariant verification and commit

**Files:**
- Verify: `Dock/qml/components/DockPanel.qml`, `Dock/tests/DockHoverQmlTest.cpp`, shared backdrop sources, and Git diff.

- [ ] **Step 1: Check the implementation against the final invariants.**

  Verify by source inspection and tests that: horizontal chrome changes width
  only; vertical chrome changes height only; root surface dimensions remain
  fixed; expansion comes only from `totalExtra`; pointer/delegate geometry is
  unchanged; backdrop and input regions target animated `dockChrome`; unused
  envelope space remains unblurred/non-interactive; drag collapse preserves
  centers/targets; lift/none are resting-sized; and shared Shell material
  bindings/values are unchanged.

- [ ] **Step 2: Run final hygiene checks.**

  Run:

  ```bash
  rtk git -C /home/agony/GitHub/Eclipse diff --check
  rtk git -C /home/agony/GitHub/Eclipse status --short --branch
  ```

  Confirm the Eclipse worktree contains only the design/plan docs and the
  intended Dock QML/test changes, with no Typhon path involved.

- [ ] **Step 3: Commit the GREEN production change and plan if still unstaged.**

  Run:

  ```bash
  rtk git -C /home/agony/GitHub/Eclipse add Dock/qml/components/DockPanel.qml \
      Dock/tests/DockHoverQmlTest.cpp \
      docs/superpowers/plans/2026-09-17-dock-magnification-chrome.md
  rtk git -C /home/agony/GitHub/Eclipse commit -m "fix: restore dock chrome magnification"
  ```

  Do not commit unrelated files or modify the already committed design spec.
