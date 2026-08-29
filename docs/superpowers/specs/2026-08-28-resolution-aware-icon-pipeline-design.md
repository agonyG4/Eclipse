# Resolution-Aware App Icon Pipeline

## Status

Approved design for implementation in Eclipse. This document is the source of truth for the implementation and verification work described by the companion plan.

## Problem

Eclipse currently has several independent icon-size policies. `AstreaIconProvider` manually walks XDG directories and can choose a smaller raster before a larger available raster. The provider then scales the selected image to the requested size. Dock, AltTab, Spotlight, and Settings also choose source sizes independently, generally by multiplying a logical size by two. Dock magnification changes presentation scale without communicating device-pixel density or the largest animated extent to the source pipeline.

The result is avoidable upscaling, inconsistent theme behavior, and source reloads when an icon is only changing presentation size. Retina and fractional-DPR displays expose these defects most clearly.

## Goals and invariants

- Select icon representations through Qt's `QIcon` and Freedesktop theme engine.
- Keep icon identity, logical presentation extent, device-pixel ratio, physical source extent, and theme revision explicit.
- Compute physical pixels as `ceil(logicalExtent * devicePixelRatio)`.
- Treat logical extent as the UI size and physical extent as the texture quality target.
- Never allocate an unbounded image from a malformed URL or QML property.
- Accept logical extents from 1 through 256 and physical extents up to 1024. Reject non-finite or non-positive values and clamp finite oversized requests to safe bounds.
- Keep the source target stable while a presentation animation runs.
- Preserve the existing Dock geometry, input, reorder, context-menu, and runtime behavior.
- Preserve direct local-file and narrow `/usr/share/pixmaps` compatibility where Qt's theme engine cannot provide the legacy icon.
- Keep all new and modified documentation in English.

The pipeline is:

```text
.desktop identity
    -> AstreaIconTheme configures Qt theme and search paths
    -> AstreaIconProvider
    -> QIcon/Freedesktop engine selects a representation
    -> high-resolution source texture
    -> shared AstreaAppIcon
    -> Qt Quick presentation transform
```

## Theme configuration

`AstreaIconTheme::apply()` remains the single configuration entry point. It
merges Astrea's discovered icon roots with the existing Qt search and fallback
paths, deduplicates them, and preserves native, resource, and platform-provided
paths. The public discovery order places `~/.icons` before XDG data roots and
Flatpak exports; Qt receives the equivalent reverse traversal order because its
loader resolves duplicate theme roots from the end of its search list.

The effective theme priority remains:

```text
ASTREA_ICON_THEME
    > QS_ICON_THEME
    > qt6ct
    > platform theme
    > WhiteSur-dark compatibility
    > hicolor
```

Search roots include XDG data locations, `~/.icons`, the user Flatpak export, and the system Flatpak export. The merge must not discard paths already installed by Qt or a desktop environment. Theme changes continue to invalidate provider caches and advance the provider revision.

## Resolution request contract

The provider uses a normalized request object equivalent to:

```cpp
struct IconRenderRequest {
    int logicalExtent;
    qreal devicePixelRatio;
    int physicalExtent;
};
```

The request constructor validates finite, positive input, applies the documented bounds, and calculates `physicalExtent` with a checked ceiling operation. The normalized DPR is quantized for stable cache keys. A malformed query falls back to a safe bounded request rather than causing an allocation or a process-wide failure.

The image-provider URL carries both the legacy logical size and the new density-aware fields:

```text
image://astrea-icon/<name>?logicalSize=96&dpr=2&pixelSize=192&revision=17
```

`size=N` remains accepted as a logical-size-only compatibility form. `pixelSize` is diagnostic and consistency-checked; the provider recomputes the authoritative physical extent from logical size and DPR.

## Representation and cache policy

For named icons, the provider asks `QIcon::fromTheme()` for the active identity and calls:

```cpp
icon.pixmap(QSize(logicalExtent, logicalExtent),
            devicePixelRatio,
            QIcon::Normal,
            QIcon::Off);
```

The returned pixmap is normalized to raw physical dimensions before it is exposed to Qt Quick. The provider must not resize a smaller selected representation upward. SVG and other scalable sources may rasterize at the requested physical extent. Larger raster sources may be downsampled by Qt, but a smaller raster must not be promoted as if it contained additional detail. `availableSizes()` is not part of this policy; Qt retains the directory metadata needed for `Scale`, `Type`, `Threshold`, scalable ranges, and inheritance.

Extension-bearing names retry with the basename where appropriate. Direct absolute files are loaded directly. The legacy pixmaps directory remains a narrow fallback, not a second XDG theme resolver.

