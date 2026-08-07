# Astrea Shell Capability Authentication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline execution is selected for this task). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Typhon's UID/ancestry-only admission for Eclipse private globals with a per-session capability handshake that authorizes exact Wayland clients and supports systemd user daemon restarts.

**Architecture:** Typhon creates and rotates a 256-bit session capability in protected runtime state and advertises a private authentication manager. Eclipse reads the capability from `ASTREA_SHELL_CAPABILITY_FILE` or the protected runtime default, authenticates each private Wayland connection before binding shortcuts or toplevel management, and Typhon records the exact authenticated `ClientId`. Native shell descendants remain supported, UID-only admission is removed, and disconnect teardown clears client authorization.

**Tech Stack:** Rust 2024, Smithay `wayland-server`, Wayland XML/scanner, C++20, Qt 6, CMake, QtTest, existing serial Cargo/CTest release validation.

## Global Constraints

- Work directly on the existing `main` branches in `/home/agony/GitHub/Typhon` and `/home/agony/GitHub/Eclipse`; do not create branches or worktrees.
- Preserve all unrelated user changes and do not reset, clean, or discard worktree state.
- Keep capability bytes out of logs, diagnostics, crash output, status JSON, and test failure messages.
- Do not authorize the entire UID; only PID descendants or exact capability-authenticated Wayland `ClientId`s may bind private Astrea globals.
- Keep the existing protected private protocol semantics and M6 activation limitations unchanged.
- Use `/home/agony/GitHub/Eclipse/build/build-release` for Eclipse compilation and tests.
- Run focused failing tests before production changes, then the relevant Typhon tests, Eclipse tests, release build, and serial CTest suite.

---

### Task 1: Add the Typhon capability model and protected runtime handoff

**Files:**
- Create: `Typhon/src/compositor/astrea_shell_capability.rs`
- Modify: `Typhon/src/compositor/mod.rs`
- Modify: `Typhon/src/compositor/server.rs`
- Modify: `Typhon/docs/NATIVE_SESSION.md`
- Test: `Typhon/src/compositor/tests/astrea_shell_capability.rs`

**Interfaces:**
- `AstreaShellCapability::new(socket_name: &str) -> io::Result<Self>` creates fresh random capability state, atomically replaces the protected runtime file, and retains a redacted verifier in memory.
- `AstreaShellCapability::matches(&self, candidate: &str) -> bool` performs bounded constant-work comparison without exposing the secret.
- `CompositorState::set_astrea_shell_capability(capability: AstreaShellCapability)` and `CompositorState::astrea_shell_capability_matches(&self, candidate: &str) -> bool` provide protocol access.

- [ ] **Step 1: Write failing capability tests** for 256-bit token shape, mode-0700 directory/mode-0600 file, rotation replacing the previous token, mismatch rejection, and redacted `Debug` output.
- [ ] **Step 2: Run the focused tests** with `cargo test astrea_shell_capability -- --nocapture` and confirm they fail because the capability model is absent.
- [ ] **Step 3: Implement protected capability storage** using `/dev/urandom`, a fixed protected runtime directory under `$XDG_RUNTIME_DIR/astrea-shell`, atomic temporary-file rename, bounded hexadecimal wire encoding, and `Drop` cleanup without logging token contents.
- [ ] **Step 4: Wire one capability into each `OwnCompositorServer` session** before private-client dispatch and keep test-created `CompositorState` instances unauthenticated unless explicitly configured.
- [ ] **Step 5: Run the focused capability tests** and confirm they pass, then commit `feat(typhon): add per-session Astrea shell capability storage`.

### Task 2: Add the private Astrea shell authentication protocol

**Files:**
- Create: `Typhon/protocols/astrea-shell-auth-v1.xml`
- Create: `Typhon/src/astrea_shell_auth.rs`
- Create: `Typhon/src/compositor/protocols/shell_auth.rs`
- Modify: `Typhon/src/lib.rs` or the crate module root that exposes generated Astrea protocols
- Modify: `Typhon/src/compositor/server_globals.rs`
- Modify: `Typhon/src/compositor/protocols/versions.rs`
- Modify: `Typhon/src/compositor/mod.rs`
- Test: `Typhon/src/compositor/tests/shell_auth.rs`

