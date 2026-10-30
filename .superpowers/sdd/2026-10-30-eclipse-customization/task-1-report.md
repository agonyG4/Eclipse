# Task 1 report: independent SDK configuration and Aether presets

## Result

Implemented and committed on `main` as `ae16f48` (`Add standalone SDK-only CMake configuration`).

`ASTREA_BUILD_SDK_ONLY` defaults to `OFF`. When enabled, the root configures Qt Core, Gui, Qml, Quick, and ShaderTools, then enters only Shared Core and the SDK. It returns before Wayland/PkgConfig, LayerShellQt, CXX-Qt, PipeWire, and Cargo discovery. The normal build reuses the same `shared/cmake/Core.cmake` declarations, preserving SDK exports and existing target definitions.

The SDK-only tree registers the icon-provider, app-icon QML, install-consumer, and backend-discovery configure tests. The configure test disables PkgConfig, LayerShellQt, and CXX-Qt discovery and disconnects FetchContent. Presets use stable Aether build directories; the debug preset is `/mnt/Aether/Desktop/GitHub/Eclipse-build`. Local gate scripts share `tools/ci/set-build-output-env.sh`, which places Cargo targets/home, temp files, XDG cache, and Python bytecode policy under the configured output root. Hosted CI explicitly redirects those paths and the CMake build root to `runner.temp`.

## Build and test evidence

- Configured with:
  `cmake --preset debug -B /mnt/Aether/Desktop/GitHub/Eclipse-build -DASTREA_BUILD_SDK_ONLY=ON -DASTREA_ENABLE_LAYER_SHELL=OFF -DASTREA_ENABLE_TYPHON_BACKEND=OFF -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_LayerShellQt=TRUE -DCMAKE_DISABLE_FIND_PACKAGE_CxxQt=TRUE -DFETCHCONTENT_FULLY_DISCONNECTED=TRUE`
- Before configure/build/test, verified the effective binary directory from `CMakeCache.txt` as `/mnt/Aether/Desktop/GitHub/Eclipse-build`. Cargo target, Cargo home, temp, and XDG cache paths were also resolved under `/mnt/Aether/Desktop/GitHub`.
- Configure succeeded with CMake 4.4.5 and Qt 6.12.0 while the backend packages were unavailable by design. The first configure printed unused-command-line warnings for backend-disable variables, evidence that the SDK-only path did not consume those backend options.
- Built `astrea-sdk-core`, `astrea-icon-provider-test`, and `astrea-app-icon-qml-test` successfully. Build output stayed in the verified Aether tree. The existing `ScreenshotConfiguration.cpp` emitted an unused-parameter warning for `changedField`.
- `astrea-icon-provider-test`: passed.
- `astrea-sdk-install-consumer-test`: passed, including independent consumer configure/build/run after installing and relocating the SDK.
- `astrea-sdk-only-configure-test`: passed with backend package discovery disabled and FetchContent disconnected.
- The registered `astrea-app-icon-qml-test` under its existing `QT_QPA_PLATFORM=offscreen` CTest environment failed two shader pixel assertions. Qt logging identified `Loading backend software`; GDB showed the shader samples were transparent black while the direct source samples were red and white. The full test binary passed under Xvfb/xcb with software GL: 17 passed, 0 failed, 0 skipped. No rendering code or assertions were changed.
- `tools/ci/tests/test_cmake_preset_paths.py`: 5 passed.
- `tools/ci/tests/test_rust_workflow_dependencies.py`: 1 passed.
- `tools/ci/check-workflow-policy.py .github/workflows/ci.yml`: passed.
- `bash -n` on the local gate scripts and output environment helper: passed. `cmake --list-presets` parsed and listed all seven presets. `git diff --check`: passed.
- Full Rust, QML, and CMake CI gates were not run in this task.

## Files changed

- `CMakeLists.txt`, `CMakePresets.json`, `shared/CMakeLists.txt`, `shared/cmake/Core.cmake`
- `shared/tests/RunSDKOnlyConfigureTest.cmake`
- `shared/docs/SDK_ARCHITECTURE_V1.md`, `docs/CI.md`
- `tools/ci/set-build-output-env.sh`, `tools/ci/preset-binary-dir.py`
- `tools/ci/run-all-local.sh`, `tools/ci/run-rust-gate.sh`, `tools/ci/run-qml-gate.sh`, `tools/ci/run-cmake-gate.sh`
- `tools/ci/tests/test_cmake_preset_paths.py`
- `.github/workflows/ci.yml`

## Self-review and remaining limits

The Core target, QML module, SDK facade, install/export declarations, and shader declaration were moved without changing their definitions. Both SDK-only and normal configuration use that shared file. The normal preset explicitly turns SDK-only mode off so a prior SDK-only cache does not suppress full targets. CMake configuration with only Qt dependencies was verified; a full normal Eclipse build was not run. The offscreen shader test limitation is documented above; running it under Xvfb/xcb passed without changing the product or test.
