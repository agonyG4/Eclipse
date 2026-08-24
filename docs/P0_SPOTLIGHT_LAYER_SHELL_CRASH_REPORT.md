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
