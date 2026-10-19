# Browser Download Bridge Implementation Plan

> Execute inline with superpowers:executing-plans. No subagents or branches.

**Goal:** Feed real Zen/Brave download progress into the existing local API.

**Architecture:** Shared WebExtension adapter, Python stdio native host, and a
user-level host installer. Reuse the existing C++ API without a new network service.

**Tech stack:** Browser downloads/Native Messaging APIs, JavaScript, Python3, CMake.

**Spec:** `docs/superpowers/specs/2026-10-19-browser-download-bridge-design.md`

## Constraints and review focus

- Work on main, preserve unrelated edits, commit logical changes.
- Outputs and test scratch files use `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
- Check unknown totals, paused/cancelled/private downloads and profile collisions.
- Check disconnect/restart replay, malformed/partial native frames and automatic startup.
- Preserve browser-managed extension activation and signing requirements.

## Tasks

- [x] Add adapter behavioral tests and implement shared background logic in
  `Downloads/browser/background.js` with generated Zen/Brave manifests.
- [x] Add native framing integration tests; implement
  `Downloads/browser/native_host.py` and bounded API forwarding/autostart.
- [x] Add build packaging and `Downloads/tools/install-browser-bridge.py`; register
  native hosts for this user's installed browsers without enabling extensions.
- [x] Build in the verified Aether tree, run download tests, review sources, document
  activation steps and commit feature files only.

## Evidence

The Eclipse graph is ready but does not contain the newly committed Downloads
module (coverage freshness: not_tracked). Read the existing API source directly.
Official Chrome Native Messaging/service worker documentation and Mozilla downloads
and native manifests documentation confirm protocol framing and polling constraints.
The user's instruction to implement approves the previously proposed bridge design.

## Execution and review

- Adapter tests initially failed because the module did not exist. Six behavioral
  cases now cover mapping, byte polling, replay, privacy, pending filenames and stale
  replies from an old native port. The stale-reply regression failed before its fix.
- Native subprocess tests cover fragmented/oversized/truncated framing, validation,
  real API forwarding, automatic startup, reconnection and an already-running
  first-release API. The initial stderr inheritance issue was reproduced and fixed
  by fully detaching the service's standard streams.
- Added a small local `ping` operation for heartbeat detection. The native host also
  treats a first-release `unknown_operation` reply to ping as proof of liveness,
  preserving compatibility with an API already running in the user's terminal.
- Packaging/installer tests run against an isolated user root on Aether; existing
  foreign registrations are preserved and both destinations are preflighted.
- Native host manifests were registered for the user. Actual extension activation
  is left to the browser managers; no signing/security settings were changed.
- Real Brave 155.1.97.59 test: isolated headless profile, 4 MiB local HTTP download,
  observed fractions 0.1875, 0.359375, 0.546875, 0.734375, 0.921875, then completed.
- Real installed Zen test: isolated headless profile and temporary add-on, 4 MiB
  local HTTP download, observed fractions 0.03125, 0.171875, 0.328125, 0.578125,
  0.75, then completed. Its native manifest root was confirmed as `~/.mozilla`.
- Test profiles, downloaded fixtures and API processes were cleaned up. Diagnostic
  smoke-test scripts remain only in the stable Aether build tree.
- All build, runtime test and generated bundle output used the existing
  `/mnt/Aether/Desktop/GitHub/Eclipse-build` tree. No subagents or branches.
