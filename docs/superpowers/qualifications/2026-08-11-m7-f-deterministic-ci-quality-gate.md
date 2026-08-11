# M7-F Deterministic CI and Quality Gate Restoration Qualification

## Result

Local qualification passed on 2026-08-11. The repository was clean before the M7-F work at `06eb300`. The implementation commits are:

- `76f584f` — design, implementation plan, canonical CMake presets, and the M7-E Rust 2024 wording correction.
- `aee455a` — deterministic gate scripts, JUnit validator/tests, workflow, CI documentation, and the two clippy-only Rust expression fixes.
- `029706b` — ignore generated Python test caches and remove the accidentally staged cache file.
- `b6e45d3` — fix the workflow policy check so it cannot match its own regex literals.
- The final qualification-report commit is the last commit containing this document.

GitHub-hosted execution is pending first push of the workflow.

## Changed scope

Intended files are limited to:

- `CMakePresets.json`
- `.github/workflows/ci.yml`
- `tools/ci/check-ctest-junit.py`
- `tools/ci/run-cmake-gate.sh`
- `tools/ci/run-rust-gate.sh`
- `tools/ci/run-qml-gate.sh`
- `tools/ci/run-all-local.sh`
- `tools/ci/tests/test_check_ctest_junit.py`
- `docs/CI.md`
- M7-F design, plan, and qualification documents
- The M7-E plan’s stale `Rust 2021` wording, corrected to `Rust 2024`.
- Two behavior-preserving clippy simplifications in `Spotlight/backend/src/desktop/entries.rs`.
- `.gitignore` for generated Python caches.

No Typhon source, QML, protocol XML, launcher, systemd, or Rust search behavior was changed. No compiled or generated build output is tracked.

## Preset contract

The root preset file lists exactly these configure, build, and test presets: `debug`, `release`, `clang`, `asan`, `ubsan`, and `no-typhon`. All six used the Ninja generator and `build/<preset>` binary directory in fresh local trees. Every cache contained `BUILD_TESTING=ON` and `ASTREA_BUILD_TESTS=ON`.

- `debug`: Debug, Typhon ON, ASan OFF, UBSan OFF.
- `release`: Release, Typhon ON, ASan OFF, UBSan OFF.
- `clang`: Debug, explicit `/usr/bin/clang` and `/usr/bin/clang++`, Typhon ON.
- `asan`: Debug, Typhon ON, ASan ON, UBSan OFF.
- `ubsan`: Debug, Typhon ON, ASan OFF, UBSan ON.
- `no-typhon`: Debug, Typhon OFF.

CTest used one job in every preset. The total test count is not encoded as a correctness rule.

## Local commands and results

The helper unit suite was intentionally run before implementation and failed because the validator did not exist. After implementation:

```text
python3 -m unittest discover -s tools/ci/tests -p 'test_*.py'   PASS — 11 tests
bash -n tools/ci/*.sh                                          PASS
cmake --list-presets=all                                       PASS
actionlint .github/workflows/ci.yml                            PASS — 1.7.12
```

The exact full local command was run sequentially with a temporary Ninja 1.13.2 binary because Ninja was not installed system-wide:

```text
PATH=/tmp/eclipse-m7f-ninja:$PATH tools/ci/run-all-local.sh
```

It exited `0` after running the helper tests, the exact Rust gate, the QML gate, and all six CMake gates in order. A standalone fresh Debug gate and a standalone fresh no-Typhon gate were also run successfully.

## CMake matrix evidence

| Preset | CTest result | JUnit policy | Skips | Typhon target proof |
| --- | --- | --- | --- | --- |
| `debug` | 49/49 passed | `mode=typhon`, valid | 0 | all three concrete targets built |
| `release` | 49/49 passed | `mode=typhon`, valid | 0 | all three concrete targets built |
| `clang` | 49/49 passed | `mode=typhon`, valid | 0 | all three concrete targets built |
| `asan` | 49/49 passed | `mode=typhon`, valid | 0 | all three concrete targets built |
| `ubsan` | 49/49 passed | `mode=typhon`, valid | 0 | all three concrete targets built |
| `no-typhon` | 49/49 passed | `mode=no-typhon`, valid | 3 | intentionally disabled |

The no-Typhon skips were exactly:

- `typhon-protocol-integration-test`
- `typhon-shortcut-protocol-integration-test`
- `shell-unified-runtime-integration-test`

No other test skipped. The five Typhon-enabled gates built `typhon-protocol-integration-test`, `typhon-shortcut-protocol-integration-test`, and `shell-unified-runtime-integration-test` before the complete build, so a missing capability could not silently become a passing skip.

## Rust and QML evidence

The Rust gate printed `rustc 1.97.1` and `cargo 1.97.1`, then passed the exact locked fmt check, clippy `--all-targets -- -D warnings`, and locked tests. The backend test run passed 29 unit tests and 0 doctests. The only production edits required were removal of a needless `return` and replacement of a narrow `is_none` early-return block with `?`; behavior is unchanged.

The QML gate passed both generated targets:

- `astrea-shell_qmllint`, covering Shell, Dock, AltTab, and Spotlight registration.
- `astrea-settings-ui_qmllint`, covering Settings registration.

It used `QT_QPA_PLATFORM=offscreen` and did not add a duplicate file list or suppress existing qmllint warnings.

## Workflow and security validation

`.github/workflows/ci.yml` has pull-request, `main` push, and manual triggers; read-only `contents` permission; PR-only cancellation; policy, Rust, QML, CMake matrix, and always-running `required` jobs. The required job is the stable visible status `CI / required` and fails unless every dependency succeeds. ASan and UBSan have no `continue-on-error`.

Checkout `v7.0.1`, Qt action `v4.3.1`, and upload-artifact `v4.6.2` are pinned to full immutable SHAs with human-readable tag comments. Qt is explicitly version `6.8.3`; the workflow installs Ninja, Wayland development files, Rust, and the required Qt modules. No cache is enabled and successful compiled output is not uploaded. Failure-only matrix artifacts contain only CTest/CMake diagnostics with five-day retention. There is no `pull_request_target` or write permission.

The local host had Qt 6.11.1, CMake 4.4.2, GCC/G++ 16.1.1, Clang 22.1.8, Python 3.14.6, and no system Ninja or actionlint. A temporary Ninja 1.13.2 binary and temporary actionlint 1.7.12 binary were used for qualification; neither is part of the repository. The workflow’s optional actionlint step will report manual validation when the hosted runner does not already provide actionlint.

## Repository checks

- `git diff --check`: passed.
- Workflow policy scan for privileged triggers, write permissions, and mutable action refs: passed.
- Shell syntax: passed.
- Preset listing: passed with all six names in each category.
- Worktree: clean after the qualification commit.
- Branch: `main`, with no reset, clean, revert, rebase, amend, or force-push operation.

GitHub-hosted execution is pending first push of the workflow.
