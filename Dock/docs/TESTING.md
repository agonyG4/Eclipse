# Dock Testing

Configure, build, and run all deterministic tests from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The resident Dock is hosted by `astrea-shell`; `astrea-dock` is retained only
as a compatibility IPC client target. Dock-specific targets can be built and
run independently:

```bash
cmake --build build --target astrea-dock dock-app-model-test dock-controller-test \
  dock-config-watcher-test dock-config-persistence-test dock-layer-shell-surface-test \
  dock-input-region-test dock-hover-qml-test dock-ipc-test dock-runtime-paths-test \
  dock-command-line-test
cmake --build build --target astrea-dock dock-app-model-test dock-controller-test \
  dock-typhon-runtime-integration-test dock-application-state-projector-test \
  dock-hover-qml-test typhon-app-matcher-test
ctest --test-dir build -R 'dock-|desktop-entry-catalog-test|typhon-app-matcher-test' \
  --output-on-failure

qmllint Dock/qml/Main.qml Dock/qml/components/DockPanel.qml \
  Dock/qml/components/DockAppDelegate.qml
```

Sanitizer validation uses the project option:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_ASAN=ON
cmake --build build-asan -j"$(nproc)"
ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_UBSAN=ON
cmake --build build-ubsan -j"$(nproc)"
ctest --test-dir build-ubsan --output-on-failure
```

Tests are deterministic and do not launch real applications in controller
tests. The IPC tests use local temporary socket names. The runtime integration
test drives a fake Typhon protocol adapter and proves dynamic rows, exact
activation, close removal, and authority loss. Explorer's source contract is
covered by `python3 -m unittest src/System/tests/test_bin_launchers.py` in the
current AstreaOS source tree.

`dock-config-watcher-test` covers hover-effect modes, legacy compatibility,
magnification defaults, type and finite-number fallbacks, and bounds.
`dock-config-persistence-test` covers pins-only atomic replacement, preservation
of known and unknown keys, malformed-file refusal, validation, write errors, and
watcher recovery. `dock-controller-test` covers stable identity reorder,
launch-state preservation, persistence failure, and unchanged runtime-only
order. `dock-layer-shell-surface-test` covers the explicit resting reservation
and zero reservation while unmapped. `dock-hover-qml-test` exercises the real
Dock panel and delegate for none/lift/magnification geometry, mode transitions,
neighbor displacement, fixed chrome height, and reorder preview/drop signaling
across all modes. It asserts that the maximum transparent surface envelope is
larger than the resting chrome but invariant through pointer entry,
magnification, hover exit, and reorder. It checks first/middle/last pointer
center stability with seven pins, the centered icon baseline and visual bounds
for every integer icon size from 32 through 64, center-relative drag stability
while magnification collapses, dragged-only vertical lift, grab-transition
terminal paths, and event-driven click/thresholded drag behavior. Its precise
interaction-target cases cover the top of a maximum-size magnified icon,
context-menu targeting, empty headroom, overlapping target arbitration, and
the real QWindow mask's actual chrome/icon union.
The cancellation case retains the panel cancellation reset path after a real
pointer drag; the offscreen Qt backend does not provide a compositor
pointer-cancel event.

`dock-input-region-test` is a pure Qt policy test for resting chrome, magnified
headroom, multiple delegate rectangles, finite/invalid geometry, clipping, and
the bounded interaction-rectangle count. The QML tests assert that the actual
centered chrome and a transformed icon head are in the mask while transparent
envelope space is outside, including after an exclusive drag release. These
offscreen tests verify Dock-side arbitration and mask geometry, not compositor
delivery; only a live Wayland test can prove that a click outside the mask
reaches the underlying application rather than merely being ignored by the
Dock.

The visual test checks the center/neighbor/distant raised-cosine ordering,
left/right symmetry, continuity at an influence boundary, exact return to
scale `1.0`, and outward prefix-width translations. The Layer Shell test keeps
the exclusive zone at the normal resting height even when the visual surface
height is larger. Its reorder cases cover off-center rendered drag origins for
first, middle, and last pins, stable geometry while magnification collapses,
deferred hover refresh after a `rowsMoved` reorder, identity lookup from live
delegates, and restoration of the release pointer when the exclusive grab
suppresses the panel hover handler. They also verify one drop request and no
activation from a drag.

The QML lint check is a syntax/type check and the focused QML test runs offscreen;
visual magnification and drag feel therefore still require live/manual
verification on a Wayland session. Live validation must additionally confirm
that the requested Layer Shell surface dimensions stay constant while the
chrome width and icon transforms change independently.

Run the same focused and complete CTest commands in both `build/debug` and
`build/release`. Normal shell validation must use the LayerShellQt-enabled
configuration; a no-LayerShell build is not production validation. QML lint,
`git diff --check`, and a live Typhon qualification are separate gates. Live
results must be reported per case and must not be inferred from deterministic
tests.
