# M8-A.3 Astrea TopBar Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the AstreaOS TopBar's component hierarchy, geometry, interaction states, and popup presentation onto Eclipse's existing native Layer Shell and system-service architecture.

**Architecture:** Keep `BarSurfaceManager`, one `BarSurfaceBundle` per `QScreen`, `BarLayoutMetrics`, `BarPopupController`, `ThemeController`, `WorkspaceModel`, and the native audio/network/Bluetooth services as the only backend authorities. Restore the reference's small QML components (`IndicatorButton`, `TopbarIndicator`, `BarSegment`, per-service indicators, popup cards, menu items, and workspace delegates) as the presentation layer, with `BarPopupController::PopupKind` extended for service-specific popup intent.

**Tech Stack:** Qt 6 QML/Quick, C++/Qt `QObject` controllers and models, CMake, Qt Test, Python legacy-QML guard.

## Global Constraints

- Preserve Layer Shell integration, output ownership, lifecycle management, native services, C++ controllers, and multi-monitor behavior.
- Keep `BarLayoutMetrics` as the geometry authority; do not duplicate production geometry constants across QML components.
- Keep `BarPopupController`'s `closed -> opening -> visible -> closing -> unmapped` lifecycle and native popup overlay surface.
- Use `AudioService`, `NetworkService`, `BluetoothService`, `BarClockService`, `WorkspaceModel`, `ThemeController`, and `BarController` directly; do not add Python, polling, subprocess, status-daemon JSON, Quickshell, or fake production data.
- Do not add Control Center, System Tray, notification center, volume OSD, or another shell/daemon architecture.
- Keep capability-gated Astrea actions disabled/hidden when the native controller reports them unavailable.
- Preserve unrelated dirty worktree changes and stage only files belonging to this change plus this plan/report.

---

### Task 1: Restore the reference QML component primitives and geometry mapping

**Files:**
- Create: `Bar/qml/components/IndicatorButton.qml`
- Create: `Bar/qml/components/TopbarIndicator.qml`
- Create: `Bar/qml/components/PopupHeader.qml`
- Create: `Bar/qml/components/MenuSeparator.qml`
- Modify: `Bar/qml/components/ShellBarTheme.qml`
- Modify: `Bar/qml/components/BarSegment.qml`
- Modify: `Bar/qml/components/WorkspaceStrip.qml`
- Modify: `Bar/qml/LauncherSurface.qml`
- Modify: `Bar/qml/StatusSurface.qml`
- Modify: `Bar/qml/components/Clock.qml`
- Modify: `Bar/qml/components/NetworkIndicator.qml`
- Modify: `Bar/qml/components/BluetoothIndicator.qml`
- Modify: `Bar/qml/components/VolumeIndicator.qml`
- Modify: `Bar/qml/AstreaMenu.qml`
- Modify: `Bar/qml/components/MenuAction.qml` (remove the text-only row implementation after `AstreaMenu` switches to icon-bearing menu items)
- Modify: `Bar/qml/components/PopupCard.qml`
- Test: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- `IndicatorButton.qml` exposes `fixedWidth`, `horizontalPadding`, `backgroundMargin`, `backgroundRadius`, `active`, `hovered`, `pressed`, `spacing`, `clicked(real anchorX)`, and `wheel(var event)`.
- `TopbarIndicator.qml` consumes `popupController` and a service-specific popup kind and emits `activated(real anchorX)` after routing the click to the native controller.
- `WorkspaceStrip.qml` consumes the existing `WorkspaceModel` roles (`id`, `active`, `occupied`, `urgent`) and calls a model-facing activation hook only when the model exposes one; no synthetic workspace data is introduced.
- `ShellBarTheme.qml` maps the reference semantic names (`background`, `border`, `barBorderHover`, `hover`, `pressed`, `active`, `text*`, `icon*`, spacing, typography, and animation tokens) to shared `ThemeController` state while retaining current `shell*` aliases.

- [ ] Write failing QML smoke assertions for the reference icon font/glyphs, passive outer status pill, dedicated launcher click target, workspace hover/click hitbox, indicator states, and `DATE | TIME` sizing/typography.
- [ ] Run `cmake --build build/release --target bar-qml-smoke-test -j2` and `QT_QPA_PLATFORM=offscreen build/release/Shell/bar-qml-smoke-test`; confirm the new assertions fail for the current text-only/menu-wide-click implementation.
- [ ] Implement the primitives and update the existing components to consume theme/geometry tokens, use actual Astrea glyphs, remove the permanent network connection-name label, restore the Bluetooth scanning ring, restore reference volume thresholds/wheel routing, and make only the Astrea icon and each individual indicator clickable.
- [ ] Re-run the focused QML test until the new assertions pass, then run the existing `bar-qml-legacy-guard` test to confirm no forbidden backend bridge was added.

