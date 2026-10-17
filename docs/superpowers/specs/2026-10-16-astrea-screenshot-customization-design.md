# Astrea Screenshot Customization Design

## Goal

Give the screenshot thumbnail, enlarged preview, and Astrea Settings preview one consistent, configurable visual treatment, with reliable dismissal, shared live configuration, and unchanged screenshot file and drag payload data.

## Verified architecture

- `Shell/screenshot/ScreenshotThumbnail.qml` is a full-output Qt Quick `Window`. Its card is the pointer target and native drag source. It exports PNG bytes, a file URI, and a local path through Qt drag mime data. `ScreenshotInputRegionBridge` tracks the card bounds.
- `ScreenshotPreview.qml` is a separate full-output window. Its card animates between the thumbnail rectangle and a centered, aspect-fitted maximum size.
- `SettingsController` owns the navigation model and the page controllers. `SettingsNavigationCatalog` maps route IDs to resources. Settings QML resources are listed explicitly in `Settings/qml/CMakeLists.txt`.
- Wallpaper preview already demonstrates a rounded `QtQuick.Effects.MultiEffect` mask sourced from a rounded QML item.
- `Astrea.Effects/BackdropEffectSurface.qml` embeds a separate Wayland child window. Typhon's rounded backdrop coverage describes compositor backdrop effects, not screenshot image alpha. The authenticated named surface effect protocol selects named compositor effects. Neither is a screenshot-content mask.
- No screenshot-specific change needs a Typhon protocol or global Material Program change. Keep screenshot content, border, shadow, and UI motion in Qt Quick in the existing Shell windows; this preserves the current input and native drag surface.

## Component and rendering contract

Add `shared/qml/ScreenshotVisual.qml`, compiled from the same source into both `Astrea.Shell` and `Astrea.Settings`. It accepts an image URL, target geometry, corner radius, border visibility/width/color, and shadow visibility/strength. The component preserves image aspect ratio and masks image alpha with a rounded mask source using `MultiEffect`, following the Wallpaper page's mask technique. A matching outer rounded outline draws the border. Its root does not clip, so a shadow can extend beyond the image rectangle. The Shell card's position includes edge margin and shadow clearance, while the input bridge continues to describe the card's interactive image bounds.

The mask uses logical Qt Quick coordinates; Qt Quick owns device-pixel scaling. The screenshot controller's canonical image and PNG bytes are never modified for presentation. Rendering is limited to the small thumbnail/preview/settings-preview rectangles; there is no full-output effect layer and no compositor child window over the drag source.

## Lifecycle contract

The thumbnail lifecycle is `Hidden → Entering → Visible → Dismissing → Hidden`. The lifecycle is independent of `nativeFileDragging`, which pauses the idle countdown and input-region updates. Hover and pressed pointer state pause and resume the remaining idle time. A completed native copy drop requests animated dismissal; a canceled drag restores the same screenshot and resumes the remaining time.

All entry and exit work belongs to a monotonically increasing lifecycle token and screenshot image generation. Starting a newer capture stops old animations and invalidates old tokens. Only natural completion of the current exit run, with matching lifecycle token and image generation, may call `dismissThumbnail()`. Interrupted or restarted animations have no hide side effect. Disabled animations and zero-duration animations dismiss synchronously.

The local styles are `fade`, `scale-fade`, and `slide-fade`. Slide direction follows the selected corner. Entry/exit duration and easing are separate settings. The existing preview-to-thumbnail geometry transition remains in place and uses the shared visual primitive throughout.

## Shared configuration contract

The typed `ScreenshotConfiguration` model lives in `shared/screenshot/ScreenshotConfiguration.hpp/.cpp` and is owned by Shell's `ScreenshotController` and Settings' `SettingsController`. Both resolve `$XDG_CONFIG_HOME/AstreaOS/screenshot.json` through `QStandardPaths::GenericConfigLocation`, use the same defaults and validators, and watch the parent directory so atomic file replacement is observed.

