# Astrea Shell Capability Authentication Design

## Problem

Typhon currently admits the private Astrea toplevel protocol by checking PID
ancestry. Eclipse AltTab and Dock are correctly managed by `systemd --user`,
but their Wayland clients are not descendants of the native shell process, so
Typhon rejects their manager binds. The live descendant launch test proved
this is the authorization boundary: AltTab opened a real mapped overlay with
two Typhon windows and Dock reached an authoritative ready snapshot.

## Design

Each Typhon compositor session creates a fresh cryptographically random
capability. Typhon stores only the capability verifier in memory and writes
the one-session capability to a mode-0600 file under the mode-0700
`$XDG_RUNTIME_DIR/astrea-shell/` directory. The file is atomically replaced
on each compositor start and removed during normal compositor shutdown. The
capability is never included in logs, diagnostics, crash messages, or status
payloads.

Typhon advertises a private `astrea_shell_auth_manager_v1` global. A client
reads the protected session capability file and authenticates once on its own
Wayland connection. A successful request authorizes only that connection's
exact Wayland `ClientId`; client disconnect teardown removes the authorization.
Invalid capability requests receive a bounded rejection event and do not
authorize the client.

The existing PID-descendant path remains valid for the native shell and its
children, but UID membership is never sufficient. Capability-authenticated
clients are admitted without PID ancestry. The private shortcuts, shell
control, and toplevel globals all use the same exact-client authorization
boundary.

Eclipse adds a shared Wayland authentication helper. AltTab's shortcut client
and the Typhon toplevel adapter authenticate before binding their private
globals. The systemd user units expose the protected runtime capability path
through `ASTREA_SHELL_CAPABILITY_FILE`; the helper also has the same protected
runtime-path default for session bootstrap environments. A daemon reconnect
reads the current file again, so a Typhon restart invalidates the old token
without requiring an Eclipse restart.

## Security invariants

- The capability has 256 bits of randomness and a bounded hexadecimal wire form.
- The capability file and parent directory are session-runtime state, not home-directory configuration.
- UID equality never grants Astrea shell privilege.
- Authorization is bound to the exact Wayland `ClientId`, not to PID, app ID, title, or a reusable credential.
- The capability is compared without early-exit equality and is never logged.
- Client disconnect removes authenticated-client state.
- A new Typhon process generates a new capability and replaces the previous file.

## Verification

Unit and protocol tests cover capability generation/storage, valid and invalid
authentication, UID-only rejection, exact-client authorization, disconnect
cleanup, and capability rotation. Eclipse fake Wayland servers cover the
authentication handshake before shortcut and toplevel binding. The release
build and serial CTest suite run from the existing Eclipse build directory.