### Task 2: Extend the native popup intent and restore popup presentation

**Files:**
- Modify: `Bar/core/BarPopupController.hpp`
- Modify: `Bar/core/BarPopupController.cpp`
- Modify: `Bar/qml/PopupOverlaySurface.qml`
- Modify: `Bar/qml/components/PopupCard.qml`
- Modify: `Bar/qml/AstreaMenu.qml`
- Modify: `Bar/qml/ClockPopup.qml`
- Create: `Bar/qml/NetworkPopup.qml`
- Create: `Bar/qml/BluetoothPopup.qml`
- Create: `Bar/qml/VolumePopup.qml`
- Modify: `Bar/cmake/BarResources.cmake`
- Modify: `Bar/platform/wayland/BarSurfaceBundle.cpp` only if the new QML properties require an existing native injection adjustment
- Test: `Bar/tests/BarCoreTest.cpp`
- Test: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- Extend `BarPopupController::PopupKind` with `Network`, `Bluetooth`, and `Volume`, preserving existing enum values for `None`, `AstreaMenu`, and `Clock`.
- Add `toggleNetwork(int)`, `toggleBluetooth(int)`, and `toggleVolume(int)` as QML-invokable wrappers over `toggle(PopupKind, int)`.
- Each popup consumes its native service object and closes through `popupController.close()`; service operations are only `NetworkService::requestWifiScan/setWifiEnabled`, `BluetoothService::requestScan/releaseScan/setPowered/connectDevice/disconnectDevice`, and `AudioService::adjustVolume/setMuted/setDefaultOutput`.
- `PopupOverlaySurface.qml` keeps the overlay mapped while `closing`, runs reference `opacity 0->1`/`scale 0.97->1` enter and matching exit transitions, and calls `completeClose()` only for the still-current popup kind.

- [ ] Add failing controller tests for all three new popup kinds, their toggle wrappers, replacement behavior, and close-retains-surface lifecycle.
- [ ] Add failing QML assertions for popup anchors/clamps and required popup object visibility for Astrea, Clock, Network, Bluetooth, and Volume.
- [ ] Implement the native popup-kind extension, reference card padding/radius/border/spacing, Astrea icon/separator/menu grouping and capability gates, clock card, network traffic card, Bluetooth model/scanning card, and volume slider/mute card without subprocesses.
- [ ] Add all popup QML files to the shared resource manifest and run focused core/QML tests plus the legacy guard.

### Task 3: Add state and geometry regression coverage

**Files:**
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`
- Modify: `Bar/tests/BarCoreTest.cpp`
- Modify: `Bar/tests/check_bar_qml.py` only if the guard needs an explicit native-service allowlist entry

- [ ] Cover Network disconnected/Wi-Fi/wired icon states without a connection-name label.
- [ ] Cover Bluetooth unavailable/powered-off/powered-on/connected/scanning states, including the scanning animation properties.
- [ ] Cover audio muted/zero/low/medium/high glyph thresholds, wheel deltas, and popup slider/mute routing.
- [ ] Cover Astrea-only launcher click behavior, per-indicator popup routing, interactive workspace delegates, status/launcher spacing, narrow-output popup clamps, and absence of duplicated card padding in the geometry formulas.
- [ ] Run the focused tests under `QT_QPA_PLATFORM=offscreen` and keep all existing M8-A/M8-B assertions green.

### Task 4: Report, validate, and commit the coherent parity change

**Files:**
- Create: `docs/M8-A.3_IMPLEMENTATION_REPORT.md`

- [ ] Record reference components compared, components restored, behavior differences fixed, architecture preserved, tests added, validation performed, and remaining differences; explicitly avoid claiming pixel parity without a live manual visual comparison.
- [ ] Build and test the existing Debug and Release directories when available:
  `cmake --build build/debug --target astrea-shell bar-core-test bar-qml-smoke-test -j2`,
  `cmake --build build/release --target astrea-shell bar-core-test bar-qml-smoke-test -j2`,
  `ctest --test-dir build/debug --output-on-failure`, and
  `ctest --test-dir build/release --output-on-failure`.
- [ ] Run available sanitizer configurations, the focused CTest set, `git diff --check`, and inspect `git diff --stat`/`git status --short` for scope isolation.
- [ ] Stage only the M8-A.3 files, create one commit with the report, and record the resulting commit hash.
