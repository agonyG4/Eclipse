# AstreaOS native one-shot screenshot design

Eclipse receives screenshots through the existing authenticated
`TyphonSharedConnection`; it does not create another Wayland connection or
authenticate separately. `TyphonScreenCaptureClient` binds the v1 capture
manager and the relevant `wl_output` on each ready connection generation,
validates bounded RGBA8888 fd metadata, deep-copies into `QImage`, and ignores
stale events.

The shell-level `ShellShortcutDispatcher` owns the cross-feature action enum:
`Ignore`, `AltTabNext`, `AltTabPrevious`, `AltTabCommit`, `SpotlightToggle`,
and `ScreenshotCapture`. Alt+Tab retains only its own mapping; feature gates
are independent. `ScreenshotController` owns request lifecycle, the frozen
image, overlay visibility, selection-to-pixel mapping, cancellation, atomic
PNG persistence, and clipboard publication. The QML layer-shell overlay uses
scope `astrea-screenshot`, Overlay layer, all anchors, Exclusive keyboard
focus, and exclusive zone `-1`; it remains hidden until a complete image is
ready. The protocol XML is kept byte-identical with Typhon and checked by a
regression test.
