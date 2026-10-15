# Dock Testing

Configure, build, and run the deterministic tests in the stable external Debug
tree (build output must stay outside the source checkout):

```bash
cmake -S . -B /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -DCMAKE_BUILD_TYPE=Debug -DASTREA_ENABLE_LAYER_SHELL=ON
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build -j"$(nproc)"
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build --output-on-failure
```

The resident Dock is hosted by `astrea-shell`; `astrea-dock` is retained only
as a compatibility IPC client target. Dock-specific targets can be built and
run independently:

```bash
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target \
  astrea-dock-runtime astrea-dock-typhon astrea-dock-wayland astrea-shared-typhon \
  astrea-shell dock-app-matcher-test dock-application-state-projector-test \
  dock-app-model-test dock-controller-test dock-typhon-runtime-integration-test \
  dock-hover-qml-test dock-layer-shell-surface-test dock-input-region-test \
  dock-surface-geometry-test
python3 Dock/tests/check_dock_architecture.py
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build \
  -R '(^dock-|legacy-dock)' \
  --output-on-failure

ASTREA_CMAKE_BUILD_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build \
  tools/ci/run-qml-gate.sh
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build --output-on-failure
```

The focused `dock-app-matcher-test` and
`dock-application-state-projector-test` are Dock-owned tests. The core
controller test uses a fake `DockRuntimeBackend`; the real boundary test keeps
the `FakeTyphonProtocolAdapter -> TyphonToplevelConnection ->
TyphonDockRuntimeBackend -> DockController` path.

Repeat the configuration and relevant Dock/Shell builds in a separate
no-Layer-Shell tree because its CMake options are incompatible with the
production Debug tree:

```bash
cmake -S . -B /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build \
  -DCMAKE_BUILD_TYPE=Debug -DASTREA_ENABLE_LAYER_SHELL=OFF
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build --target \
  astrea-dock-runtime astrea-dock-typhon astrea-dock-wayland astrea-shell \
  dock-app-matcher-test dock-application-state-projector-test dock-app-model-test \
  dock-controller-test dock-typhon-runtime-integration-test dock-hover-qml-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-no-layer-shell-build \
  -R '(^dock-|legacy-dock)' --output-on-failure
```

Sanitizer validation uses the project option:

```bash
cmake -S . -B /mnt/Aether/Desktop/GitHub/Eclipse-asan-build -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_ASAN=ON
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-asan-build -j"$(nproc)"
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-asan-build --output-on-failure

cmake -S . -B /mnt/Aether/Desktop/GitHub/Eclipse-ubsan-build -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_UBSAN=ON
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-ubsan-build -j"$(nproc)"
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-ubsan-build --output-on-failure
```

Tests are deterministic and do not launch real applications in controller
tests. The IPC tests use local temporary socket names. The controller test
uses a fake neutral runtime backend. The runtime integration test drives a fake
Typhon protocol adapter through the real Typhon connection and Dock adapter,
covering dynamic rows, exact activation/close, and authority loss. Explorer's source contract is
covered by `python3 -m unittest src/System/tests/test_bin_launchers.py` in the
current AstreaOS source tree.

`dock-config-contract-test` covers personalization defaults, new-field parsing,
enum and finite-number handling, bounds, legacy `bottomMargin` and
`magnificationEnabled` compatibility, unknown-key/pin preservation, malformed
file refusal, and atomic replacement. `dock-config-watcher-test` covers
hover-effect modes, legacy compatibility, all personalization values, type and
finite-number fallbacks, and bounds.
`dock-config-persistence-test` covers pins-only atomic replacement, preservation
of known and unknown keys, malformed-file refusal, validation, write errors, and
watcher recovery. `dock-controller-test` covers stable identity reorder,
launch-state preservation, persistence failure, unchanged runtime-only order,
all personalization values, floating effective margin, animation and indicator
state, and deterministic auto-hide policy. `dock-layer-shell-surface-test`
covers Bottom/Left/Right anchors and margins, position-aware output geometry,
the explicit resting reservation, and zero reservation while unmapped or
auto-hidden. `dock-hover-qml-test` exercises the real
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
The icon-quality regression also checks the shared logical-size/DPR formula and
proves that changing the visual scale leaves the resolved icon URL and physical
source target unchanged.
`astrea-icon-provider-test` separately proves Qt-owned `Scale=2`, scalable SVG,
threshold, inheritance, current-theme preference, hicolor fallback, smaller
raster preservation, `.icons` priority, cache invalidation, and concurrent
theme mutation. Its representation assertions compare distinguishable pixels
and raw output dimensions, including deterministic `DPR=1`, `1.5`, and `2`
requests. No production path uses `QIcon::availableSizes()` for selection.
The shared QML test keeps a deterministic
`roundedAlphaMathPreservesInteriorDetail` source-buffer check, which does not
execute QML `OpacityMask`. It also has
`opacityMaskPreservesInteriorDetailAtMaximumScale`, which instantiates the real
`AstreaAppIcon`, captures unmasked and rounded paths, and compares interior
detail separately from corner alpha and edge antialiasing. The offscreen Qt
backend currently skips that case because its masked output is not capturable;
the live Wayland run is the qualifying A/B and records the measured contrast
ratio rather than treating the synthetic check as proof.
The cancellation case retains the panel cancellation reset path after a real
pointer drag; the offscreen Qt backend does not provide a compositor
pointer-cancel event.

`floatingAutoHideUsesPhysicalEdgeRevealGeometry` proves the shared placement
policy and the one-surface QML geometry for Bottom, Left, and Right with a
non-zero floating gap: normal mode keeps the Layer Shell margin, auto-hide
uses the physical edge, the revealed chrome keeps the visual inset, and the
exclusive zone stays zero. It does not prove that a compositor delivers a
pointer touching the physical output boundary to the reveal target, or that
pointer traversal through the floating gap reaches the target reliably. Those
cases require live Wayland qualification with the production LayerShellQt
backend. The deterministic vertical drag test exercises real QML delegates and
signals, but it does not prove compositor pointer delivery or visual drag feel.

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
scale `1.0`, and inward prefix-width translations for vertical positions. Its
vertical cases cover Left/Right first/middle/last delegates, fixed envelopes,
primary-axis drag/reorder coordinates, release behavior, and edge-side
indicators. The Layer Shell test keeps
the exclusive zone at the normal resting height even when the visual surface
height is larger. Its reorder cases cover off-center rendered drag origins for
first, middle, and last pins, stable geometry while magnification collapses,
deferred hover refresh after a `rowsMoved` reorder, identity lookup from live
delegates, and restoration of the release pointer when the exclusive grab
suppresses the panel hover handler. They also verify one drop request and no
activation from a drag.

The QML lint check is a syntax/type check and the focused QML test runs offscreen;
visual magnification and drag feel therefore still require live/manual
verification on a Wayland session. The deterministic verification does not
constitute live Wayland/Typhon visual validation. Live validation must additionally confirm
that the requested Layer Shell surface dimensions stay constant while the
chrome width and icon transforms change independently.

The production Debug configuration uses LayerShellQt. The separate
no-Layer-Shell configuration checks that the Dock and Shell targets still
build without that optional backend. QML lint, `git diff --check`, and live
Wayland/Typhon qualification are separate gates. Live results must be reported
per case and must not be inferred from deterministic tests.
