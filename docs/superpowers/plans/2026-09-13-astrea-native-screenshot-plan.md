# AstreaOS native screenshot implementation plan

1. Keep `shared/platform/typhon/protocols/astrea-screen-capture-v1.xml`
   byte-identical to Typhon's protocol and add client/server scanner outputs,
   build dependencies, and an XML drift regression test.
2. Extend `TyphonShortcutClient` with `screenshot_capture` and implement
   `TyphonScreenCaptureClient` on the existing `TyphonSharedConnection`; bind
   manager/output per generation, validate bounded RGBA8888 fd metadata, deep
   copy `QImage`, close/unmap safely, and ignore stale events. Add failing
   tests first for valid/malformed/failure/reconnect behavior.
3. Refactor `ShellShortcutDispatcher` to own the exact shell action enum;
   leave Alt+Tab mapping Alt+Tab-specific and prove all feature gates are
   independent.
4. Add `Shell/screenshot/ScreenshotController` and its
   `QQuickImageProvider`. Implement frozen-image lifecycle, generation-busted
   source, logical-to-pixel crop mapping/clamping, six-unit full-image
   fallback, cancellation, collision-safe `QSaveFile` PNG persistence, and
   Qt clipboard publication under tests.
5. Add the screenshot QML surface and configure the requested layer-shell
   policy. Keep it hidden until a complete ready image exists; implement drag,
   border/dimming/crosshair, Escape, and right-click cancellation.
6. Wire runtime/application ownership and the unified shortcut -> request ->
   ready -> overlay flow, then run focused tests followed by the existing
   configure/build/CTest workflow.

Do not create a second Wayland connection, add an external capture utility, or
commit changes. Use the existing build directory and `rtk` wrappers.
