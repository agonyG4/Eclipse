# Zen Download Prototype Implementation Plan

> Execute inline using superpowers:executing-plans. User instructions prohibit
> subagents and branches; work on main and commit only this feature's files.

**Goal:** Make the proven Zen AutoConfig producer reproducible and usable.
**Architecture:** Isolated browser launcher + startup module + existing Unix API.
**Tech Stack:** Python stdlib, privileged Gecko JavaScript, existing Qt API.
**Spec:** ../specs/2026-10-19-zen-downloads-prototype-design.md

## Global constraints

All output under `/mnt/Aether/Desktop/GitHub/Eclipse-build`; no normal Zen changes.
No extension, XWayland, debugger, new build tree, branch or subagent.

## Review focus

Private downloads never leave the browser; unknown sizes never get fabricated
percentages; runtime links cannot redirect writes into the installed browser;
API failure must retain and replay snapshots; only owned processes are stopped.

## Task 1: Producer

- [ ] Add focused failing checks for public/private mapping, final target size,
  initial history filtering, removal, bounded retention and coalescing.
- [ ] Implement `downloadSnapshot(download, source, id)`, `ProgressRecords` and
  `start()` in `Downloads/zen/ZenDownloadPublisher.sys.mjs`.
- [ ] Add AutoConfig loader `Downloads/zen/astrea-downloads.cfg`.
- [ ] Run checks against the implementation and exercise real Gecko transport.

## Task 2: Launcher and documentation

- [ ] Add `Downloads/tools/zen-downloads-prototype.py` with prepare/run/list/watch,
  owned output marker, locking, isolated profile/runtime and API supervision.
- [ ] Check foreign/symlink output refusal and preparation idempotence.
- [ ] Prepare the stable Aether runtime and exercise real native Wayland downloads
  with the existing research probe; check replay after API restart.
- [ ] Document the two user commands and prototype limitations in Downloads/README.md.
- [ ] Review diff, verify output cleanup and commit the logical implementation.

## Execution record

Research design accepted by the user's request to make this prototype. Implementation
runs inline, as required by the user's no-subagent rule. Existing research tests
remain authorized by the preceding request to study and test this integration.