Positive cache keys include icon identity, logical extent, normalized DPR, physical extent, and theme revision. Positive cache cost is based on actual image byte cost where available and remains bounded by the existing memory budget unless measured evidence justifies a change. Negative-cache entries remain bounded and are invalidated on theme changes. Cache invalidation must cover theme reloads and search-path changes.

## Shared QML policy

`shared/qml/AstreaAppIcon.qml` owns source-quality policy. Its public policy properties are:

```qml
property real maximumPresentationScale: 1.0
property real maximumPresentationLogicalSize: 0
property real devicePixelRatioOverride: 0
readonly property real effectiveDevicePixelRatio
readonly property int effectiveMaximumLogicalSize
readonly property int effectiveSourcePixelSize
readonly property string resolvedSource
```

An explicit positive `maximumPresentationLogicalSize` wins. Otherwise the maximum logical target is `iconSize * maximumPresentationScale`. The normal DPR comes from `Screen.devicePixelRatio`; the override exists for deterministic QML tests. A Dock integration boundary records and compares this value with `QQuickWindow::devicePixelRatio()` and `QQuickWindow::effectiveDevicePixelRatio()` at 1x, 1.5x, and 2x. The source target is `ceil(effectiveMaximumLogicalSize * effectiveDevicePixelRatio)`, bounded by the provider contract.

The image keeps `smooth: true`, `mipmap: true`, `asynchronous: true`, and the existing fallback behavior. Its source and `sourceSize` use the effective source target. Presentation scale, opacity, masking, and animation remain separate from source selection.

## Consumer migration

### Dock

Dock removes the fixed `iconSize * 2` source-size policy. It supplies the configured maximum magnification to `maximumPresentationScale`. The animated `magnificationScale` changes only the delegate transform and never changes the icon URL or `sourceSize`. Configuration changes may request a new quality target once; hover frames must not.

### AltTab

AltTab retains its 72-to-84 logical presentation animation and selected-state visuals. Both states use one maximum logical source target of 84, multiplied by the effective DPR. Selection must not change the icon source URL or source size.

### Spotlight

Spotlight uses its 40 logical result extent and the effective DPR. It no longer assumes a blind 2x source size.

### Masking

The existing `Qt5Compat.GraphicalEffects.OpacityMask` remains. A deterministic
same-raster A/B at a 1.6 presentation scale confirms that rounded alpha leaves
interior high-frequency RGB detail unchanged while changing corner alpha and
edge antialiasing. The offscreen scene graph does not provide a stable visual
OpacityMask capture, so a live Wayland visual comparison remains pending.

## Verification strategy

The implementation is verified at both the provider and QML policy boundaries.

The dedicated provider test covers missing and empty names, absolute files, extension-bearing names, Qt-selected 48/96/128 representations, true `Scale=2`, logical-size versus DPR distinction, named scalable SVG, thresholds, inheritance, current-theme preference, hicolor, `.icons` priority and deduplication, theme switches, positive and negative cache invalidation, cache-key separation, invalid zero/negative/non-finite/huge values, fractional DPR, and bounded physical targets. Representation tests use deliberately distinguishable pixel content. Flatpak roots are covered in search-root discovery/deduplication; a live Flatpak application icon remains a manual validation case.

QML tests cover these calculations:

| Maximum presentation target | DPR | Physical source |
| ---: | ---: | ---: |
| 48 × 2 | 1 | 96 |
| 48 | 2 | 96 |
| 64 | 2 | 128 |
| 48 × 1.6 | 1.5 | `ceil(115.2)` = 116 |

The tests also prove that Dock hover changes visual scale while keeping source URL and effective source size unchanged, that a DPR change causes one quality-target change independent of hover, that AltTab selection changes presentation width without a quality reload, and that Spotlight uses 40 logical pixels times DPR.

The verification record is command-based: focused provider/QML/Dock/AltTab/Spotlight tests, the hardened Dock test subset, Debug and Release builds, QML lint, `git diff --check`, and high-DPI runs at 1x, 1.5x, and 2x are recorded only after they execute. The deterministic mask A/B is a same-raster source-buffer comparison because the offscreen scene graph does not expose a stable OpacityMask capture. Live Wayland/Typhon visual validation, including Flatpak and low-resolution-only applications, remains pending unless explicitly run and reported.

## Non-goals

- No redesign of Dock geometry or input behavior.
- No migration of unrelated Settings icon sizing unless required by a failing shared contract.
- No second manual XDG resolver.
- No unbounded cache increase.
- No changes to Typhon.
