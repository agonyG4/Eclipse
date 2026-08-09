# M7-D Unified Eclipse Shell Qualification

Date: 2026-08-09

## Result

M7-D production integration is complete. Native compositor qualification remains deferred by the approved M7-D scope.

`astrea-shell` is now the sole resident shell process. It owns one `QGuiApplication`, one `QQmlApplicationEngine`, one `ShellRuntime`, one shared catalog/identity/launcher graph, one Typhon session/display/authentication graph when compiled, one shortcut client, and one `astrea-shell-v1` IPC endpoint. Dock, Alt+Tab, and Spotlight remain separate controllers and load their existing QML surfaces into that engine.

The legacy `astrea-dock`, `astrea-alt-tab`, and `astrea-spotlight` names are non-resident IPC clients. Their `--daemon` modes fail deterministically with exit code 2. Only `astrea-shell.service` is installed; the three old resident service files were removed.

## Validation evidence

- Debug/Typhon build: `cmake --build build/debug -j2`
- Debug/Typhon tests: `ctest --test-dir build/debug --output-on-failure` — 45/45 passed.
- No-Typhon build: configured with `-DASTREA_ENABLE_TYPHON_BACKEND=OFF`, then `cmake --build build/no-typhon -j2`.
- No-Typhon tests: `ctest --test-dir build/no-typhon --output-on-failure` — 43 passed, 2 expected Typhon integration tests skipped.
- Focused runtime tests: `shell-runtime-test` and `shell-ipc-test` pass in both matrices.
- Compatibility guards: three CTest cases verify that legacy `--daemon` invocations do not become resident services.
- Live offscreen startup loaded all three QML roots and accepted status/actions through the single endpoint; all legacy status commands returned the same unified status JSON.
- Install topology contains `astrea-shell.service` as the only systemd user service and installs `astrea-shell` alongside non-resident compatibility client binaries.

## Deferred boundary

This qualification does not claim native compositor/display qualification. Native Typhon action semantics remain covered by the earlier M7-B/M7-C evidence and are not reopened by the M7-D process consolidation.