**Interfaces:**
- Global: `astrea_shell_auth_manager_v1`.
- Request: `authenticate(capability: string)` followed by `destroy`.
- Events: `authenticated` and bounded `rejected`.
- `CompositorState::authenticate_astrea_shell_client(client_id: ClientId, candidate: &str) -> bool` records only the exact client after a successful capability match.

- [ ] **Step 1: Add failing protocol tests** for valid authentication, invalid capability rejection without authorization, repeated authentication bounded behavior, and authentication state cleanup on disconnect.
- [ ] **Step 2: Run the focused Typhon test target** and confirm the new protocol/global is missing and the tests fail for the intended reason.
- [ ] **Step 3: Add the XML and generated server/client module** with one-way authentication and no secret-bearing event payloads.
- [ ] **Step 4: Register the global and implement dispatch** so valid capability requests mark the exact `ClientId`, invalid requests only emit `rejected`, and no UID or PID fallback is added by this protocol.
- [ ] **Step 5: Run focused shell-auth tests and protocol contract tests**, then commit `feat(typhon): authenticate Astrea shell clients by capability`.

### Task 3: Replace UID-only admission with exact-client capability admission

**Files:**
- Modify: `Typhon/src/compositor/state/shortcuts.rs`
- Modify: `Typhon/src/compositor/server_toplevel.rs`
- Modify: `Typhon/src/compositor/protocols/shell_control.rs`
- Modify: `Typhon/src/compositor/mod.rs`
- Modify: `Typhon/src/compositor/state/client_lifecycle.rs`
- Test: `Typhon/src/compositor/tests/astrea_shortcuts.rs`
- Test: `Typhon/src/compositor/tests/toplevel_management.rs`
- Test: `Typhon/src/native_output/tests/shell_control.rs`

**Interfaces:**
- `astrea_shell_identity_is_authorized*` retains PID ancestry only and no longer accepts a UID authorization set.
- `astrea_shortcut_registration_allowed`, `astrea_shell_client_allowed`, and `astrea_toplevel_client_allowed` first accept exact capability-authenticated clients, then the existing native-shell PID ancestry path.

- [ ] **Step 1: Add failing regression tests** proving a same-UID non-descendant cannot bind shortcuts, shell control, or toplevel management, while a capability-authenticated exact client can.
- [ ] **Step 2: Run those focused tests** and observe the existing UID/ancestry behavior fail the new same-UID rejection assertions.
- [ ] **Step 3: Remove `astrea_shell_client_uids` and update admission checks** so `authorize_astrea_shell_pid` records only PID ancestry; capability authentication is the only non-descendant path.
- [ ] **Step 4: Clear authenticated-client state in the common client disconnect teardown** and keep private manager bookkeeping cleanup idempotent.
- [ ] **Step 5: Run the focused authorization suite and full Typhon test suite**, then commit `fix(typhon): require capability or shell ancestry for Astrea globals`.

### Task 4: Add Eclipse capability protocol/client support

**Files:**
- Create: `Eclipse/shared/platform/typhon/protocols/astrea-shell-auth-v1.xml`
- Create: `Eclipse/shared/platform/typhon/TyphonShellAuthenticator.hpp`
- Create: `Eclipse/shared/platform/typhon/TyphonShellAuthenticator.cpp`
- Modify: `Eclipse/shared/CMakeLists.txt`
- Modify: `Eclipse/shared/platform/typhon/TyphonShortcutClient.cpp`
- Modify: `Eclipse/shared/platform/typhon/TyphonProtocolAdapter.cpp`
- Test: `Eclipse/shared/tests/TyphonShellAuthenticatorTest.cpp`
- Test: `Eclipse/shared/tests/TyphonShortcutProtocolIntegrationTest.cpp`
- Test: `Eclipse/shared/tests/TyphonProtocolIntegrationTest.cpp`

