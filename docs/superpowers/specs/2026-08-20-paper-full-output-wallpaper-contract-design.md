# Paper Full-Output Wallpaper Contract

**Date:** 2026-08-20  
**Status:** Approved by the supplied closure brief; implementation follows the repository baseline.  
**Repositories:** Eclipse `0afe2696bfb72e7a459f1f5c4f7872054039230b`; Typhon `0ef9f7b99fa38d0fc04bf5ffa8f494db5a6eade6`

## User-visible bug

Paper wallpaper currently stops at the compositor's usable rectangle, leaving the physical area behind the Topbar and Dock uncovered. Typhon's decorative gradient is then visible through those gaps. This closure changes only the Paper presentation contract; Paper remains the owner of the real wallpaper and Typhon remains a compositor of ordinary Wayland surfaces.

## Evidence classification

- **CONFIRMED:** `WallpaperSurfaceBundle::initialize()` sets `Background`, all four anchors, `None` keyboard interactivity, and the physical `QScreen::geometry()`, but leaves `exclusiveZone` at the shared helper default of `0`.
- **CONFIRMED:** `AstreaLayerShellConfig::exclusiveZone` defaults to `0` in `shared/platform/wayland/LayerShellHelper.hpp`.
- **NATIVE-PROVEN:** Typhon's layer-shell layout uses the full output rectangle for `exclusive_zone == -1` and the reserved usable rectangle otherwise.
- **PROTOCOL-DEFINED:** wlr-layer-shell uses `-1` for an anchored surface that should extend to its output edges without accommodating positive exclusive reservations; `0` is arranged against the usable area.
- **PROPOSED:** A dedicated `WallpaperSurfacePolicy` will own the complete Paper layer-shell contract and be consumed by `WallpaperSurfaceBundle`.
- **UNPROVEN:** Native visual coverage under the real Astrea Topbar and Dock cannot be claimed until an Astrea session is available.

## Selected policy

`WallpaperSurfacePolicy::background(QScreen *)` returns:

```text
scope                  = astrea-paper-wallpaper
layer                  = Background
keyboardInteractivity  = None
anchorTop/Bottom/Left/Right = true
exclusiveZone          = -1
margins                = QMargins()
screen                 = supplied QScreen
```

The policy is deliberately explicit, so a future change to the shared helper default cannot silently reintroduce zone `0`. `WallpaperSurfaceBundle` uses this policy and continues to use `QScreen::geometry()` for the physical output width/height hint and QQuickWindow size. It does not use `availableGeometry()`.

## Geometry and ownership

For a 1920x1080 output with a 45px Topbar reservation and a Dock reservation `D`:

```text
Paper surface:       0,0 1920x1080
usable normal area:  0,45 1920x(1080-45-D)
```

Positive Topbar/Dock zones continue to constrain normal and maximized windows. They do not constrain the Paper surface. One bundle remains associated with each current `QScreen`; all bundles display the same global effective Paper wallpaper and do not introduce per-output selection.

On `QScreen::geometryChanged`, `WallpaperSurfaceBundle::updateForScreen()` continues to update output properties, screen, and QQuickWindow size. The compositor's layer-shell configure is responsible for the final full-output surface extent.

## Tests

1. A deterministic `WallpaperSurfacePolicy` unit test asserts every field, including `exclusiveZone == -1` and zero margins.
2. The existing manager test continues to prove one bundle per current screen and effective-wallpaper forwarding; production bundle policy is tested independently without a compositor.
3. Typhon's generic layer-shell integration test covers a 1280x800 output, positive Topbar/Dock reservations, a zone `-1` Background surface, both creation orders, and resize to 1600x900. It asserts full Paper geometry and independently reduced usable geometry.
4. Existing zone-zero coverage remains unchanged and continues to prove that zone `0` uses the reserved usable area.

## Rejected alternatives

- Changing Typhon's zone-`0` semantics would break compliant clients and hide the Paper client bug.
- Removing Topbar/Dock reservations would break normal and maximized window placement.
- Moving wallpaper ownership into Typhon or adding Paper-specific namespace behavior would create a second wallpaper authority.
- Changing `QScreen::geometry()` to `availableGeometry()` would encode compositor reservations into the client hint and preserve the wrong abstraction.
- Changing the Paper image pipeline, API, catalog, or persistence is outside this geometry-only closure.
