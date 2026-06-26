# Testing

## Automated tests

```bash
cmake -S AltTab -B AltTab/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build AltTab/build
ctest --test-dir AltTab/build --output-on-failure
```

### Test suites

- `alttab-controller-test`: State machine, selection, pending commands, filtering
- `alttab-ipc-test`: IPC command parsing, status responses, malformed input, size limits
- `alttab-identity-test`: Alias resolution, Steam AppID, desktop entries, deep pending
- `alttab-source-test`: Address normalization, JSON parsing, display names, sorting
- `alttab-model-test`: All model roles, insert/remove/update, stable keys, selection

### Static checks

```bash
qmllint AltTab/qml/Main.qml AltTab/qml/components/*.qml
```

## Manual runtime validation

See `AltTab-Cpp-Migration-Material/docs/TEST_PLAN.md` for the full manual matrix.

Key manual checks:
1. Daemon starts hidden (`--status` returns running, hidden)
2. Alt+Tab opens instantly
3. Repeated Alt+Tab cycles
4. Alt+Shift+Tab cycles backward
5. Alt release commits
6. Escape cancels
7. Enter/Return commits
8. Background click cancels
9. Hover previews
10. Item click commits and focuses
11. Window closes while open handled safely
12. Zero and one-window cases deterministic
