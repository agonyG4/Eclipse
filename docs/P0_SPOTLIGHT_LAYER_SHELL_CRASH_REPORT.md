# P0 Spotlight Layer-Shell Crash Closure Report

Date: 2026-08-24

## Result

The crash was an Eclipse client-side activation race. Typhon was not the
responsible authority and was not modified.

The failing client called LayerShellQt `requestActivate()` while a Spotlight
`QWindow` was becoming visible. On a remap, that visibility transition occurs
before the fresh `zwlr_layer_surface_v1.configure`/ACK/buffer lifecycle is
complete. Spotlight also had a second activation path in the deferred
`focusRequested` handler. LayerShellQt 6.7.4 terminated in
`QWaylandLayerSurface::requestActivate()` with SIGSEGV.

The fixed client disables LayerShellQt visibility-time activation and owns one
activation request for the window lifetime. A focus request is held until the
first rendered frame, then activation and panel focus are performed once.
Subsequent remaps rely on the compositor's Exclusive keyboard focus and only
restore panel focus; they do not call LayerShellQt activation again. The same
activation boundary is applied to the Alt+Tab control surface.

## Repository state

- Eclipse baseline before the P0 work: `eb9d7e47d80feaa86bc91365753a7c56458df084`.
- Typhon baseline: `9d3fb34b45f6ce4ffc4582c3231e220b3643e959`.
- Typhon already contained unrelated dirty changes, including active-scene and
  output-geometry renames in `src/compositor/layer_shell.rs`. Those hunks were
  preserved exactly; no Typhon files were changed or staged.

## Failing evidence

The release build used the real Wayland session and LayerShellQt 6.7.4.
Repeated Spotlight show/hide reproduced the failure. For PID `132745`, the
backtrace was:

```text
SIGSEGV (SEGV_MAPERR)
LayerShellQt::QWaylandLayerSurface::requestActivate()
QQmlBinding::doUpdate()
SpotlightController::show()
ShellIpcServer::commandReceived()
```

The bounded `WAYLAND_DEBUG=client` trace showed this sequence:

1. Spotlight created an Overlay/Exclusive layer surface.
2. It committed bufferless, received configure, ACKed it, and committed a
   buffer.
3. Close destroyed the layer role and committed a NULL buffer.
4. Remap created a fresh layer role on the same `wl_surface` and committed
   bufferless.
5. Before the new configure/ACK was available, the QML visibility binding
   called `requestActivate()` and the client crashed. No Wayland protocol
   error preceded termination.

Removing only the direct visibility activation proved insufficient: the
queued controller focus signal could still call LayerShellQt before the new
surface was ready. A subsequent frame-bound attempt also showed that
re-requesting LayerShellQt activation after a NULL-buffer remap was unsafe in
this runtime. This established the final lifetime-scoped activation rule.

## Fix and regression coverage

Changed Eclipse files:

- `shared/platform/wayland/LayerShellHelper.cpp`: disable LayerShellQt's
  visibility-time automatic activation; activation is controller-owned.
- `Spotlight/qml/Main.qml`: remove visibility-time activation, wait for
  `frameSwapped`, issue activation once per window lifetime, and preserve
  panel focus on later remaps.
- `AltTab/qml/Main.qml`: apply the same safe activation boundary for the
  control surface.
- `Spotlight/CMakeLists.txt` and
  `Spotlight/tests/static/LayerShellActivationStructureTest.cmake`: enforce
  one activation route, no visibility-time activation, frame readiness, and
  lifetime-scoped activation for Spotlight and Alt+Tab.

The structural regression was red before the fix: the original Spotlight QML
contained two `requestActivate()` routes. It passes after the fix. No sleep,
permanent invisible Exclusive surface, window recreation, swallowed protocol
error, or focus-loss UX disablement was introduced.

## Fixed qualification

Build and focused tests:

```text
cmake --build build/release --target astrea-shell spotlight-tests -j2  PASS
ctest -R 'spotlight-tests|layer-shell-activation-structure'              PASS
```

The real IPC stress ran 105 complete show/hide cycles. The captured bounded
trace contained 106 Spotlight generations including the final manual open:

- 106 layer-surface creations and 106 destroys;
- 106 bufferless NULL unmaps;
- 106 configure events and 106 ACKs in the corresponding remap sequences;
- 106 keyboard enter events and 106 keyboard leave events;
- zero Wayland protocol errors;
- the same shell PID remained alive and status reported `running: true`.

The final open/close status checks reported `spotlight.open: true` while open
and `spotlight.open: false` after close.

## Controls

- Direct Spotlight show/hide and 105-cycle remap stress: **PASS**.
- Empty query/no-result open and close: **PASS**; no activation was attempted.
- Alt+Tab status/show/next/cancel IPC control: **PASS**; the shell remained
  alive. The live session exposed zero switchable windows, so no mapped
  Alt+Tab selection surface was available for a full visual selection check.
- Reopen during the close interval: **Not Run live**; controller timer
  behavior remains covered by existing focused controller tests.
