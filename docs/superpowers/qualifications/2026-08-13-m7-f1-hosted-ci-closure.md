# Eclipse M7-F.1 Hosted CI Correctness and Security Closure Qualification

## Scope

M7-F.1 changes only hosted workflow bootstrap, action pinning, workflow-policy validation, committed-range whitespace validation, and their documentation/tests. No CMake preset, Rust/QML/native gate contract, production source, or Typhon implementation was changed.

## Implementation

- `jurplel/install-qt-action` remains v4.3.1 at `48d3ad6db93f3627c8ee7a0454bc6f3744f7e730`; Qt remains `6.8.3`.
- Both Qt installation sites request exactly `qt5compat qtshadertools` and verify `qtpaths --qt-version` plus `command -v qmllint`.
- Hosted evidence showed that the pinned action's current `aqtinstall 3.3.x` requires `linux_gcc_64` for the Qt 6.8.3 Linux package metadata; the original `gcc_64` input failed before installation. The workflow now uses `linux_gcc_64`.
- `actions/checkout` remains v7.0.1 at `3d3c42e5aac5ba805825da76410c181273ba90b1`, with `persist-credentials: false` on every checkout.
- `actions/upload-artifact` is v7.0.1 at `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`, retaining failure-only five-day diagnostics.
- `tools/ci/check-workflow-policy.py` is standard-library-only and replaces the workflow's inline regex policy.
- `tools/ci/check-git-whitespace.sh` validates PR, push, new-ref, and manual-dispatch commit ranges.

## Local verification

The following checks passed after the implementation commits `bb0242c` and `eba0b69`:

- `python3 -m unittest discover -s tools/ci/tests -p 'test_*.py'`: 31 tests passed.
- `bash -n tools/ci/*.sh`: passed.
- `python3 tools/ci/check-workflow-policy.py .github/workflows/ci.yml`: passed.
- `cmake --list-presets=all`: passed; all six canonical presets were listed.
- `actionlint` 1.7.12: passed.
- `tools/ci/check-git-whitespace.sh pr 8e56432 HEAD`, commit mode, and `git log --check 8e56432..HEAD`: passed.
- Rust gate: 29 tests and all doctests passed.
- QML gate: passed.
- Fresh local Debug CMake gate: 49/49 passed, zero skips.
- Fresh local no-Typhon CMake gate: 49/49 passed, three expected integration skips.

The full local wrapper was started but intentionally interrupted during its fresh Debug build; it is not reported as a passing wrapper run.

## Hosted verification

The normal push to `main` succeeded for both implementation commits.

Run [31737332649](https://github.com/agonyG4/Eclipse/actions/runs/31737332649) for `bb0242c` confirmed policy and Rust, but all Qt-dependent jobs failed in the pinned installer because `gcc_64` did not resolve the current Qt 6.8.3 Linux package metadata.

Run [31737891410](https://github.com/agonyG4/Eclipse/actions/runs/31737891410) for `eba0b69` confirmed the installer correction: policy, Rust, and QML passed, and all six CMake jobs reached CTest. The required aggregator failed because:

- `compositor-page-source-test` failed in all six CMake variants under the hosted CMake environment.
- `typhon-protocol-integration-test` also failed in debug, clang, and release at `reconnectRequiresFreshAuthenticationAndClientIdentity()`.
- The native failures are outside the M7-F.1 hosted bootstrap/security scope. No production or Typhon behavior was changed to conceal them.

## Result

The hosted workflow correctness/security closure is implemented and the Qt installer now reaches the native gates, but the hosted required check is not green. The qualification is therefore partial pending resolution of the existing hosted native test failures.