**Interfaces:**
- `TyphonShellAuthenticator::authenticate(wl_display *display, QString *diagnostic) -> bool` reads the protected capability file, performs the auth manager handshake, and never includes the token in diagnostics.
- Default capability path: `$ASTREA_SHELL_CAPABILITY_FILE` when set, otherwise `$XDG_RUNTIME_DIR/astrea-shell/capability`.
- Authentication happens before `astrea_shortcuts_manager_v1` or `astrea_toplevel_manager_v1` binding on each connection generation.

- [ ] **Step 1: Add failing helper tests** for explicit/default capability path selection, missing capability handling, token redaction, and handshake success/rejection against a fake server.
- [ ] **Step 2: Extend the fake Wayland servers** with the auth global and deterministic test capability, then run the focused tests red to prove current clients bind without authenticating.
- [ ] **Step 3: Add scanner generation for the auth protocol** and implement the reusable synchronous Wayland handshake using a temporary registry listener and `wl_display_roundtrip` before the connection's normal event loop setup.
- [ ] **Step 4: Authenticate `TyphonShortcutClient` and `GeneratedTyphonProtocolAdapter`** before binding their private globals; report bounded degraded/unsupported diagnostics when authentication is unavailable or rejected.
- [ ] **Step 5: Run shared auth, shortcut, and toplevel integration tests green**, then commit `feat(shared): authenticate Eclipse clients with Typhon capability`.

### Task 5: Make systemd user services consume the session capability

**Files:**
- Modify: `Eclipse/AltTab/packaging/systemd/astrea-alt-tabd.service`
- Modify: `Eclipse/Dock/packaging/systemd/astrea-dock.service`
- Modify: `Eclipse/Spotlight/packaging/systemd/astrea-spotlightd.service`
- Modify: `Eclipse/AltTab/docs/TYPHON_BACKEND.md`
- Modify: `Eclipse/Dock/docs/TYPHON_RUNTIME_STATE.md`
- Modify: `Eclipse/README.md`
- Test: `Eclipse/shared/tests/TyphonShellCapabilityPathTest.cpp` or the existing packaging/config contract test target

- [ ] **Step 1: Add failing packaging assertions** that all Astrea systemd services expose the protected `%t/astrea-shell/capability` handoff and do not embed a capability value.
- [ ] **Step 2: Implement the shared `ASTREA_SHELL_CAPABILITY_FILE=%t/astrea-shell/capability` environment handoff** and document reconnect behavior across Typhon restarts.
- [ ] **Step 3: Run packaging/config tests** and commit `chore(systemd): pass Typhon shell capability path to Eclipse services`.

### Task 6: Rebuild, install service units, and validate live systemd restart behavior

**Files:**
- No source changes expected; use the existing build/install directories and runtime logs only.

- [ ] **Step 1: Build Typhon release** with `cargo build --release` and run its focused/auth/full tests.
- [ ] **Step 2: Reconfigure and build Eclipse** in `/home/agony/GitHub/Eclipse/build/build-release`, then run focused tests followed by serial `ctest`.
- [ ] **Step 3: Install Eclipse release artifacts and user units** to the existing user-local prefix without touching Hyprland or SDDM.
- [ ] **Step 4: Stop only the temporary descendant-stage Typhon/Eclipse processes, launch Typhon with the normal shell/session wrapper, and start the systemd user Eclipse units against the new capability file.
- [ ] **Step 5: Verify live acceptance:** AltTab status reaches `ready` and a real `--next` maps the overlay with Typhon windows; Dock status reaches `runtimeKnown: true`; restart each Eclipse daemon independently and verify it re-authenticates without restarting Typhon; rotate the capability by restarting Typhon and verify old sessions reconnect only with the new capability.
- [ ] **Step 6: Inspect both worktrees, remove only the temporary stage-1 wrapper/runtime logs if safe, and report automated gates separately from live TTY/DRM evidence.

