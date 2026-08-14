# Layer Shell Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Require and correctly initialize Qt 6 LayerShellQt for Eclipse's unified shell without changing surface placement policy.

**Architecture:** Root CMake exposes an ON-by-default capability and delegates discovery/diagnostics to a small reusable module. `astrea-shared-layer-shell` owns the dependency and preparation seam; `AstreaShellApplication` enforces the production runtime contract and reports truthful per-surface status.

**Tech Stack:** CMake 3.24+, Qt 6/QML/QtTest, C++20, LayerShellQt, Python `unittest`, GitHub Actions.

## Global Constraints

- `ASTREA_ENABLE_LAYER_SHELL` defaults to `ON`; `OFF` is explicit non-production/test mode.
- `astrea-shared-core` and Settings must remain LayerShellQt-free.
- Standard CMake discovery must use `CMAKE_PREFIX_PATH`, `LayerShellQt_DIR`, or `LayerShellQt_ROOT`; no developer path is tracked.
- Unified shell startup must fail on missing LayerShellQt, non-Wayland Qt platforms, QML load failure, or Layer Shell setup failure.
- Dock/AltTab/Spotlight visual and Layer Shell policy must remain unchanged.
- Typhon must not be modified.

### Task 1: Add the failing contract tests

**Files:**
- Create: `tools/ci/tests/test_layer_shell_contract.py`
- Create: `shared/tests/LayerShellHelperTest.cpp`
- Modify: `shared/CMakeLists.txt`

**Interfaces:**
- The Python test invokes the CMake contract module through a minimal generated fixture and verifies success/failure diagnostics.
- The QtTest consumes `AstreaLayerShellHelper::compiled()` and the new `prepare(QString *)` seam.

- [ ] **Step 1: Write the Python fixture assertions and QtTest expectations.**
- [ ] **Step 2: Register the QtTest in `shared/CMakeLists.txt` without adding production implementation.**
- [ ] **Step 3: Run the Python test and the focused build/test; verify they fail because the contract module and `prepare()` do not exist.**

### Task 2: Implement explicit CMake discovery

**Files:**
- Create: `cmake/AstreaLayerShell.cmake`
- Modify: `CMakeLists.txt`
- Modify: `shared/CMakeLists.txt`
- Modify: `CMakePresets.json`

**Interfaces:**
- `astrea_configure_layer_shell()` validates `LayerShellQt_FOUND` and `LayerShellQt::Interface` when enabled.
- The shared target links the imported interface and defines `ASTREA_HAVE_LAYER_SHELL_QT=1` only when enabled; OFF defines `0`.

- [ ] **Step 1: Add `ASTREA_ENABLE_LAYER_SHELL` ON by default and invoke the CMake module before component subdirectories.**
- [ ] **Step 2: Add actionable standard-package discovery and target validation diagnostics.**
- [ ] **Step 3: Remove the old QUIET automatic fallback from `shared/CMakeLists.txt`.**
- [ ] **Step 4: Add the explicit `no-layer-shell` configure/build/test preset while keeping canonical presets production-enabled.**
- [ ] **Step 5: Run the CMake contract test red/green cycle and verify ON/OFF cache values.**

### Task 3: Centralize LayerShellQt preparation and make shell startup strict

**Files:**
- Modify: `shared/platform/wayland/LayerShellHelper.hpp`
- Modify: `shared/platform/wayland/LayerShellHelper.cpp`
- Modify: `Shell/app/main.cpp`
- Modify: `Shell/app/AstreaShellApplication.hpp`
- Modify: `Shell/app/AstreaShellApplication.cpp`
- Modify: `Dock/platform/wayland/DockLayerShellSurface.cpp`

**Interfaces:**
- `AstreaLayerShellHelper::prepare(QString *)` calls `LayerShellQt::Shell::useLayerShell()` in ON builds and returns a clear error in OFF builds.
- `AstreaShellApplication` tracks successful Dock, AltTab, and Spotlight configuration for status output.

- [ ] **Step 1: Implement `prepare()` and call it before `QGuiApplication` construction.**
- [ ] **Step 2: Require the Wayland Qt platform before creating production shell surfaces.**
- [ ] **Step 3: Make Dock, AltTab, and Spotlight QML load/configuration failures fatal.**
- [ ] **Step 4: Remove all ordinary-window visibility/map behavior from LayerShell stubs and make mapping failures terminate the shell instead.**
- [ ] **Step 5: Add truthful `layerShell` status fields and preserve all existing surface configuration values.**
- [ ] **Step 6: Run the focused QtTest and source-policy assertions.**

### Task 4: Add required CI production coverage

**Files:**
- Modify: `.github/workflows/ci.yml`
- Modify: `tools/ci/run-cmake-gate.sh`
- Modify: `tools/ci/run-all-local.sh`

- [ ] **Step 1: Add a reproducible Qt 6 LayerShellQt installation step without selecting a Qt 5 package.**
- [ ] **Step 2: Pass the discovered prefix through standard CMake configuration for the production matrix.**
- [ ] **Step 3: Add an explicit no-LayerShell validation path and update helper usage/usage text.**
- [ ] **Step 4: Run workflow policy and shell syntax tests.**

### Task 5: Update affected documentation

**Files:**
- Modify: `README.md`
- Modify: `shared/README.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `AltTab/docs/ARCHITECTURE.md`
- Modify: `docs/CI.md`
- Modify: relevant Shell documentation if present
- Modify: `CMakePresets.json` comments/documentation if needed

- [ ] **Step 1: Document the required production dependency, standard search paths, explicit OFF mode, and no runtime fallback.**
- [ ] **Step 2: Document the canonical production presets and explicit no-LayerShell preset.**
- [ ] **Step 3: Run Markdown/source searches for contradictory optional/fallback claims.**

### Task 6: Full verification and commit

- [ ] **Step 1: Configure clean Debug and Release builds with the discovered local LayerShellQt prefix.**
- [ ] **Step 2: Build and run CTest, QML, Rust, policy, sanitizer, and archive gates available in the workspace.**
- [ ] **Step 3: Verify cache, linked library, compile definition, and absence of hardcoded local paths/fallback strings.**
- [ ] **Step 4: Inspect the complete diff and run `git diff --check`.**
- [ ] **Step 5: Run live Hyprland qualification only if the current session and shell replacement are safe; report it separately.**
- [ ] **Step 6: Commit all Eclipse changes as `fix(shell): require functional LayerShellQt integration`.**
