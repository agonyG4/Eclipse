# M7-C Eclipse Native Toplevel Actions Implementation Plan

> Execute this plan directly on the existing `main` checkout. Do not create a branch or worktree and do not use subagents.

**Goal:** Integrate Typhon M7-B protocol v2 into Eclipse's existing shared Typhon client, expose one typed exact-`WindowId` action API, and migrate AltTab and Dock without beginning M7-D.

**Starting state:** Eclipse `/home/agony/GitHub/Eclipse`, branch `main`, starting HEAD `d3321f3a2c3ca0b5327e38ecac23446903a3ad03`. Typhon dependency is `/home/agony/GitHub/Typhon` at `211dfe835d1d6d6faf449e7a0239d6f099945e6e`. The canonical protocol is `protocols/astrea-toplevel-management-v1.xml` with SHA-256 `0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7`.

**Constraints:** Do not modify Typhon, do not consolidate shell processes, do not migrate Spotlight policy, do not add PID/UID/title/app fallback mutation, do not change v1 semantics, and keep M7-C Native `DEFERRED`. Existing user changes must be preserved. Use focused, bisectable commits without amend, squash, reset, or history rewrite.

## Design

1. Keep the existing `TyphonToplevelConnection` as the shared transport owner. Its generated adapter authenticates the same native `wl_display` before manager binding, binds `min(advertised, 2)`, and exposes a typed capability state: disconnected/read-only v1/authenticating/action-ready v2/degraded. Each new connection generation starts unauthenticated; stale protocol objects and completions are ignored.
2. Add typed `ToplevelAction`, `ToplevelActionResult`, and local client-error types in shared Typhon code. Server results remain exactly accepted/no-change/unavailable; transport, authorization, unsupported protocol, stale handle, and local-capacity errors remain distinct.
3. Add one manager-owned action table with at most 64 pending entries. Each entry stores connection generation, a collision-safe `(token_hi, token_lo)`, exact `WindowId`, action, and a consumer completion token. Synchronous compositor actions reserve, send the exact handle primitive, process manager `action_done`, and release state. `close` is acknowledged when the graceful close request is issued; the later handle `closed` event is independent. No completed-token history is retained.
4. Resolve exact live handles by published `WindowId` in the current generation. Invalid targets, duplicate tokens, and capacity rejection settle authorized requests as semantic `Unavailable` without leaving pending state; unauthorized, unauthenticated, v1, disconnected, and invalid local requests return typed local errors without emitting `action_done`.
5. AltTab captures the selected `WindowId` at release, submits only `activate(WindowId)`, dismisses without blocking the GUI thread, and reconciles unavailable/stale outcomes without retargeting. Typhon remains the owner of minimized restore/focus/raise policy. Selection, hover, and active state remain separate, with a real QML three-row regression.
6. Dock projects exact live Typhon windows through the existing catalog/identity projector, chooses the most recent focus-serial candidate with deterministic tie-breaking (including minimized windows), and activates it without launching. Unavailable or stale-target outcomes reconcile only; they never launch in the same click. The existing full desktop filename `astrea-launch --desktop` fallback and launch tracking remain unchanged.

## Implementation sequence

### 1. Protocol fixture and contract gate

- Copy the canonical Typhon XML byte-for-byte into the existing Eclipse mirror path.
- Add a deterministic contract test that embeds the expected SHA-256 and verifies the fixture is byte-identical to the checked-in expected content without requiring Typhon at runtime.
- Update generated-client/server scanner inputs only through existing CMake conventions; do not commit generated build artifacts.
- Add failing tests first for v1/v2 negotiation and the protocol fixture/hash gate, then implement the fixture/build wiring.

### 2. Shared typed action transport

- Extend protocol types and `TyphonProtocolAdapter` with capability state, typed action request/result/error values, and manager-owned action completion signals/API.
- Extend the generated Wayland adapter to preserve same-display authentication, bind manager version 1 or 2 according to the server advertisement, retain exact handle proxies by current connection generation, and send only v2 requests after same-connection authentication.
- Route all v2 requests through the shared connection. Validate in this order: manager/resource version and authorization; token validity/duplicate/bound; exact `WindowId` handle resolution; pending admission; exact action primitive; manager completion.
- Ensure v1 remains read-only, no `since=2` request is sent unless negotiated v2 and authenticated, and no unauthorized/invalid-version request mutates token state.
- Make manager completion exact-once and generation-safe for accepted, no-change, unavailable, stale, duplicate, unknown, disconnect, and close-before/after-ack races.
- Keep diagnostics bounded and never log capability material.

### 3. Shared action lifecycle tests

- Add test doubles or the existing Wayland harness coverage for v1 read-only, v2 authenticated, and v2 unauthenticated paths.
- Qualify all four actions and all three server result mappings.
- Qualify 64-entry admission, deterministic 65th rejection, completion release/reuse, duplicate pending token, unknown/duplicate completion, stale-generation completion, target destruction, disconnect settlement, reconnect generation, and close independence.
- Test synchronous result handling without sleeps or artificial production queues; exercise the bounded primitive directly where the server normally completes immediately.

### 4. AltTab migration

- Add failing backend/controller tests for exact WindowId activation, minimized activation via one Typhon activate action, asynchronous dismissal, no retarget after unavailable, and selected-target disappearance before submission.
- Implement `TyphonWindowSource::activateWindow` through the shared typed action API and map local/server outcomes without collapsing errors.
- Preserve release-time exact target identity through controller/model mutation and keep completion manager-owned.
- Add the real QML model/delegate integration regression with three rows, one selected row, independently active and hovered rows, and deterministic A→B→C→A cycling.

### 5. Dock migration

- Add failing controller tests for no-live launch fallback, one live exact activation, multi-window focus-serial selection, minimized candidate selection, accepted/no-change/unavailable, stale target, and no same-click duplicate launch.
- Implement Dock action requests through the shared connection and existing runtime projection/catalog identity. Do not add Dock-side restore/focus/raise policy.
- Reconcile runtime state on action failure without launching; retain unresolved pins, launch correlation, watcher recovery, and full desktop filename fallback.

### 6. Documentation, commits, and qualification

- Update shared/AltTab/Dock docs only where required to describe the typed action boundary, exact target/error semantics, and M7-C qualification. Add/update the qualification ledger with starting/final HEADs, Typhon pin/hash, test counts, build configurations, sanitizer/QML/hygiene results, and `Native: DEFERRED`.
- Use focused commits such as protocol sync, shared client, shared lifecycle tests, AltTab, Dock, and qualification docs, adapting only if the actual diff requires fewer commits.
- Reconfigure and verify the existing `build/debug`, `build/release`, `build/clang`, `build/asan`, `build/ubsan`, and `build/no-typhon` directories where supported; run focused CTest, serial full CTest, qmllint for changed QML, ASan/UBSan, `git diff --check`, and `git log --check d3321f3a2c3ca0b5327e38ecac23446903a3ad03..HEAD`.
- Re-check Typhon/Eclipse protocol SHA-256 and `cmp`, inspect the final source for duplicate action paths, confirm no Typhon/M7-D/Spotlight policy changes, and only then choose the required final decision.

## Verification checkpoints

- Before production source edits: each new focused test exists and fails for the intended missing behavior.
- After each subsystem: focused target build and test pass, with `git diff --check`.
- Before completion: all available build/test/sanitizer/QML gates have recorded output; failures remain explicitly listed and cannot be reported as pass.
