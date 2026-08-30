# Sharp Rounded App Icons Design

Status: Approved by the implementation request.

## Goal

Replace the proven rounded-icon `Qt5Compat.GraphicalEffects.OpacityMask`
bottleneck while preserving the existing resolution-aware source request and
all Dock, AltTab, Spotlight, theme, cache, and DPR behavior.

## Design

`AstreaAppIcon.qml` will keep its current `Image` as the sole source of icon
pixels. The unrounded path will remain a direct `Image` with no active effect.
When `iconRadius > 0`, a `Loader` will create one `QtQuick.Effects.MultiEffect`
whose `source` is that same `Image` and whose `maskSource` is a texture-backed
rounded mask item. The source image will use `opacity: 0` while masked so it
remains a usable texture source without being drawn a second time. The mask
item will use `layer.enabled`, `layer.smooth`, and `layer.mipmap`, and will
cover the same logical rectangle as the icon.

Only masking will be enabled. Blur, shadow, saturation, brightness,
colorization, contrast, and related effect features remain disabled. The test
will require `MultiEffect.hasProxySource == false` so the source Image is
consumed directly rather than flattened into an unnecessary proxy texture.

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
and source URL will be used for three captures: direct Image, an inline
test-only legacy OpacityMask, and the production MultiEffect path. The test
will exclude a 12-pixel interior margin from edge analysis, require the new
path to retain at least `0.98` of direct interior contrast, and require a
measured improvement over the legacy path. It will separately verify
transparent corners, antialiased edges, opaque interiors, and absence of
proxy source use. The existing deterministic alpha-math test remains clearly
identified as non-QML validation.

The split-theme validation will separately test first-`index.theme` metadata
priority and duplicate content-root selection using identical metadata. The
second test will record the actual Qt behavior instead of claiming that public
QIcon APIs provide independent metadata/content ordering controls.

If MultiEffect fails the quality threshold after correct DPR scaling and
source/mask configuration, the next step is a minimal analytic rounded-mask
shader that samples the original texture directly. No shader will be added
unless the MultiEffect capture proves insufficient.

## Scope

No provider resolver, `QIcon::availableSizes()` selection, Dock geometry,
Layer Shell behavior, input/reorder behavior, Typhon code, or unrelated
personalization work changes.
