# M8-C Implementation Report

Date: 2026-08-21

Status: implemented and qualified for the available non-LayerShell test environment.

## Delivered

- Added the shared native StatusNotifier service, including deterministic watcher
  discovery/ownership, dual freedesktop/KDE watcher aliases, registration
  normalization, item-owner tracking, and health reporting.
- Added StatusNotifierItem proxies with asynchronous property loading, refresh on
  `PropertiesChanged`, activation/secondary activation/scroll dispatch, and stale
  reply generation guards.
- Added bounded ARGB32 network-order pixmap decoding, named-icon/theme fallback,
  attention and overlay composition, revisioned `image://astrea-tray` sources, and
  the QML image provider.
- Added a bounded DBusMenu tree/model with mnemonic handling, nested menus,
  `GetLayout`, `AboutToShow`, and menu event dispatch.
- Added the native tray QML surface, per-output stable-context popup menu, and a
  separate tooltip surface that is input-transparent and clamped to the output.
- Wired one process-wide `StatusNotifierService` through the shell runtime, app
  health JSON, QML context, and every bar output.
- Added focused C++ and QML smoke coverage plus implementation/design documentation.

## Verification

The main qualification build was configured with `ASTREA_ENABLE_LAYER_SHELL=OFF`,
tests enabled, and sanitizers disabled. It passed:

- `astrea-shell` build.
- Focused CTest set: 7/7 passed, including StatusNotifier, shell runtime, bar
  core, bar QML smoke, legacy QML guard, shell IPC, and shortcut dispatcher.
- Private-session-bus StatusNotifier test: 12/12 passed.
- GCC Debug + ASAN StatusNotifier test: 12/12 passed with leak detection
  disabled. LeakSanitizer with detection enabled reported 183 bytes from the
  installed NVIDIA GL driver during Qt startup; no project frame was reported.
- GCC Release + UBSAN StatusNotifier test: 12/12 passed.
- Clang Release StatusNotifier test: 12/12 passed.
- `git diff --check` passed.

The test environment was headless/offscreen and did not provide a real desktop
StatusNotifier host or a production LayerShell compositor, so no manual visual
Wayland verification or real third-party tray application fixture was available.
The live DBusMenu path is covered by bounded model/protocol tests; end-to-end
interaction with an external application remains an environment-level follow-up.
