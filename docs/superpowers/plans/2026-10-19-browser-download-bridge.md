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

- [ ] Add adapter behavioral tests and implement shared background logic in
  `Downloads/browser/background.js` with generated Zen/Brave manifests.
- [ ] Add native framing integration tests; implement
  `Downloads/browser/native_host.py` and bounded API forwarding/autostart.
- [ ] Add build packaging and `Downloads/tools/install-browser-bridge.py`; register
  native hosts for this user's installed browsers without enabling extensions.
- [ ] Build in the verified Aether tree, run download tests, review sources, document
  activation steps and commit feature files only.

## Evidence

The Eclipse graph is ready but does not contain the newly committed Downloads
module (coverage freshness: not_tracked). Read the existing API source directly.
Official Chrome Native Messaging/service worker documentation and Mozilla downloads
and native manifests documentation confirm protocol framing and polling constraints.
The user's instruction to implement approves the previously proposed bridge design.
