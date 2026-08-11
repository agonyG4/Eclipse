# M7-F Deterministic CI and Quality Gate Restoration Implementation Plan

> Execute directly in `/home/agony/GitHub/Eclipse` on `main`. Preserve newer changes. Do not use branches, worktrees, reset, clean, revert, amend, or history rewrite.

**Goal:** Restore a deterministic local and GitHub Actions quality gate for Eclipse while leaving Typhon, QML, protocol, launcher, and application behavior unchanged.

**Architecture:** Use six canonical CMake configure/build/test presets, shared local gate scripts, a standard-library CTest JUnit policy validator, authoritative generated QML lint targets, locked Rust quality commands, and a required GitHub Actions aggregator.

## Constraints

- CMake minimum remains 3.24; use preset schema version 5 and Ninja.
- Configure every preset with `BUILD_TESTING=ON` and `ASTREA_BUILD_TESTS=ON`.
- Typhon-enabled presets must build `typhon-protocol-integration-test`, `typhon-shortcut-protocol-integration-test`, and `shell-unified-runtime-integration-test`; no-Typhon must skip exactly those three CTest identities.
- CTest is serial within each configuration; the local wrapper is sequential.
- Do not hardcode a total test count.
- Rust commands, QML target names, action SHAs, and workflow status semantics are exact and reviewable.
- All new prose, comments, and scripts are English.

## Task 1: Add and validate the canonical preset matrix

**Files:** `CMakePresets.json`

- [ ] Add exactly six visible configure presets, six matching build presets, and six matching test presets.
- [ ] Set generator `Ninja`, binary directories `build/<preset>`, required cache options, compiler selection, and mutually exclusive sanitizer options.
- [ ] Validate with `cmake --list-presets` and inspect each preset’s resolved cache contract.

## Task 2: Implement the JUnit policy validator with tests first

**Files:** `tools/ci/tests/test_check_ctest_junit.py`, `tools/ci/check-ctest-junit.py`

- [ ] Add unittest coverage for valid Typhon, unexpected Typhon skip, missing Typhon integration test, valid no-Typhon, missing expected skip, extra no-Typhon skip, failure, error, malformed XML, zero tests, and duplicate identity.
- [ ] Run the new tests before implementation and record the expected red state.
- [ ] Implement a standard-library XML validator with explicit Typhon/no-Typhon policies and no frozen total.
- [ ] Run the focused test file and representative command-line checks until green.

## Task 3: Implement shared local gates

**Files:** `tools/ci/run-cmake-gate.sh`, `tools/ci/run-rust-gate.sh`, `tools/ci/run-qml-gate.sh`, `tools/ci/run-all-local.sh`

- [ ] Use strict shell mode and reject unknown CMake presets with exit status 2.
- [ ] Configure a unique mode-0700 runtime directory and clean it with a trap.
- [ ] Configure/build/test using the matching presets, emit JUnit, and validate it.
- [ ] Prove Typhon capability by building all three concrete integration targets in the five Typhon configurations.
- [ ] Run the exact locked Rust commands and the two generated QML lint targets.
- [ ] Make the all-local wrapper sequential and fail-fast.
- [ ] Run shell syntax validation and the focused helper tests.

## Task 4: Add the GitHub Actions workflow

**Files:** `.github/workflows/ci.yml`

- [ ] Add pull request, `main` push, and manual triggers with read-only permissions and PR-only cancellation.
- [ ] Add policy, Rust, QML, CMake matrix, diagnostics, and always-running required aggregator jobs.
- [ ] Install exact Qt 6.8.3 through the immutable-SHA-pinned Qt action and install native CMake/Ninja/Wayland prerequisites.
- [ ] Pin checkout and artifact actions by full SHA with release comments; do not enable caches.
- [ ] Run the same scripts used locally and upload only failure diagnostics.
- [ ] Validate YAML structure, action pinning, no privileged/pull-request-target usage, shell syntax, and preset listing. Document unavailable optional actionlint if necessary.

## Task 5: Document operation and qualification

**Files:** `docs/CI.md`, `docs/superpowers/qualifications/2026-08-11-m7-f-deterministic-ci-quality-gate.md`

- [ ] Document protection/status policy, toolchains, fast/full commands, preset semantics, skip rules, sanitizers, JUnit, and failure reproduction.
- [ ] Run fresh local qualification for all six presets, Rust, QML, and helper tests; record exact versions/results.
- [ ] Run `git diff --check`, inspect the diff and commit history, verify no Typhon/QML/protocol production changes, and confirm a clean worktree.
- [ ] Record GitHub-hosted execution as pending until the workflow is pushed.

## Completion evidence

The final report must include starting and final HEAD, commits, changed files, all preset names, local gate results, workflow jobs and pinning, tool versions, Rust/QML results, each preset’s result, skip policy, Typhon proof, workflow validation, git checks, worktree state, and the exact pending-push statement if applicable.
