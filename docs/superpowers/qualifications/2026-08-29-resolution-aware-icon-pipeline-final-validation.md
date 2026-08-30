# Resolution-Aware Icon Pipeline Final Validation

Date: 2026-08-29  
Environment: Qt 6.11.2, x86_64, NVIDIA, Debug and Release builds,
Qt Quick Wayland (`WAYLAND_DISPLAY=wayland-1`) and offscreen tests

## Representation-selection correctness

The provider still delegates representation selection to
`QIcon::fromTheme()` and `QIcon::pixmap(logicalSize, devicePixelRatio, ...)`.
Production code does not use `QIcon::availableSizes()` or reimplement XDG
Scale, Threshold, scalable-SVG, inheritance, or fallback matching.

`astrea-icon-provider-test` passed all 21 test cases. The required
pixel-distinguishable regressions remain present and pass:

```text
providerHonorsThemeScaleMetadata
providerResolvesNamedScalableSvgAtPhysicalTarget
providerFollowsQtThresholdSelection
providerFallsBackThroughThemeInheritance
providerPrefersCurrentThemeOverCloserInheritedSize
providerFallsBackToHicolor
providerDoesNotUpscaleSmallerRaster
```

`providerInvalidatesPositiveAndNegativeCachesOnThemeChange` also passed the
concurrent invalidation stress sequence. The existing independent cache locks
remain separate from the serialized `AstreaIconTheme::qIconMutex()` region
covering QIcon theme mutation and rendering.

## Split-theme metadata and content-root priority

The tests now use the production `AstreaIconTheme::apply()` path. The pure
`searchPathsFor()` ordering test remains separate from provider behavior.

`applyPreservesSplitThemeMetadataPriority` uses different `index.theme`
metadata: the high-priority user root advertises only `96x96/apps`, while the
lower-priority system root advertises only `48x48/apps`. The user-only green
icon is found, proving that the first user `index.theme` is the metadata
source.

`applyPreservesDuplicateContentRootPriority` uses identical `48x48/apps`
metadata and green user versus red system duplicate content. Qt 6.11.2
selects the red lower-priority system file. This is recorded as actual
behavior; it is not claimed as a user-content override guarantee.

The production list remains in Freedesktop priority order so metadata priority
is predictable. Qt exposes one shared search-path list for both metadata and
content, while its QIconLoader content-entry insertion order can differ from
metadata traversal. Public QIcon APIs do not provide independent lists. The
task therefore documents this Qt limitation and does not add a second Astrea
Scale/Threshold/representation resolver.

## Rounded-path A/B result

`roundedAlphaMathPreservesInteriorDetail` remains as a deterministic image
math test. Its documentation explicitly says that it does not execute QML
`OpacityMask` and is not proof about effect texture resolution.

`opacityMaskPreservesInteriorDetailAtMaximumScale` now performs a real
three-path Wayland capture using identical high-frequency dark and light SVG
artwork, source URL, logical geometry, presentation scale `1.6`, source
extent, DPR, and outer scene transform:

```text
A. direct production Image
B. inline test-only legacy Qt5Compat OpacityMask
C. production analytic ShaderEffect rounded path
```

The metric uses a 12-pixel inset so rounded corners and antialiased boundary
pixels are not treated as interior detail. `maximumInteriorDifference` and
interior contrast are retained. The acceptance threshold is
`new/unmasked contrast >= 0.98`, plus a measurable improvement over legacy.

The correctly configured MultiEffect trial was also measured before choosing
the shader. At DPR 1 it reported `hasProxySource == false`, but its interior
contrast was `16.0475` versus `17.5791` unmasked, a ratio of `0.912873`, and
its maximum interior difference was `190`, the same as legacy OpacityMask.
It therefore did not close the measured quality gap and was not retained.

The final ShaderEffect samples the already-loaded Image texture directly and
computes rounded coverage analytically. It uses the Qt `.qsb` shader pipeline,
premultiplied-alpha output, no blur/shadow/color features, and no fixed-size
mask texture. `hasProxySource` is not applicable to the final ShaderEffect;
the test reports it as unavailable. `supportsAtlasTextures: false` keeps the
shader's UVs local, allowing Qt to detach an atlas texture when necessary.

### Measured Wayland matrix

Each row is one artwork. Contrast ratios are relative to the unmasked path;
maximum differences are 8-bit channel differences within the interior.

| `QT_SCALE_FACTOR` | source px | artwork | unmasked contrast | legacy contrast | legacy ratio | new contrast | new ratio | legacy max diff | new max diff |
| ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 154 | dark | 17.5791 | 16.0475 | 0.912873 | 17.5789 | 0.999986 | 190 | 2 |
| 1 | 154 | light | 24.7019 | 22.0442 | 0.892408 | 24.7020 | 1.000000 | 234 | 2 |
| 1.5 | 231 | dark | 11.0059 | 10.6696 | 0.969445 | 11.0059 | 0.999995 | 133 | 2 |
| 1.5 | 231 | light | 15.7433 | 15.1324 | 0.961195 | 15.7433 | 0.999999 | 147 | 2 |
| 2 | 308 | dark | 8.09799 | 8.02245 | 0.990672 | 8.09794 | 0.999994 | 237 | 3 |
| 2 | 308 | light | 11.6905 | 11.5273 | 0.986040 | 11.6905 | 1.000000 | 264 | 3 |

