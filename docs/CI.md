# Eclipse CI and local quality gates

The protected status is `CI / required`. Branch protection should require that check for pull requests and changes to `main`. The workflow runs for pull requests, pushes to `main`, and manual dispatch. It has `contents: read` permissions, cancels only superseded pull-request runs, and has an always-running aggregator so a skipped or canceled dependency cannot look like a successful required check.

## Prerequisites

The project requires CMake 3.24 or newer, Ninja, Qt 6.6 or newer, a C++20 compiler, Wayland client/server development files and scanner, Python 3, and stable Rust. The hosted workflow installs Qt 6.8.3 with the pinned Qt action and installs the required Qt declarative, Qt5Compat, shader-tools, and tools modules. Native Wayland packages are installed explicitly.

The local commands use the tools already on `PATH`. A temporary Ninja binary is sufficient when a distribution package is unavailable.

## Canonical CMake presets

The root `CMakePresets.json` defines exactly these configure, build, and test presets:

- `debug`: Debug, Typhon enabled.
- `release`: Release, Typhon enabled.
- `clang`: Debug with explicit `clang` and `clang++`, Typhon enabled.
- `asan`: Debug with AddressSanitizer enabled and UBSan disabled.
- `ubsan`: Debug with UBSan enabled and ASan disabled.
- `no-typhon`: Debug with Typhon disabled.

All presets use Ninja, write to `build/<preset>`, and enable `BUILD_TESTING` and `ASTREA_BUILD_TESTS`. CTest runs with one job and prints output on failure. The total test count is intentionally not part of the contract.

## Local gates

Run the fast gates while iterating:

```text
tools/ci/run-rust-gate.sh
tools/ci/run-qml-gate.sh
```

The Rust gate prints `rustc` and `cargo` versions, then runs locked format checking, clippy with `-D warnings`, and tests. The QML gate builds the generated `astrea-shell_qmllint` and `astrea-settings-ui_qmllint` targets with `QT_QPA_PLATFORM=offscreen`; those targets own the authoritative QML file registration for Shell, Dock, AltTab, Spotlight, and Settings.

Run the complete local contract with:

```text
tools/ci/run-all-local.sh
```

It runs the JUnit helper tests, Rust, QML, and then `debug`, `release`, `clang`, `asan`, `ubsan`, and `no-typhon` sequentially. Individual CMake runs are available with `tools/ci/run-cmake-gate.sh <preset>`. Unknown preset names return exit status 2.

Each CMake gate creates a unique mode-0700 `XDG_RUNTIME_DIR`, cleans it with a trap, configures with the matching preset, builds, runs serial CTest, writes `build/<preset>/ctest.junit.xml`, and validates that report. The five Typhon-enabled presets must build these concrete targets before the full build:

```text
typhon-protocol-integration-test
typhon-shortcut-protocol-integration-test
shell-unified-runtime-integration-test
```

Their CTest identities therefore cannot silently downgrade to skipped tests. The `no-typhon` preset must report exactly those three identities as skipped and no others. The standard-library validator rejects malformed or empty JUnit, duplicate testcase identities, failures, errors, missing required tests, unexpected skips, and missing or extra no-Typhon skips.

## Reproducing a failure

Run the same named gate locally. For example:

```text
tools/ci/run-cmake-gate.sh asan
```

Inspect `build/asan/Testing/Temporary/LastTest.log`, `build/asan/ctest.junit.xml`, and `build/asan/CMakeCache.txt`. The workflow uploads these files only when a CMake matrix job fails, with five-day retention; successful compiled output is not uploaded. ASan and UBSan are mandatory matrix entries and are not allowed to continue on error.

The CI matrix may run configurations concurrently because each has an isolated build directory. Tests within each configuration remain serial to avoid fixed runtime-socket collisions.

## Workflow policy

Checkout, Qt installation, and diagnostics upload actions are pinned to immutable full commit SHAs with release comments. No action cache is enabled. The workflow does not use `pull_request_target`, write permissions, privileged jobs, or mutable action references. The policy job runs helper tests, shell syntax checks, preset listing, whitespace checks, and actionlint if it is already installed; when actionlint is unavailable, manual workflow validation is recorded in the qualification report.
