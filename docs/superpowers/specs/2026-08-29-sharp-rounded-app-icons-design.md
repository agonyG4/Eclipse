# Sharp Rounded App Icons Design

Status: Approved by the implementation request.

## Goal

Replace the proven rounded-icon `Qt5Compat.GraphicalEffects.OpacityMask`
bottleneck while preserving the existing resolution-aware source request and
all Dock, AltTab, Spotlight, theme, cache, and DPR behavior.

## Design

`AstreaAppIcon.qml` keeps its current `Image` as the sole source of icon
pixels. The unrounded path remains a direct `Image` with no active effect.
When `iconRadius > 0`, a `Loader` creates one small analytic `ShaderEffect`
whose `source` is that same high-resolution `Image`. The fragment shader
samples the source texture directly and multiplies its premultiplied alpha by
a rounded-rectangle coverage computed from local UV coordinates. The source
image is hidden only while the shader owns the draw, so rounded mode cannot
draw the source twice.

Only masking is implemented. Blur, shadow, saturation, brightness,
colorization, contrast, and related effect features remain disabled. The test
uses `MultiEffect.hasProxySource` when available while evaluating the modern
effect path. The measured Qt 6.11.2 MultiEffect capture had
`hasProxySource == false`, but its interior contrast matched the legacy
OpacityMask regression, so it is not used in the final production path.

The shader is compiled through Qt's `.qsb` pipeline. It deliberately sets
`supportsAtlasTextures: false`, leaving Qt to detach an atlas texture when
needed so the shader can use local UV coordinates without a low-resolution
mask or an extra `ShaderEffectSource`. No fixed-size mask texture is created.

The provider request remains unchanged: `resolvedSource`,
`effectiveSourcePixelSize`, logical size, DPR, maximum-presentation preload,
theme selection, and cache keys remain outside the rounded effect. The effect
is configured by the stable `iconRadius` property, not by Dock magnification.

## Validation

The real QML test will let `QQuickWindow::effectiveDevicePixelRatio()` flow
into `AstreaAppIcon` and will assert the screen, window, effective window, and
QML DPR values before interpreting image metrics. With a 96 logical-pixel
icon at presentation scale 1.6, expected source extents are approximately
154, 231, and 308 physical pixels at DPR 1, 1.5, and 2.

The same high-frequency dark and light sources, logical geometry, outer scale,
and source URL are used for three captures: direct Image, an inline
test-only legacy OpacityMask, and the production shader path. The test
excludes a 12-pixel interior margin from edge analysis, requires the new path
to retain at least `0.98` of direct interior contrast, and requires a measured
improvement over the legacy path. It separately verifies transparent
corners, antialiased edges, and opaque interiors. The existing deterministic
alpha-math test remains clearly identified as non-QML validation.

The split-theme validation will separately test first-`index.theme` metadata
priority and duplicate content-root selection using identical metadata. The
second test will record the actual Qt behavior instead of claiming that public
QIcon APIs provide independent metadata/content ordering controls.

The final shader exists because the correctly configured MultiEffect trial
failed the quality threshold: at DPR 1 its new-path contrast ratio was
`0.912873`, identical to legacy OpacityMask, with maximum interior difference
`190`. The direct-sampling shader was then qualified by the same three-path
capture at DPR 1, 1.5, and 2.

## Scope

No provider resolver, `QIcon::availableSizes()` selection, Dock geometry,
Layer Shell behavior, input/reorder behavior, Typhon code, or unrelated
personalization work changes.
