# Eclipse customization implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Execute task-by-task with Luna implementers.

**Goal:** Implement the approved theme, modularity, SDK and build-location improvements.
**Architecture:** Shared validated theme policy with native ownership and versioned optional presentation replacement. SDK-only configuration remains independent of internal service backends.
**Tech Stack:** C++20, Qt 6.8+, QML, CMake, existing Rust bridges.
**Spec:** docs/superpowers/specs/2026-10-30-eclipse-customization-design.md

## Global Constraints

- Work on main, no branches/worktrees. Preserve pre-existing user changes and commit only owned changes.
- All builds, tests, caches and temporary outputs are under /mnt/Aether/Desktop/GitHub. Default CMake output: /mnt/Aether/Desktop/GitHub/Eclipse-build. Cargo output: /mnt/Aether/Desktop/GitHub/Eclipse-target. Verify paths, set TMPDIR and CARGO_TARGET_DIR explicitly.
- Qt minimum remains 6.8; no new third-party runtime dependencies. Default visuals/behavior stay unchanged without customization.
- SDK exposes no privileged Typhon handles or internal system controllers. QML owns presentation; native code owns IPC, files and compositor surfaces.
- Use meaningful behavioral tests for new behavior. Do not spawn child agents from implementer/reviewer agents.

## Review Focus

- Partial/invalid theme manifests must fall back per token, including explicit zero durations and radii.
- Theme changes after atomic file replacement must update bindings; invalid replacement must safely restore defaults.
- Executable QML is opt-in and package-local, with fallback for invalid components and no new native capability exports.
- Existing runtime ownership and surface teardown ordering must remain valid after extraction.
- Independent SDK consumers must work after relocation, and SDK-only configure must not probe internal backend dependencies.

### Task 1: Independent SDK configuration and Aether presets

**Files:** Own CMakeLists.txt, CMakePresets.json, shared/CMakeLists.txt, a focused shared/cmake/Core.cmake extraction if useful, shared/docs/SDK_ARCHITECTURE_V1.md, docs/CI.md, and focused new SDK configure tests. Do not edit dirty user files.
**Interfaces:** Produce ASTREA_BUILD_SDK_ONLY option (default OFF). With ON configure only shared Core/SDK/QML and focused SDK/icon/theme tests, before all LayerShell/Wayland/Rust/PipeWire discovery. Preserve normal build targets and SDK exports.
- [ ] Read source and existing SDK consumer test; baseline changed files are clean.
- [ ] Add SDK-only configuration path, sharing Core definitions rather than duplicating them; find only Qt Core/Gui/Qml/Quick/ShaderTools and Test if needed.
- [ ] Keep debug binaryDir /mnt/Aether/Desktop/GitHub/Eclipse-build; use stable release/clang/asan/ubsan/no-typhon/no-layer-shell suffixes only for incompatible configurations. Set CARGO_TARGET_DIR environment to /mnt/Aether/Desktop/GitHub/Eclipse-target and TMPDIR to a stable Aether tmp directory.
- [ ] Configure with SDK-only ON and disabled discovery of CxxQt/PkgConfig/LayerShellQt to demonstrate no unwanted backend probing. Print verified paths before configuring/building.
- [ ] Build SDK/icon targets and run astrea-sdk-install-consumer-test plus focused existing icon tests. Keep tests/temporary output under Aether.
- [ ] Update SDK/CI docs and commit owned changes. Report commands, outputs and commit SHA.

### Task 2: Validated shared theme tokens and live package reload

**Files:** Own shared/theme/ThemePackCatalog.hpp, shared/theme/ThemeController.cpp/.hpp, shared/qml/ShellMaterialTheme.qml, new shared/qml/ThemeTokens.qml (QtObject, not process singleton), shared CMake core registration/tests, shared tests for tokens/reload, docs/THEME_PACKAGES.md, themes/examples/minimal/theme.json.
**Interfaces:** ThemeTokens exposes property var state (default existing ThemeController context if present), readonly property var overrides, and function value(name, fallback). ShellMaterialTheme uses ThemeTokens for its existing properties and compatible new semantic properties. Keep existing property names and default values. Existing style.colors and top-level fontFamily/animationFast remain compatible; add nested metrics (0..256), typography font sizes (1..96), durations (integer 0..2000) with explicitly allowlisted semantic keys consumed by Task 3. Coordinate exact names with parent before editing.
- [ ] Add behavioral tests for valid overrides, partial fallback, invalid/unknown keys, zero durations and radii, and QML live binding updates.
- [ ] Implement validation and shared resolution; preserve native default policy. Expand colors to cover existing application and shell semantic colors.
- [ ] Watch the selected manifest and its directory in addition to preference files; preserve watching after atomic replacement and deletion/recreation. Cover with filesystem watcher test.
- [ ] Register public ThemeTokens with Astrea.Shared and test through SDK-only build. Ensure fallback without ThemeController.
- [ ] Document keys and working example, verify focused tests, commit owned changes.

