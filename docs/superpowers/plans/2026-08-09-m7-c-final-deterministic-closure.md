# M7-C Final Deterministic Closure Implementation Plan

> **For agentic workers:** Execute this plan inline in the existing Eclipse checkout. Do not use subagents, branches, worktrees, amend, squash, reset, or history rewrite.

**Goal:** Close the remaining deterministic M7-C gates without changing the accepted Typhon v2, AltTab, or Dock architecture.

**Architecture:** Strengthen only the fake Wayland test server's exact `wl_client *` bookkeeping, add positive/cross-client/reconnect identity assertions through the existing production client path, and add a Dock stale-target test using the existing local `ToplevelNotLive` error. Then run all requested build/test configurations and record actual evidence.

**Tech Stack:** C++20, Qt 6, Wayland client/server protocol fixtures, CMake, Make/Ninja, CTest, QML/qmllint, ASan, UBSan.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse` on existing `main`.
- Do not modify `/home/agony/GitHub/Typhon`.
- Do not begin M7-D or run native qualification.
- Preserve exact `WindowId` actions, manager-owned `action_done`, 64-entry generation-scoped state, and distinct local/server errors.
- Do not add automatic Dock launch fallback after activation failure.
- Use `build/debug`, `build/release`, `build/clang`, `build/asan`, `build/ubsan`, and `build/no-typhon`.

---

### Task 1: Exact Wayland client identity coverage

**Files:**
- Modify: `shared/tests/TyphonProtocolIntegrationTest.cpp`
- Test: `shared/tests/TyphonProtocolIntegrationTest.cpp`

**Interfaces:**
- The fake compositor records the authenticating `wl_client *`.
- Manager binding and every action request record/compare their owning `wl_client *` against that exact pointer.
- New test cases cover positive same-client use, cross-client denial, and fresh authentication after reconnect.

- [x] **Step 1: Add failing identity assertions first.**

Record the client in `authenticateRequest`, expose test-only accessors for
identity observations, and make `bindManager`/`recordAction` assert that the
manager/action resource belongs to the recorded client. Add tests for the
positive path, a second unauthenticated connection, and reconnect generation.

- [x] **Step 2: Run the focused integration test and verify the new assertions fail for the missing identity bookkeeping.**

Run the existing `typhon-protocol-integration-test` target or its direct test
binary. The expected pre-fix failure is an identity assertion, not a compile
failure.

- [x] **Step 3: Implement the minimum fake-server identity bookkeeping.**

Store the exact `wl_client *` only after successful authentication, clear it on
client destruction/reconnect, and reject or withhold action records from a
different client. Do not change production authentication state.

- [x] **Step 4: Run the focused integration test again.**

Require the positive authenticated action to complete exactly once, the
cross-client action to produce no authenticated completion, and reconnect to
require a new authentication before action readiness.

- [x] **Step 5: Commit the focused test change.**

```bash
git add shared/tests/TyphonProtocolIntegrationTest.cpp
git diff --cached --check
git commit -m "test(typhon): prove exact client identity for actions"
```

### Task 2: Dock stale-target race regression

**Files:**
- Modify: `Dock/tests/DockTyphonRuntimeIntegrationTest.cpp`
- Do not modify: `Dock/core/DockController.cpp` unless the new test exposes a regression.

**Interfaces:**
- The fake adapter supplies a live exact target, then removes it from the
  lower-layer protocol state before the click's action request while the Dock
  retains its already-observed runtime projection.
- The controller receives the existing local `ToplevelNotLive` result.
- The fake launcher remains untouched and must record zero requests.

- [x] **Step 1: Add the stale-target test before changing production code.**

Arrange an authoritative running application with one exact WindowId, mark that
exact adapter handle stale and emit its close event before the click, then
assert that no alternate action request and no launcher request occur.

- [x] **Step 2: Run the Dock integration test and verify the expected behavior.**

Run `dock-typhon-runtime-integration-test`; if it fails, fix only the existing
local stale-target reconciliation path and keep local errors distinct from
Typhon `Unavailable`.

- [x] **Step 3: Commit the focused Dock regression.**

```bash
git add Dock/tests/DockTyphonRuntimeIntegrationTest.cpp
git diff --cached --check
git commit -m "test(dock): cover stale activation target"
```

### Task 3: Full deterministic qualification and ledger

**Files:**
- Modify: `docs/superpowers/qualifications/2026-08-09-m7-c-eclipse-typhon-actions.md`
- Modify: `docs/superpowers/plans/2026-08-09-m7-c-final-deterministic-closure.md`

- [x] **Step 1: Verify the protocol identity gate.**

Run `sha256sum` for both XML files and `cmp`; both hashes must equal
`0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7`.

- [x] **Step 2: Configure and build each established build directory.**

Use the repository's normal options in `build/debug`, `build/release`,
`build/clang`, `build/asan`, `build/ubsan`, and `build/no-typhon`. If Ninja
cannot be installed because system credentials are unavailable, use the
available Unix Makefiles generator and record the exact package failure.

- [x] **Step 3: Run focused tests and every full CTest suite serially.**

Record exact totals, failures, and skips for Debug, Release, Clang, ASan,
UBSan, and no-Typhon. Run `qmllint` on both changed AltTab QML files.

- [x] **Step 4: Update the ledger with actual current evidence.**

Set the final policy to `Implementation: PASS`, `Deterministic: PASS`, and
`Native: DEFERRED` only if all deterministic commands pass. State explicitly
that native qualification does not block M7-D.

- [x] **Step 5: Perform final static and repository checks.**

Run `git diff --check`, `git log --check` from the recorded M7-C baseline,
verify a clean Eclipse worktree and clean Typhon worktree, and list every
closure commit without rewriting earlier history.

- [x] **Step 6: Commit the ledger correction.**

```bash
git add docs/superpowers/qualifications/2026-08-09-m7-c-eclipse-typhon-actions.md
git diff --cached --check
git commit -m "docs(shell): finalize M7-C deterministic qualification"
```