The schema is a JSON object capped at 64 KiB. Known fields are strictly type-checked and bounded; invalid fields normalize independently to defaults. Valid unrelated root keys are retained when writing. Writes use `QSaveFile` and update only the changed known field. Malformed or oversized input is ignored and surfaced through status; it never crashes either app. A debounced watcher reloads external replacements. Reload is read-only, and identical content from the model's own atomic write is ignored, preventing write loops.

Defaults and limits:

| Field | Default | Bounds / values |
| --- | --- | --- |
| `cornerRadius` | 10 px | 0–64 px; render clamps to half the smaller frame dimension |
| `borderEnabled` | true | boolean |
| `borderWidth` | 1 px | 0–8 px |
| `borderColor` | Qt palette-derived light outline | valid QColor string, at most 64 characters |
| `shadowEnabled` | true | boolean |
| `shadowStrength` | 0.55 | 0–1 |
| `thumbnailSize.width` | 240 px | 120–480 px |
| `thumbnailSize.height` | 160 px | 80–320 px |
| `previewMaximumScale` | 0.78 | 0.5–0.9 of output bounds |
| `position` | `bottom-right` | four screen corners |
| `edgeMargin` | 20 px | 0–120 px |
| `animationsEnabled` | true | boolean |
| `entryAnimation` | `scale-fade` | `fade`, `scale-fade`, `slide-fade` |
| `exitAnimation` | `slide-fade` | `fade`, `scale-fade`, `slide-fade` |
| `entryDurationMs` | 160 ms | 0–1500 ms |
| `exitDurationMs` | 220 ms | 0–1500 ms |
| `entryEasing` | `out-cubic` | `linear`, `in-quad`, `out-cubic`, `in-out-cubic` |
| `exitEasing` | `out-cubic` | `linear`, `in-quad`, `out-cubic`, `in-out-cubic` |
| `showThumbnail` | true | boolean |
| `autoCopy` | true | boolean |
| `dragEnabled` | true | boolean |
| `displayDurationMs` | 5000 ms | 500–30000 ms |
| `clickAction` | `preview` | `preview`, `copy`, `dismiss` |

`thumbnailSize` is a maximum width/height box. Shell and Settings both preserve the source aspect ratio inside that box. `borderColor` is initialized from the active Qt application palette, matching the same effective color in the two processes. `resetToDefaults()` restores every supported field and persists it atomically.

During native drag, presentation changes may be recorded by the model but are applied to card geometry and input regions only after drag completion. The drag payload and drag image source are snapshotted before native drag begins; settings writes never replace the screenshot image or clipboard ownership.

## Settings page contract

Register the `screenshots` page under the existing `customization` hub and reuse the existing `ScrollPage`, `FormCard`, `SectionHeader`, `SettingRow`, slider, selector, toggle, theme, and native color dialog patterns. The page exposes appearance, position, animation, and behavior fields backed by `SettingsController.screenshots`. A compact mock desktop uses the same `ScreenshotVisual.qml` source as Shell, positions the mock card using the live settings, and offers an entrance/exit replay control. Reset and configuration status remain visible and localized. The page does not call or modify Typhon's global Material Program controls.

## Validation contract

Focused automated tests cover configuration defaults, bounds, malformed/oversized input, atomic replacement, unknown-key preservation, reset, and quick edits; rounded mask/border/shadow geometry and aspect fit; lifecycle token ownership, timeout, interaction pause, new screenshots during dismissal, disabled animation, and native drag restore/drop; Settings route/resource registration, live bindings, localization, and reset behavior. Run the relevant Shell and Settings Qt tests and CMake build from the established `/mnt/Aether/Desktop/GitHub/Eclipse-build` tree after verifying that exact output directory. Do not restart the running Shell or compositor.

The live desktop checks (Print Screen, clipboard, DnD to Zen/external apps, live Settings reload, maximum-size rounded clipping, and animated timeout dismissal) require a running desktop and remain separate from automated Qt tests.