### Task 3: Migrate presentation consumers to shared theme policy

**Files:** Own Bar/qml/components/ShellBarTheme.qml, Dock/qml/components/DockPanel.qml and DockTooltipSurface.qml, Shell/qml/contextmenu/ShellMenuTheme.qml, AltTab/qml/components/*.qml, Spotlight/qml/components/*.qml, Settings/qml/theme/{Apps,Shell,Tokens}.qml, Settings/qml/components/Theme.qml, relevant clean smoke tests or new dedicated tests. Avoid dirty Bar volume/test files and all other dirty Settings files.
**Interfaces:** Consume Task 2 ThemeTokens and ShellMaterialTheme. Keep local aliases for compatibility; defaults must remain identical, including different defaults for Settings vs Shell and Spotlight/AltTab. Explicit component geometry configuration (DockController) stays authoritative unless a named theme override is present.
- [ ] Read existing consumers and meaningful smoke tests.
- [ ] Replace fixed visual colors/typography/spacing/radii/durations with common override policy while retaining exact fallback values and existing QML contracts.
- [ ] Make derived colors such as accentForeground respond to overridden accent.
- [ ] Add runtime tests that select one manifest and verify actual Bar/Dock/menu/Spotlight/AltTab/Settings properties change, partial manifests preserve defaults, zero values survive, and switching restores defaults.
- [ ] Run focused QML tests and Bar/Dock/Spotlight architecture guards; commit only owned clean files.

### Task 4: Versioned optional QML presentation components

**Files:** Own shared/theme package component resolver, shared/qml/ThemeComponentLoader.qml, shared/theme/ThemeController accessors, shared CMake registration and behavior tests, narrowly scoped QML integration wrappers in Shell components, example package and docs/THEME_PACKAGES.md. Exact integration points must be agreed with parent after Task 3.
**Interfaces:** Optional manifest components object with apiVersion 1 and named relative QML files. Execution requires explicit ASTREA_ALLOW_THEME_QML=1 startup opt-in; selecting a normal visual package alone never executes code. Resolve canonical package-local files only. Loader exposes property url source, property Component fallbackComponent, property var initialProperties, readonly property Item item; invalid URL/load/root/contract falls back. Contracts receive native models/controllers explicitly; windows and compositor adapters remain built-in. At least Spotlight and AltTab support actual replaceable panels with documented focus/backdrop contract; add further surfaces only with defined contracts.
- [ ] Add resolver and loader tests for opt-in, supported version, traversal/symlink rejection, malformed QML, wrong root/contract, missing file and fallback.
- [ ] Implement public reusable loader and package resolver with no generated protocol types in SDK.
- [ ] Integrate replaceable Spotlight and AltTab panels preserving focus, hit testing, backdrop and native lifecycle; provide complete working sample QML and document supported surfaces and limits precisely.
- [ ] Verify valid replacement and fallback with offscreen QML tests and existing smoke tests; commit owned changes.

### Task 5: Focused ownership and large-file cleanup

**Files:** Own AltTab/core/AltTabShortcutRouter.*, AltTab/platform/typhon shortcut adapter and CMake/tests/callers, Settings/qml/pages/system/Display.qml and new display calculation helper/component, Shell/app/AstreaShellApplication.* and screenshot surface coordinator if behavior-preserving. Do not edit already dirty SettingsController or page files other than clean Display.qml.
**Interfaces:** AltTab core shortcut policy consumes a platform-neutral phase enum; Typhon phase translation belongs to platform/typhon. Display helper holds pure resolution grouping/formatting/geometry functions with explicit inputs. Screenshot coordinator handles the three screenshot transient slots without changing native ownership/teardown order.
- [ ] Move Typhon-specific mapping to platform layer and adapt callers/build targets, preserving shortcut behavior; run shortcut/router tests and architecture guards.
- [ ] Extract cohesive pure display calculations into a QML JS helper registered with resources; add focused behavioral checks and run Display smoke/integration tests.
- [ ] Extract screenshot composition only if existing lifecycle tests can be retained and exercised; otherwise document concrete remaining pressure rather than risky unrelated rewrite.
- [ ] Run affected tests, inspect diff against original user changes, and commit owned changes.
