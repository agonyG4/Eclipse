# Settings Animations Rust/CXX-Qt Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore compatibility and reproducibility gaps in the approved Settings Animations/Typhon Rust/CXX-Qt migration without changing its architecture or adding routes.

**Architecture:** Keep `Animations.qml` and `SettingsController.animations` unchanged at the public behavior boundary. Use CXX-Qt's native `QList<QVariant>` binding for the two list properties, retain typed Rust state/protocol/discovery, and keep all Unix-socket I/O on the existing bounded worker. Make CI/CMake select and verify one pinned Qt installation for the Rust/CXX-Qt crate.

**Tech Stack:** Rust 2024, CXX-Qt 0.10.0, cxx-qt-lib 0.10.0, Qt 6.8+, CMake/Corrosion, C++20, Qt Test, GitHub Actions, Python unittest.

## Global Constraints

- Work directly on `main`; do not create a branch or worktree.
- Keep build and Cargo target output inside the repository/build tree.
- Do not use sub-agents or Python for implementation code.
- Preserve `cargo fmt --check`, locked clippy with `-D warnings`, and locked tests for both Rust backends.
- Keep `Animations.qml` behavior and the public `slots`, `presets`, and `SettingsController.animations` names unchanged.
- Keep Qt 6.8 as the minimum and CXX-Qt crates/CMake helper at `0.10.0`.
- Keep JSON at the Typhon wire boundary only; do not add JSON to the Rust/Qt boundary.
- Preserve one in-flight request, a bounded worker lane, secure endpoint discovery, monotonic deadlines, and bounded 1 MiB response handling.
- Do not migrate additional C++ services or add Settings pages.

---

### Task 1: CI and CMake dependency identity

**Files:**
- Modify `.github/workflows/ci.yml` to install and verify the pinned Qt 6.8.3 toolchain in the Rust job.
- Modify `tools/ci/run-rust-gate.sh` only if the explicit Qt environment needs a gate-side assertion.
- Modify `Settings/CMakeLists.txt` to use `find_package(CxxQt QUIET)`, the 0.10.0 FetchContent fallback, `LOCKED`, `CRATES astrea_settings_backend`, and qmake derived from the selected Qt6 installation.
- Modify `Settings/tests/static/SettingsStructureTest.cmake` to assert the Rust/CXX-Qt boundary and source-registered QML invariants.
- Modify `tools/ci/tests/` only if a new helper/policy assertion is needed.

- [ ] Write the CI/CMake/static assertions before changing the implementation and run the affected policy/static checks to capture the current failure.
- [ ] Install the repository's `QT_VERSION` in the Rust workflow job with the same immutable action pin and version used by the CMake/QML jobs; verify `qtpaths`, qmake, and the Qt version before the Rust gate.
- [ ] Resolve qmake from `Qt6::qmake` when available or from the selected `Qt6Core_DIR` prefix, verify qmake's `QT_VERSION` and `QT_INSTALL_PREFIX` against CMake's Qt selection, and fail configuration on ambiguity/mismatch rather than searching arbitrary PATH locations.
- [ ] Configure CXX-Qt through `find_package(CxxQt QUIET)` with the existing 0.10.0 FetchContent fallback and call `cxx_qt_import_crate` with `MANIFEST_PATH`, `LOCKED`, `CRATES astrea_settings_backend`, the selected qmake, and Qt Core/Network modules.
- [ ] Verify the workflow policy tests, CMake configure, and the static Settings structure test, then commit the dependency/build-system correction.

### Task 2: Qt property compatibility and QObject rejection state

**Files:**
- Modify `Settings/backend/src/animation/qobject.rs` to bind `slots` and `presets` as `QList<QVariant>` and preserve the existing getter names.
- Modify `Settings/tests/unit/SettingsAnimationControllerTest.cpp` for meta-object and rejection/lifecycle regressions.

