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
ctest --test-dir build -R 'dock-|desktop-entry-catalog-test' --output-on-failure
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
tests. The IPC tests use local temporary socket names. A real Typhon session
smoke test is separate from CTest and must not be claimed unless a session was
actually run.