All six final rounded captures passed the edge checks: transparent outside
corners, antialiased partial edge pixels, opaque interior pixels, and clean
zero-alpha RGB values. No square-corner, halo, black-fringe, or
premultiplied-alpha artifact was observed by the deterministic checks.

The offscreen QML run is intentionally non-qualifying for effect quality: it
passed its non-capture tests and explicitly skipped the unmeasurable effect
capture. The Wayland captures above are the qualifying runtime evidence.

## DPR evidence

The real rounded capture asserts Screen DPR, `QQuickWindow::devicePixelRatio`,
`QQuickWindow::effectiveDevicePixelRatio`, and QML's effective DPR before
interpreting image metrics. All matched:

| `QT_SCALE_FACTOR` | Screen DPR | window DPR | effective window DPR | QML DPR | source px |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 1 | 1 | 1 | 154 |
| 1.5 | 1.5 | 1.5 | 1.5 | 1.5 | 231 |
| 2 | 2 | 2 | 2 | 2 | 308 |

The expected source extent is `ceil(ceil(96 * 1.6) * DPR)`. No DPR service or
process-global replacement was introduced.

## Source-request invariant and fallback behavior

The Wayland `dock-hover-qml-test` passed its real-hover regression. During
magnification, `resolvedSource` and `effectiveSourcePixelSize` remain
unchanged while `magnificationScale` changes. The new rounded path only
changes the draw path; it does not feed instantaneous magnification into the
provider request. The test does not instrument filesystem, provider,
rasterization, shader-compilation, or texture-allocation counters per frame;
the source stability and static effect configuration are the automated
evidence for this pass.

The QML regression also passed for direct Image mode, rounded mode, missing
icon readiness, error fallback, and fallback initials. `iconRadius = 0` has no
active rounded effect and uses the direct Image path.

## Verification commands and results

Fresh Debug build:

```text
rtk run cmake --build build/debug --clean-first -j2
=> exit 0; all 774 build steps completed
```

Fresh Release build:

```text
rtk run cmake --build build/release --clean-first -j2
=> exit 0; all configured targets completed
```

Focused tests:

```text
QT_QPA_PLATFORM=offscreen rtk run ctest --test-dir build/debug -R '^astrea-icon-provider-test$' --output-on-failure
=> 1/1 passed; direct test output: 21 passed, 0 failed, 0 skipped

QT_QPA_PLATFORM=offscreen rtk run ctest --test-dir build/debug -R '^astrea-app-icon-qml-test$' --output-on-failure
=> 1/1 passed; direct offscreen output: 7 passed, 0 failed, 1 skipped

WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland rtk run ctest --test-dir build/debug -R '^dock-hover-qml-test$' --output-on-failure
=> 1/1 passed
```

The affected suites also passed under `QT_QPA_PLATFORM=offscreen`:

```text
rtk run ctest --test-dir build/debug -R dock --output-on-failure
=> 16/16 passed

rtk run ctest --test-dir build/debug -R alttab --output-on-failure
=> 11/11 passed

rtk run ctest --test-dir build/debug -R spotlight --output-on-failure
=> 2/2 passed
```

The focused provider, QML, and Dock tests were rerun at offscreen scale
factors `1`, `1.5`, and `2`; the qualifying rounded capture was rerun on
Wayland at all three factors. QML lint/cache generation completed through:

```text
rtk run cmake --build build/debug --target astrea-shell_qmllint astrea-settings-ui_qmllint -j2
=> exit 0; existing qmllint warnings, no errors
```

`git diff --check` passed before the final commit.

## Live Dock quality pass and remaining limits

No Typhon/Eclipse production Shell and Dock session was available for a
manual visual pass in this workspace. The running session exposed a separate
Astrea quickshell compositor, and the repository's real Wayland QML tests
were available, but there was no running Typhon/Eclipse process to exercise
the production Dock with pointer, context-menu, reorder, and input-region
interactions.

Therefore these manual production checks remain unverified:

```text
named scalable SVG
large raster theme icon
Scale=2 theme raster
Flatpak application
small-only raster
```

The automated evidence verifies the source invariant and the rounded render
quality on a real Wayland QQuickWindow, but it does not substitute for the
requested live Typhon/Eclipse visual pass. The small-only raster remains
honestly source-limited: the provider cannot recover detail absent from its
available source pixels.

No Typhon source was modified, and no Dock geometry, Layer Shell envelope,
input-region, reorder, context-menu, personalization, provider resolver, or
`availableSizes()` behavior was changed.
