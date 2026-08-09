# M7-C Eclipse Typhon Actions Qualification Ledger

## Scope

This ledger records the deterministic M7-C Eclipse implementation only. It
does not begin M7-D shell consolidation, does not migrate Spotlight policy,
and does not change Typhon.

| Item | Recorded value |
| --- | --- |
| Starting Eclipse HEAD | `d3321f3a2c3ca0b5327e38ecac23446903a3ad03` |
| Final implementation HEAD | `d32a4c5c1362d118aa04ba81da1875a7f7861aa8` (`fix(shared): reject invalid Typhon action tokens`) |
| Eclipse branch | `main` |
| Typhon pin | `211dfe835d1d6d6faf449e7a0239d6f099945e6` |
| Canonical protocol | `/home/agony/GitHub/Typhon/protocols/astrea-toplevel-management-v1.xml` |
| Eclipse protocol SHA-256 | `0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7` |
| Native qualification | `DEFERRED` |

## Deterministic evidence

The following focused tests were built and run manually with the checked-in
source and Qt 6.11.1 because the environment has no `cmake`, `ninja`, or
Python `pip` executable:

| Test | Result |
| --- | --- |
| Typhon action-state primitive | 8 passed, 0 failed |
| Typhon protocol fixture contract | 3 passed, 0 failed |
| Shared Typhon protocol client | 15 passed, 0 failed |
| Wayland client/server integration | 12 passed, 0 failed |
| AltTab Typhon source | 19 passed, 0 failed |
| AltTab real QML selection regression | 3 passed, 0 failed |
| Dock Typhon runtime integration | 5 passed, 0 failed |
| Changed QML `qmllint` | exit 0 |

The Wayland integration covers v1 read-only, authenticated v2 on the same
native connection, unauthenticated v2 read-only behavior, XDG and managed-X11
targets, activate/minimize/restore/close, accepted/no-change/unavailable,
close acknowledgement independent of the later `closed` event, and exact
manager-owned completion. The action-state tests cover the deterministic
64-entry bound, duplicate pending tokens, completion release/reuse, stale
generation completion, and disconnect cleanup. No sleeps, artificial delayed
production actions, or test-only action queues were added.

`git diff --check` passed for each staged commit. The Eclipse protocol mirror
was checked with both SHA-256 and `cmp` against the pinned Typhon XML. Typhon's
worktree remained clean.

## Unresolved locked gates

The full locked qualification cannot be claimed in this environment:

- `cmake` is unavailable, so the existing debug/release/clang/ASan/UBSan/
  no-Typhon configurations could not be reconfigured or built;
- `ninja` is unavailable, so the existing CMake build graph and full serial
  CTest suite could not be executed;
- ASan/UBSan full-suite evidence is therefore unavailable;
- no real native Typhon session was run, by design.

## Decision

M7-C NOT READY FOR M7-D — REMAINING GATES:

- install/provide CMake and Ninja, then reconfigure and build every locked
  Eclipse configuration;
- run the full serial CTest suite and ASan/UBSan suites with no failures;
- perform the separately authorized native session qualification while
  retaining `M7-C Native = DEFERRED` until that gate is explicitly passed.
