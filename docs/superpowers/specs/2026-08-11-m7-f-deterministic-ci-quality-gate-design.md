# M7-F Deterministic CI and Quality Gate Restoration Design

## Status

Architecture approved by the M7-F request. This document records the implementation decisions and their reasons; it is not a request for another design review.

## Goal

Restore a deterministic, local/CI-parity quality gate for Eclipse without changing product behavior. The gate must prove that the Typhon-capable configurations actually build their integration executables, that the no-Typhon configuration reports only its three intentional skips, and that Rust and QML checks are run through the project’s authoritative build topology.

## Scope and invariants

- Change only CI, build-preset, gate-script, test-helper, and documentation infrastructure.
- Do not modify Typhon source, QML, protocol XML, launcher behavior, or Rust search behavior.
- Preserve the existing CMake test registration and use it as the source of truth.
- Keep CTest serial inside every configuration. Matrix configurations may run in parallel because each has its own build tree.
- Do not encode the current total test count. Test identity and skip policy are the stable contract.
- Do not add caches or successful-build artifact uploads.

## Preset topology

`CMakePresets.json` uses schema version 5, the newest schema supported by the project’s CMake minimum of 3.24. It defines exactly six visible configure, build, and test presets: `debug`, `release`, `clang`, `asan`, `ubsan`, and `no-typhon`. Every preset uses Ninja and `build/<preset>` as its binary directory. Configure presets explicitly enable `BUILD_TESTING` and `ASTREA_BUILD_TESTS`.

The ordinary presets use Typhon. `no-typhon` disables it. The sanitizer presets enable exactly one sanitizer each. The Clang preset explicitly selects `clang` and `clang++`. Test presets request failure output, one CTest job, and an error when no tests are discovered.

CTest JUnit output is passed by the gate script with `--output-junit`, because the `outputJUnitFile` preset field requires a newer preset schema than the project minimum permits.

## Gate scripts

The scripts under `tools/ci` are the executable local contract used by CI:

1. `run-cmake-gate.sh` accepts only the six canonical names, creates a unique mode-0700 `XDG_RUNTIME_DIR`, configures and builds the matching preset, proves the five Typhon-enabled configurations expose and build the three concrete integration targets, runs serial CTest, writes JUnit, and validates it.
2. `check-ctest-junit.py` is a standard-library validator. It rejects malformed, empty, duplicate, failed, errored, and policy-invalid JUnit. Typhon mode requires the three integration tests and zero skips. No-Typhon mode requires exactly those three skips and no others.
3. `run-rust-gate.sh` prints tool versions and runs the exact locked fmt, clippy, and test commands required by M7-F.
4. `run-qml-gate.sh` configures the debug preset and builds the generated `astrea-shell_qmllint` and `astrea-settings-ui_qmllint` targets with `QT_QPA_PLATFORM=offscreen`. It does not maintain a second QML file list.
5. `run-all-local.sh` runs the helper unit tests, Rust gate, QML gate, then all six CMake gates sequentially and stops at the first failure.

The JUnit validator has focused `unittest` coverage for valid modes and every rejection class, including duplicate testcase identity and missing required tests.

## Workflow topology

`.github/workflows/ci.yml` has the visible required status `CI / required`. It runs on pull requests, pushes to `main`, and manual dispatch. It has read-only repository permissions and PR-only cancellation. Independent jobs are:

- `policy`: helper tests, shell syntax, preset listing, whitespace checks, and optional actionlint when already available.
- `rust`: stable Rust with the Rust gate only.
- `qml`: Qt 6.8.3 with the required declarative, compatibility, shader-tools, and tools modules, then the QML gate.
- `cmake-matrix`: the six canonical presets with explicit GCC, Clang, Ninja, Wayland, Python, and Qt prerequisites. Its Typhon-enabled entries cannot downgrade to a skipped backend because the local gate builds concrete targets.
- `required`: an always-running aggregator that fails unless every required job succeeds.

Third-party actions are pinned by immutable full commit SHA and retain a tag comment. Qt is installed with the pinned `jurplel/install-qt-action` release because Ubuntu 24.04’s distro Qt is below Eclipse’s Qt 6.6 minimum. No cache is enabled. Failed CMake matrix entries upload only CTest/CMake diagnostics with short retention; successful compiled output is not uploaded.

## Security and reproducibility

The workflow does not use `pull_request_target`, privileged permissions, mutable action refs, or implicit caches. Qt’s exact 6.8.3 version is selected explicitly, while the action itself is SHA-pinned. A fresh per-process runtime directory prevents cross-job socket collisions and cleanup is trap-protected. CI and local gates invoke the same scripts and presets.

## Documentation and qualification

`docs/CI.md` documents branch protection expectations, toolchain setup, the fast/full local gates, sanitizer and no-Typhon semantics, JUnit output, failure reproduction, and the required status name. A qualification report records the starting and final commits, all local commands and results, tool versions, workflow validation, the no-Typhon skip set, Typhon target proof, and any GitHub-hosted execution that remains pending because this change is not pushed.
