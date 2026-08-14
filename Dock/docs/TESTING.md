# Dock Testing

Configure, build, and run all deterministic tests from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

The Dock-specific targets can be built and run independently:

```bash
cmake --build build --target astrea-dock dock-app-model-test dock-controller-test \
  dock-config-watcher-test dock-ipc-test dock-runtime-paths-test dock-command-line-test
cmake --build build --target astrea-dock dock-app-model-test dock-controller-test \
  dock-typhon-runtime-integration-test dock-application-state-projector-test \
  typhon-app-matcher-test
ctest --test-dir build -R 'dock-|desktop-entry-catalog-test|typhon-app-matcher-test' \
  --output-on-failure
```

Sanitizer validation uses the project option:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_ASAN=ON
cmake --build build-asan -j"$(nproc)"
ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DASTREA_ENABLE_UBSAN=ON
cmake --build build-ubsan -j"$(nproc)"
ctest --test-dir build-ubsan --output-on-failure
```

Tests are deterministic and do not launch real applications in controller
tests. The IPC tests use local temporary socket names. The runtime integration
test drives a fake Typhon protocol adapter and proves dynamic rows, exact
activation, close removal, and authority loss. Explorer's source contract is
covered by `python3 -m unittest src/System/tests/test_bin_launchers.py` in the
current AstreaOS source tree.

Run the same focused and complete CTest commands in both `build/debug` and
`build/release`. Normal shell validation must use the LayerShellQt-enabled
configuration; a no-LayerShell build is not production validation. QML lint,
`git diff --check`, and a live Typhon qualification are separate gates. Live
results must be reported per case and must not be inferred from deterministic
tests.
