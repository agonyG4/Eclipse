# M7-C Eclipse Typhon Actions Qualification Ledger

## Scope

This ledger records the deterministic M7-C Eclipse implementation and closure.
It does not begin M7-D shell consolidation, does not migrate Spotlight policy,
and does not change Typhon.

| Item | Recorded value |
| --- | --- |
| M7-C implementation baseline | `d3321f3a2c3ca0b5327e38ecac23446903a3ad03` |
| Deterministic closure baseline | `6bdea1afa40b9690f63a6a4918850bbc57e4976f` |
| Final code HEAD before this ledger commit | `4281880` (`fix(dock): preserve equal-focus window order`) |
| Eclipse branch | `main` |
| Typhon pin | `211dfe835d1d6d6faf449e7a0239d6f099945e6` |
| Canonical protocol | `/home/agony/GitHub/Typhon/protocols/astrea-toplevel-management-v1.xml` |
| Eclipse protocol mirror | `/home/agony/GitHub/Eclipse/shared/platform/typhon/protocols/astrea-toplevel-management-v1.xml` |
| Protocol SHA-256 (both files) | `0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7` |
| Native qualification | `DEFERRED` |

## Closure commits

These commits were added on `main` without amending, squashing, resetting, or
rewriting earlier history:

| Commit | Purpose |
| --- | --- |
| `f490f02` | Exact authenticated `wl_client *`, cross-client denial, and reconnect identity regressions |
| `f671b4f` | Dock stale exact-target and no same-click launch fallback regression |
| `4281880` | Preserve stable snapshot order for equal focus serials |

## Deterministic evidence

The available toolchain is CMake 4.4.2, Unix Makefiles, GCC 16.1.1, Clang
22.1.8, Qt 6.11.1, Wayland scanner, and Rust/Cargo. Ninja installation was
attempted with the system package manager (`sudo pacman -S --needed --noconfirm
ninja`, then `yay -S --needed --noconfirm ninja`) but both were blocked by the
unavailable sudo password/terminal. Unix Makefiles were used for all required
configurations; no production behavior was delayed or queued to compensate.

| Focused test or check | Result |
| --- | --- |
| Typhon action-state primitive | 8 passed, 0 failed |
| Typhon protocol fixture contract | 3 passed, 0 failed |
| Shared Typhon protocol client | 15 passed, 0 failed |
| Wayland client/server integration | 14 passed, 0 failed |
| Dock Typhon runtime integration | 6 passed, 0 failed |
| AltTab Typhon source coverage | 19 passed, 0 failed |
| AltTab real QML selection regression | 3 passed, 0 failed |
| Changed AltTab QML `qmllint` | exit 0; existing informational/unqualified-access warnings only |
| Protocol mirror `cmp` | passed |

The Wayland integration covers v1 read-only, authenticated v2 on the same
native connection, exact `wl_client *` identity, cross-client capability
borrowing denial, fresh authentication after reconnect, unauthenticated v2
read-only behavior, XDG and managed-X11 targets, activate/minimize/restore/
close, accepted/no-change/unavailable, close acknowledgement independent of the
later `closed` event, and exact manager-owned completion. The action-state tests
cover the deterministic 64-entry bound, duplicate pending tokens, completion
release/reuse, stale generation completion, and disconnect cleanup. The Dock
integration covers exact activation, multiple/minimized targets, unavailable
results, and a stale target that produces zero action records and zero launcher
requests.

| Configuration | Build | Serial CTest |
| --- | --- | --- |
| `build/debug` | passed | 39 passed, 0 failed, 0 skipped |
| `build/release` | passed | 39 passed, 0 failed, 0 skipped |
| `build/clang` | passed | 39 passed, 0 failed, 0 skipped |
| `build/asan` | passed | 39 passed, 0 failed, 0 skipped |
| `build/ubsan` | passed | 39 passed, 0 failed, 0 skipped |
| `build/no-typhon` | passed | 37 passed, 0 failed, 2 skipped (the two Typhon Wayland integration tests) |

The canonical protocol mirror remained byte-identical to Typhon. Repeated
`wayland-scanner` warnings about the existing version ordering at XML line 127
were retained rather than suppressing or changing the canonical protocol.
Clang also reported the pre-existing unused lambda capture in
`AltTab/app/AltTabApplication.cpp`; neither diagnostic was introduced by this
closure.

The Eclipse worktree and Typhon worktree were clean after the closure commits,
and `git diff --check` plus `git log --check` passed.

## Policy

| Gate | Status |
| --- | --- |
| Implementation | `PASS` |
| Deterministic qualification | `PASS` |
| Native qualification | `DEFERRED` |

Native qualification was intentionally not run. It is not a deterministic M7-C
gate and does not block M7-D.

## Decision

M7-C IMPLEMENTATION COMPLETE — DETERMINISTIC QUALIFICATION PASSED — READY FOR M7-D
