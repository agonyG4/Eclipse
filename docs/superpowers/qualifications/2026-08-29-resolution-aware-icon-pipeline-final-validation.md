# Resolution-Aware Icon Pipeline Final Validation

Date: 2026-08-29  
Environment: Qt 6.11.2, x86_64, Debug build, Qt Quick offscreen and Wayland
(`WAYLAND_DISPLAY=wayland-1`)

## Representation-selection correctness

The production provider continues to use `QIcon::fromTheme()` followed by
`QIcon::pixmap(logicalSize, devicePixelRatio, ...)`. Production code does not
use `QIcon::availableSizes()` to select a representation. The existing
pixel-distinguishable tests remain in place for theme `Scale=2`, named
scalable SVG, threshold selection, inheritance, current-theme preference,
hicolor fallback, and smaller-raster non-upscaling.

`astrea-icon-provider-test` passed all 20 tests, including:

```text
providerHonorsThemeScaleMetadata
providerResolvesNamedScalableSvgAtPhysicalTarget
providerFollowsQtThresholdSelection
providerFallsBackThroughThemeInheritance
providerPrefersCurrentThemeOverCloserInheritedSize
providerFallsBackToHicolor
providerDoesNotUpscaleSmallerRaster
```

## Split-theme metadata and content-root priority

`applyPreservesSplitThemePriority` creates two roots for the same theme and
invokes the production `AstreaIconTheme::apply()` path. The user root contains
only `96x96/apps` metadata and green content; the lower-priority system root
contains only `48x48/apps` metadata and red content, with a duplicate icon name.
The user-only icon proves which `index.theme` metadata was selected, while the
duplicate icon proves which content root won.

The original reversed-QIcon-path behavior failed the user-only probe. Keeping
the merged Freedesktop-priority order passes both probes on Qt 6.11.2. This is
the smallest change that preserves predictable user overrides without adding
an Astrea size/Scale/Threshold resolver. Qt's public API offers one shared
search-path order for metadata and content rather than independent controls;
the regression records the supported-version behavior instead of guessing
around that limitation. The pure `searchPathsFor()` ordering and deduplication
test remains separate.

## Real OpacityMask A/B

`roundedAlphaMathPreservesInteriorDetail` is retained as a deterministic
source-buffer check and is explicitly not a QML effect test.

`opacityMaskPreservesInteriorDetailAtMaximumScale` instantiates the real
`AstreaAppIcon.qml`, renders the same high-frequency SVG unmasked and with the
normal rounded mask at presentation scale `1.6`, and compares interior detail
separately from corner alpha and edge antialiasing.

The real offscreen capture is not qualifying: the masked interior is not
capturable, so the test reports an explicit skip. The real Wayland capture
passed at all three requested scale factors with:

```text
QT_SCALE_FACTOR=1:   source pixels 154, maximum interior difference 190, contrast ratio 0.912873
QT_SCALE_FACTOR=1.5: source pixels 154, maximum interior difference 125, contrast ratio 0.975791
QT_SCALE_FACTOR=2:   source pixels 154, maximum interior difference 231, contrast ratio 0.99043
```

The rounded path therefore loses measurable interior detail in this runtime;
`OpacityMask` is a proven secondary bottleneck. It remains installed because
the bounded `MultiEffect` substitution did not provide a qualifying capture
or a source-preserving drop-in. A replacement needs a separate, measured
implementation design.

## DPR evidence

`windowDprMatchesScreenDprForDockIcon` passed under the offscreen backend at
all requested scale factors:

| `QT_SCALE_FACTOR` | Screen DPR | window DPR | effective window DPR |
| --- | ---: | ---: | ---: |
| `1` | `1` | `1` | `1` |
| `1.5` | `1.5` | `1.5` | `1.5` |
| `2` | `2` | `2` | `2` |

It also passed in the live Wayland backend at `QT_SCALE_FACTOR=1.5` with
`1.5 / 1.5 / 1.5`. No reproduced per-window mismatch justifies a DPR service;
the simple QML property is retained.

## Dock source-request invariant

The live-capable Wayland run of
`iconSourceQualityRemainsStableDuringHover` passed. It preserves both
`resolvedSource` and `effectiveSourcePixelSize` while `magnificationScale`
changes. The mask investigation does not feed instantaneous magnification
back into source resolution.

## Verification commands and results

Focused tests passed:

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/debug \
  -R '^(astrea-icon-provider-test|astrea-app-icon-qml-test|dock-hover-qml-test)$' \
  --output-on-failure
=> 3/3 passed
```

The affected offscreen suite passed `27/27`, covering the provider, shared QML,
Spotlight, AltTab, and Dock test groups. The QML lint/cache targets passed:

```text
cmake --build build/debug --target astrea-shell_qmllint astrea-settings-ui_qmllint -j2
=> exit 0; existing qmllint warnings, no errors
```

The focused tests also passed at offscreen `QT_SCALE_FACTOR=1`, `1.5`, and
`2`. The real Wayland mask and Dock invariant passed as reported above.

`git diff --check` passed before commit.

## Live Dock quality and remaining limits

The real Wayland QML hover regression ran successfully, but no Typhon/Eclipse
production session was available in this workspace: no running Typhon, shell,
or Dock process was present, and the full Debug build is blocked by an
unrelated existing compile error in `Shell/app/AstreaShellApplication.cpp`:
`DockController::surfacePlacement() const` is private. Typhon was not modified.

Consequently, the manual production Dock pass remains unverified for these
specific live inputs:

```text
Scale=2 theme raster
large raster
named scalable SVG
direct SVG
small-only raster
Flatpak application
```

Resting quality, maximum magnification sharpness, delayed sharpness changes,
provider reload behavior, rounded/unrounded visual comparison, and the honest
small-raster source limitation still require a real Typhon/Eclipse session.
The repository tests verify the source-request invariant and representation
selection, but do not substitute for that manual visual qualification.