- [ ] Add a Qt Test assertion that `slots` and `presets` report `QMetaType::QVariantList` through `QMetaProperty`, and add focused tests for mutation rejection preserving availability and refresh rejection clearing it.
- [ ] Run the focused Qt test to prove the property assertions fail against the current generic `QVariant` declarations and the refresh rejection test fails against the current Rust branch.
- [ ] Declare the CXX-Qt `QList_QVariant` binding using cxx-qt-lib's supported list header, return native `QList<QVariant>` values from the Rust getters, and keep nested entries as QVariant maps/lists.
- [ ] In the `ProtocolOutcome::ServerRejected` branch, clear availability only when the active operation was an explicit refresh; preserve availability for a rejected mutation and keep transport/protocol errors on their existing unavailable path.
- [ ] Add/strengthen real QObject tests for stale operation tokens, repeated speed debounce folding, and destruction with outstanding work without exposing test-only invokables to QML.
- [ ] Run the focused Qt test and Rust test suite, then commit the QObject compatibility/state-machine correction.

### Task 3: Rust client transport fixtures and timeout classification

**Files:**
- Modify `Settings/backend/src/typhon/client.rs` for deterministic test deadlines and timeout-form classification if required.
- Add the client/worker Unix-domain-socket fixture tests in `Settings/backend/src/typhon/client.rs`.

- [ ] Add tests first for off-caller-thread completion, timeout, connection/disconnection failure, bounded submission rejection, worker reuse, safe drop during outstanding I/O, response-size enforcement, and a short-deadline timeout-form error.
- [ ] Run the focused client tests and confirm they fail for the missing coverage/timeout classification.
- [ ] Treat `TimedOut` and timeout-form `WouldBlock` returned by the deadline-bounded socket operations as `ClientError::Timeout` without relying on a microscopic post-syscall clock comparison; leave other error kinds as transport failures.
- [ ] Keep the worker's one-slot submission lane and detached shutdown behavior safe for QObject destruction; do not add per-request threads or move protocol parsing/discovery out of their modules.
- [ ] Run focused client/protocol/discovery tests, format, clippy, and the full Settings Rust test suite, then commit the transport correction.

### Task 4: Documentation and architecture-policy reconciliation

**Files:**
- Modify `Settings/README.md`.
- Modify `Settings/docs/ARCHITECTURE.md`.
- Modify `Settings/docs/COMPONENT_CATALOG.md`.
- Modify `Settings/docs/STRUCTURE.md`, `Settings/docs/TESTING.md`, or `Settings/docs/MIGRATION_NOTES.md` only where the same factual drift is present.

- [ ] Derive the current QML registration count from `Settings/qml/CMakeLists.txt` and compare it with the catalogue; identify stale count and route statements before editing.
- [ ] Update factual count text to 44, add Appearance and Animations to the current native route description/catalogue, and state the approved QML/CXX-Qt/Rust/C++ ownership split without implying that all system-facing state remains C++.
- [ ] Keep all Markdown English and avoid changing migration scope or promising deferred pages/services.
- [ ] Run Markdown/source-policy checks and commit the documentation correction.

### Task 5: Full verification and review

- [ ] Run `cargo fmt --manifest-path Settings/backend/Cargo.toml -- --check`.
- [ ] Run locked Settings clippy and tests.
- [ ] Run the equivalent format/clippy/test commands for `Spotlight/backend`.
- [ ] Run `tools/ci/run-rust-gate.sh`, `tools/ci/run-qml-gate.sh`, CI helper/policy tests, and shell syntax checks.
- [ ] Configure/build/test through the supported CMake path, including Settings unit, integration, static, QML, ASan, and UBSan coverage available in this checkout; record every unavailable command with its exact reason.
- [ ] Review generated QObject property types, qmake/Qt identity, lockfile enforcement, worker/QObject destruction, refresh-vs-mutation rejection, stale tokens, debounce boundedness, timeout classification, QML registration/routes/count, and superseded C++ implementation absence.
- [ ] Run `git diff --check`, inspect the complete diff, and commit the verified correction pass.
