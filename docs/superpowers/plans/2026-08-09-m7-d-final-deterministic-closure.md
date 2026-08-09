# M7-D Final Deterministic Closure Implementation Plan

> **For agentic workers:** Execute this plan inline in the existing Eclipse checkout. Do not use subagents, branches, worktrees, amend, squash, reset, clean, or history rewrite.

**Goal:** Close M7-D by making Spotlight a first-class shared shortcut consumer and recording deterministic unified-runtime, packaging, resource, and build-matrix evidence without changing the accepted shell architecture.

**Architecture:** Extend the existing single `TyphonShortcutClient` registration set with `spotlight_toggle`, route events through a small shell shortcut dispatcher with independent feature gates, and keep `ShellRuntime` as the sole owner of transport and controllers. Add isolated installer/resource tools and a real runtime integration test that uses the existing fake Wayland protocol fixtures, then update the plan and qualification ledger from measured results.

**Tech Stack:** C++20, Qt 6 Core/Gui/Qml/Quick/Test, Wayland client/server fixtures, CMake/CTest, Bash, Linux `/proc`, systemd user-unit packaging, qmllint, ASan, and UBSan.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse` on the existing `main` branch.
- Do not modify `/home/agony/GitHub/Typhon`, protocol XML, M7-C exact-action semantics, or the accepted unified-process architecture.
- Keep one `QGuiApplication`, one `QQmlApplicationEngine`, one `ShellRuntime`, one authenticated Typhon transport/reconnect owner, one shortcut client, one catalog, and one identity resolver.
- Keep `spotlight_toggle` on the shared `TyphonShortcutClient`; disabling one feature must not stop shared shortcut transport.
- Do not spawn compatibility wrappers for normal protocol shortcuts; legacy wrappers remain nonpersistent IPC clients.
- Do not add artificial production delays, sleeps, queues, or asynchronous state machines for synchronous actions.
- Native qualification remains `DEFERRED`; no M8 work is allowed.
- All documentation is written in English and all completion claims cite actual command results.

---

### Task 1: Add shared Spotlight shortcut ownership and independent dispatch gating

**Files:**
- Modify: `shared/platform/typhon/TyphonShortcutClient.cpp`
- Modify: `shared/tests/TyphonShortcutProtocolIntegrationTest.cpp`
- Create: `Shell/platform/shortcut/ShellShortcutDispatcher.hpp`
- Create: `Shell/platform/shortcut/ShellShortcutDispatcher.cpp`
- Create: `Shell/tests/ShellShortcutDispatcherTest.cpp`
- Modify: `Shell/runtime/ShellRuntime.hpp`
- Modify: `Shell/runtime/ShellRuntime.cpp`
- Modify: `Shell/CMakeLists.txt`

**Interfaces:**
- `ShellShortcutAction mapShellShortcut(const QString &, const QString &, TyphonShortcutPhase)` returns `Ignore`, `AltTabNext`, `AltTabPrevious`, `AltTabCommit`, or `SpotlightToggle`.
- `ShellShortcutDispatcher(AltTabController *, SpotlightController *)` retains controller ownership in `ShellRuntime` and exposes `setAltTabEnabled(bool)`, `setSpotlightEnabled(bool)`, and `dispatch(...)`.
- `TyphonShortcutClient` registers exactly four `astrea-shell` names, including `spotlight_toggle`, with no second connection or reconnect owner.

- [x] Write the failing router/dispatcher and four-registration tests; run the focused targets and observe the missing `spotlight_toggle` behavior.
- [x] Implement the minimal mapping, registrations, dispatcher callbacks, and runtime wiring.
- [x] Keep the shortcut transport started for the lifetime of an active runtime; config toggles update dispatcher gates and only cancel the affected feature.
- [x] Run focused shortcut, dispatcher, runtime, and compatibility tests; verify repeated AltTab events and one Spotlight toggle, with no process launch.
- [x] Commit the focused production/test change as `fix(shell): own Spotlight shortcut in unified runtime` (`43a2de3`).

### Task 2: Add deterministic unified lifecycle, fan-out, and stress qualification

**Files:**
- Modify: `shared/tests/TyphonProtocolIntegrationTest.cpp`
- Modify: `shared/tests/TyphonShortcutProtocolIntegrationTest.cpp`
- Create: `Shell/tests/ShellUnifiedRuntimeIntegrationTest.cpp`
- Modify: `Shell/CMakeLists.txt`
- Modify: `Shell/tests/ShellRuntimeTest.cpp`

**Interfaces:**
- The integration test uses one fake compositor/session and the actual `ShellRuntime`, `TyphonToplevelConnection`, `TyphonShortcutClient`, Dock, AltTab, and Spotlight controllers.
- Test helpers pump Qt/Wayland events by condition, never by fixed production sleeps.
- The test records current connection generation, registration count, publication revision, feature-local state, and orderly shutdown state.

- [x] Write failing tests for direct Spotlight dispatch, both feature-gating permutations, publication fan-out/isolation, exact Dock/AltTab behavior, disconnect/reconnect, and clean shutdown.
- [x] Add deterministic fake-compositor event helpers for stale-generation rejection and fresh authentication/registration observation.
- [x] Run the focused integration target red, then implement only the test seams needed to exercise the production path.
- [x] Add 100 AltTab show/dismiss cycles and 100 Spotlight toggle/query/dismiss cycles with bounded-state assertions.
- [x] Add the highest cheap deterministic reconnect loop supported by the fake server, targeting 100 generations; record a lower count only with an evidence-based reason.
- [x] Run focused and serial full CTest suites; the ownership-boundary implementation and lifecycle tests are committed in `43a2de3`.

### Task 3: Add installed systemd migration and isolated packaging tests

**Files:**
- Create: `Shell/tools/astrea-migrate-shell-systemd`
- Create: `Shell/tools/test-systemd-migration.sh`
- Modify: `Shell/CMakeLists.txt`
- Modify: `Shell/packaging/systemd/astrea-shell.service`
- Modify: `docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell-runtime.md`

**Interfaces:**
- The migration script accepts `ASTREA_SYSTEMCTL` and `ASTREA_USER_UNIT_DIR` test overrides, retires only `astrea-dock.service`, `astrea-alt-tabd.service`, and `astrea-spotlightd.service`, reloads user systemd, and enables/starts only `astrea-shell.service`.
- The isolated test supplies a fake `systemctl`, checks fresh install, old-unit upgrade, and a second idempotent run without touching the real user manager.
- CMake installs the migration tool alongside the authoritative service and does not install legacy resident units.

- [x] Add the isolated migration test and validate its fresh, upgrade, and idempotent cases; an initial red transcript was not retained in this closure run.
- [x] Implement scoped, idempotent migration and install it as the packaging entry point without putting policy in `ShellRuntime`.
- [x] Run the isolated test twice, the legacy daemon guards, and an install-tree payload check.
- [x] Commit as `fix(systemd): migrate legacy shell services` (`9fd800e`).

### Task 4: Add reproducible resource measurement tooling

**Files:**
- Create: `Shell/tools/measure-shell-resources.sh`
- Create: `Shell/tools/test-resource-measurement.sh`
- Create: `Shell/docs/RESOURCE_MEASUREMENT.md`
- Modify: `Shell/CMakeLists.txt`
- Modify: `docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell-runtime.md`

**Interfaces:**
- The measurement tool accepts one or more `--pid` values, `--interval` seconds, and `--typhon-connections` observed owner count, and emits stable TSV/JSON-like fields for process count, PSS, private memory, RSS, threads, FDs, protocol-owner connections, and interval CPU.
- PSS comes from `/proc/<pid>/smaps_rollup:Pss`; private memory is `Private_Clean + Private_Dirty`; RSS and threads come from `/proc/<pid>/status`; FDs come from `/proc/<pid>/fd`; CPU is a delta over the documented interval.
- The methodology explicitly marks the old three-process baseline unavailable unless measured from real artifacts; no RSS sum is used as the primary metric.

- [x] Write and run the self-test for required fields and `/proc` parsing; an initial red transcript was not retained in this closure run.
- [x] Implement the read-only sampler and documentation; the self-test covers repeated multi-PID sampling.
- [x] Record `historical baseline unavailable`; no live compositor-backed unified shell was available for an equivalent idle sample.
- [x] Commit the tooling in the packaging qualification commit `9fd800e`.

### Task 5: Run the complete matrix and finalize plan/ledger evidence

**Files:**
- Modify: `docs/superpowers/plans/2026-08-09-m7-d-unified-shell-runtime.md`
- Modify: `docs/superpowers/qualifications/2026-08-09-m7-d-unified-shell-runtime.md`

**Interfaces:**
- The final ledger reports exact totals/passed/failed/skipped for `build/debug`, `build/release`, `build/clang`, `build/asan`, `build/ubsan`, and `build/no-typhon`, plus focused tests, qmllint, source-layout, whitespace/history checks, and protocol hash comparison.
- Plan checkboxes are marked only when implementation and evidence exist; any remaining item states its exact reason.
- Qualification status is implementation/deterministic `PASS` only when all mandatory deterministic gates pass; native status is `DEFERRED` for M7-A through M7-D.

- [x] Configure/build/run each required matrix entry serially and preserve exact output counts.
- [x] Run qmllint on Dock, AltTab, Spotlight, and changed QML; run source-layout/line-count checks, `git diff --check`, `git log --check 6fc6f7fec12f78f7396ae57386753d2c4af2153f..HEAD`, and the protocol SHA-256/cmp check.
- [x] Audit production ownership search results for one app/engine/runtime/session/shortcut/catalog/identity/reconnect owner and zero legacy resident service payloads.
- [x] Update the implementation plan and qualification ledger with measured evidence; the documentation commit is the remaining final commit in this closure.