- Astrea-menu popup Search handoff: **Not Run live**; no stable scoped input
  driver was available. The direct Spotlight path reproduced independently,
  so no speculative popup serialization was added.
- Weather-disabled live path: **Not Run live**; the existing config-sync test
  covers the disabled state without changing the user's live configuration.

## Typhon decision

Typhon's existing layer-shell authority already clears mapping eligibility,
initial configure/ACK state, pending configures, and stale ACK state on
NULL-buffer unmap. The live wire trace showed the required fresh configure,
ACK, and buffer sequence on every fixed remap, with no compositor protocol
error. Therefore the P0 is Eclipse-only and no Typhon commit is warranted.

## Final compatibility follow-up and closure qualification

Date: 2026-08-25

This section records the final compatibility correction and live handoff
qualification. It supersedes the provisional activation wording above where
the current source is more restrictive.

### LayerShellQt compatibility contract

- LayerShellQt `6.4.5` remains the minimum supported package.
- The `setActivateOnShow()` capability is enabled only at LayerShellQt
  `6.4.90` and newer. KDE's 6.4.5-to-6.4.90 changelog records the addition of
  request-activate-on-show support:
  <https://kde.org/announcements/changelogs/plasma/6/6.4.5-6.4.90/>.
- `ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW` is `OFF` for the `6.4.5` package
  and `ON` for `6.4.90`/`6.5.0` contract fixtures. The unsafe helper call is
  therefore not compiled for `6.4.5`; on newer packages it explicitly calls
  `setActivateOnShow(false)`.
- The final QML source has zero direct LayerShellQt `requestActivate()` calls
  in both Spotlight and Alt+Tab. Remaps restore panel focus only; there is no
  arbitrary popup delay, invisible mapped surface, or duplicate activation
  route.

### Verification matrix

- Static activation structure test: **PASS**.
- LayerShellQt capability contract tests: **11/11 PASS**.
- GNU Release build with real LayerShellQt `6.7.4`, including the shell and
  P0-relevant targets: **PASS**.
- GNU Release focused compatibility suite: **21/21 PASS**, including
  LayerShellHelper, Spotlight, LayerShell activation structure, Alt+Tab,
  Bar, shell IPC, shortcut dispatch, unified runtime integration, and legacy
  guards.
- Clang/Clang Release build with real LayerShellQt `6.7.4`: **PASS** for the
  built P0 targets (Spotlight, LayerShellHelper, Bar, shell IPC/shortcut/
  unified integration, and Alt+Tab controller/routing).
- AddressSanitizer and UndefinedBehaviorSanitizer smoke suites: **PASS** for
  LayerShellHelper, Spotlight, and shell IPC.
- Explicit no-LayerShell configuration: **PASS** for LayerShellHelper,
  Spotlight, and shell IPC tests; production LayerShell remains required by
  the shell runtime.
- Real LayerShellQt `6.4.5` configure and focused compilation: **PASS**. The
  configure log found the actual `6.4.5` library and reported
  `setActivateOnShow()` unavailable. Its Debug runtime suite was **18/21
  PASS**; the three failures were unrelated SIGSEGVs in
  `DesktopEntryCatalog::rebuildIndex()` at `watchPaths.append(...)` during
  Spotlight/Bar/unified-runtime catalog setup. No DesktopEntryCatalog change
  was made, and the current/newer Release suite and sanitizer suites remain
  green.
- Repository-wide Release CTest: **66/67 PASS**. The only failure was the
  unrelated `SettingsNavigationModelTest` catalogue-order/route expectation
  mismatch; all P0, LayerShell, Spotlight, Bar, Alt+Tab, shell, and Typhon
  contract tests passed.

### Live native IPC qualification

The current Eclipse Release shell was launched with the explicit Hyprland
backend and real LayerShellQt `6.7.4`, then exercised through its native IPC
clients. No synthetic pointer/keyboard input, `ydotool`, `wtype`, `xdotool`, or
screenshot tooling was used.

- Spotlight show/status/hide: **100/100 cycles PASS**.
- Immediate hide-to-show reopen during the close transition: **100/100 cycles
  PASS**.
- Alt+Tab show/status/hide control: **PASS**.
- The same shell PID remained alive throughout the qualification. No shell
  restart, SIGSEGV, Qt fatal message, or Wayland protocol error was observed.
- The process started for this qualification was stopped cleanly afterward.

The exact Astrea-menu-popup-to-Search pointer handoff remains **Manual User
Qualification Required**. The production-native seam is present, but this
qualification did not synthesize the bar-menu click or use a test-only
alternate handoff. A human should open the Astrea menu, click Search, and
confirm that the normal Spotlight surface appears and receives focus.

### Final status

**P0 SPOTLIGHT LAYER-SHELL CRASH: CLOSED for the Eclipse compatibility and
live IPC scope.** The LayerShellQt `6.4.5` safety boundary is explicit, newer
automatic activation is disabled, the focused current/newer build and runtime
qualification pass, the reopen regression is covered live, and Typhon remains
unchanged.
