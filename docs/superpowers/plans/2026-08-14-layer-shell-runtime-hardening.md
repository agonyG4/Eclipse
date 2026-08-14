# Layer Shell runtime hardening plan

## 1. Contract tests first

- Extend the deterministic CMake fixture with package-version metadata and an
  enabled/too-old failure case.
- Replace the obsolete global-bootstrap assertion with checks for the protocol
  preflight and the absence of unsupported LayerShellQt 6.4.5 API calls.
- Add Qt tests for the capability decision and the production surface policy.

## 2. Runtime capability and API compatibility

- Add a small helper that discovers `zwlr_layer_shell_v1` on Qt's native
  `wl_display` using a registry roundtrip.
- Run the probe after `QGuiApplication` starts and before QML surface creation.
- Remove `Shell::useLayerShell()`, `Window::setScreen()`, and
  `Window::setActivateOnShow()`; select the screen on the `QWindow` before
  obtaining the LayerShellQt wrapper.
- Fail startup clearly when the build, platform, or compositor capability is
  missing.

## 3. Build and diagnostics contract

- Require LayerShellQt >= 6.4.5 and a Wayland client development target for the
  production Layer Shell target.
- Align the root Qt minimum and CI documentation with the pinned dependency.
- Add truthful status fields for backend/protocol proof and configuration
  requests.

## 4. Verification and handoff

- Run focused Python and Qt tests, then clean Debug/Release builds and CTest.
- Run the no-LayerShell contract and QML/Rust gates that remain in scope.
- Inspect the live Wayland/Hyprland environment and run a bounded smoke test if
  it is safe and available.
- Review the diff for Typhon changes and commit with
  `fix(shell): harden LayerShellQt runtime integration`.
