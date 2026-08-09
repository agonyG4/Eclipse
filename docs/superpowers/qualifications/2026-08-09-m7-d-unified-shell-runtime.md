# M7-D Unified Eclipse Shell — Final Deterministic Qualification

Date: 2026-08-09

## Decision

M7-D implementation and deterministic qualification pass. Integrated native compositor qualification remains deferred.

## Baseline and commits

- Eclipse closure baseline: `85ec6c27a644e1b9938781be44d80bbba2b94234`.
- Typhon HEAD: `211dfe835d1d6d6faf449e7a0239d6f099945e6e`; no Typhon files were changed.
- Focused implementation: `43a2de3 fix(shell): own Spotlight shortcut in unified runtime`.
- Packaging/resource tooling: `9fd800e fix(systemd): migrate legacy shell services`.
- Teardown/sanitizer closure: `07ab01b fix(build): close sanitizer and disconnect teardown gaps`.

## Required build matrix

All entries were configured, built, and tested with serial `ctest --output-on-failure -j1` after the final implementation commit.

| Build tree | Configured | Passed | Failed | Skipped |
|---|---:|---:|---:|---:|
| `build/debug` | 49 | 49 | 0 | 0 |
| `build/release` | 49 | 49 | 0 | 0 |
| `build/clang` | 49 | 49 | 0 | 0 |
| `build/asan` | 49 | 49 | 0 | 0 |
| `build/ubsan` | 49 | 49 | 0 | 0 |
| `build/no-typhon` | 49 | 46 | 0 | 3 |

The no-Typhon skips are the expected `typhon-protocol-integration-test`,
`typhon-shortcut-protocol-integration-test`, and
`shell-unified-runtime-integration-test` cases. ASan and UBSan completed with
no sanitizer diagnostics.

## Focused and lifecycle evidence

The final Debug focused run passed 7/7:

- `typhon-shortcut-protocol-integration-test`
- `alttab-shortcut-routing-test`
- `shell-runtime-test`
- `shell-shortcut-dispatcher-test`
- `shell-unified-runtime-integration-test`
- `shell-systemd-migration-test`
- `shell-resource-measurement-test`

The real unified lifecycle fixture uses one `ShellRuntime` and fake compositor
and verifies authenticated startup, initial publication, Dock exact activation,
Alt-Tab exact activation, direct Spotlight shortcut dispatch, feature-gate
isolation, 100 Alt-Tab cycles, 100 Spotlight cycles, disconnect, fresh
authentication/registration on reconnect, snapshot rebuild, and clean shutdown.
The shared shortcut reconnect fixture restores four registrations through 100
connection generations.

## Packaging and resource evidence

- `Shell/tools/test-systemd-migration.sh` passed fresh, upgrade, and idempotent
  rerun cases using a fake `systemctl` and temporary unit directory.
- The Debug install tree contains exactly `astrea-shell.service` under
  `share/systemd/user`, plus `astrea-shell` and
  `astrea-migrate-shell-systemd`; no legacy resident unit payload is installed.
- All three legacy `--daemon` guards pass in every matrix entry.
- `Shell/tools/test-resource-measurement.sh` passed repeated multi-PID `/proc`
  sampling. The historical three-resident-process baseline is unavailable, and
  no live compositor-backed idle shell sample was available; no baseline is
  fabricated.

## Static, QML, and protocol gates

- `/usr/lib/qt6/bin/qmllint` over all Dock, Alt-Tab, and Spotlight QML files:
  exit 0; existing unqualified-access/info warnings only.
- Deterministic source-layout checks passed: `ShellRuntime.cpp` 232 lines
  (limit 250), `AstreaShellApplication.cpp` 328 lines (limit 350), shortcut
  dispatcher 35 lines, and exactly one systemd service payload.
- `git diff --check` passed. `git log --check` passed from both the closure
  baseline and the requested `6fc6f7fec12f78f7396ae57386753d2c4af2153f`
  range.
- Eclipse protocol XML SHA-256:
  `0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7`.
  `cmp` against `/home/agony/GitHub/Typhon/protocols/astrea-toplevel-management-v1.xml`
  returned 0.

## Ownership and compatibility audit

The resident `astrea-shell` composition has one application, one QML engine,
one `ShellRuntime`, one catalog, one identity resolver, one shared authenticated
Typhon session/reconnect owner, one shortcut client, one shortcut dispatcher,
and one IPC endpoint. Dock, Alt-Tab, and Spotlight remain feature-local
controllers sharing those owners. The legacy binaries remain non-resident
IPC clients, and only the unified service is packaged.

M7-A focus/raise/capture/resize/border behavior, v1 compatibility, and M7-B/C
exact-action semantics were not redesigned by M7-D. Native qualification is
outside this deterministic closure.

M7 A/B/C/D IMPLEMENTATION COMPLETE — INTEGRATED NATIVE QUALIFICATION DEFERRED
