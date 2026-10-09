# Application Launch Wayland Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refresh systemd's activation environment before Eclipse sends an application launch to its systemd/GIO helper, so it uses the active Wayland socket.

**Spec:** `docs/superpowers/specs/2026-10-09-application-launch-wayland-environment-design.md`

**Architecture:** Add synchronization at the shared `ApplicationLauncher` dispatch boundary, which is used by Spotlight and Dock. For the normal `astrea-launch` path, run `dbus-update-activation-environment --systemd` with the current `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` before starting the helper; do not include `DISPLAY`. Preserve the Typhon shell-control bridge path because it does not use the systemd/GIO helper.

**Tech Stack:** C++17, Qt 6 `QProcess`/`QProcessEnvironment`, Qt Test, CMake.

## Global Constraints

- Keep all compilation, tests, caches, and temporary build output under `/mnt/Aether/Desktop/GitHub`.
- Reuse `/mnt/Aether/Desktop/GitHub/Eclipse-build`; verify the effective directory before every build command.
- Keep the Eclipse source checkout at `/home/agony/GitHub/Eclipse`, on `main`; do not create a branch.
- Add focused regression coverage for current Wayland values, exclusion of `DISPLAY`, and synchronization failure preventing helper dispatch.

---

### Task 1: Synchronize the launch environment

**Files:**
- Modify: `shared/launch/ApplicationLauncher.cpp`
- Modify: `shared/launch/ApplicationLauncher.hpp`
- Test: `Shell/tests/ShellRuntimeTest.cpp`
- Build definition: existing `shell-runtime-test` target in `Shell/CMakeLists.txt`; no target change is expected

**Interfaces:**
- Consumes: `ApplicationLauncher::launchDesktop()` and `runSupervised()`; `QProcessEnvironment::systemEnvironment()`.
- Produces: an internal activation-environment sync helper that returns success and an error message; `runSupervised()` calls it only for the normal systemd/GIO launcher path before it starts the launch helper.

- [x] **Step 1: Add a passing-path regression test first.** In `ShellRuntimeTest`, add `launcherRefreshesSystemdWaylandEnvironmentBeforeDispatch()`. Use `QTemporaryDir` and `writeExecutable()` to put two scripts in the directory:

  ```sh
  # Fake dbus-update-activation-environment
  printf '%s\n' "$@" > "$ASTREA_TEST_ARGS"
  printf 'sync\n' >> "$ASTREA_TEST_ORDER"
  # Fake astrea-launch helper
  printf 'launch\n' >> "$ASTREA_TEST_ORDER"
  ```

  Set `PATH` to the temporary directory, initially set `WAYLAND_DISPLAY=wayland-before-launch` and `XDG_RUNTIME_DIR=/run/user/before-launch`, set `DISPLAY=:42`, clear the Typhon bridge selector variables, and guard every modified variable with `EnvironmentGuard`. Construct `ApplicationLauncher`, then change Wayland/runtime values to `wayland-current` and `/run/user/1000` before launching a desktop file. Assert success, arguments exactly `--systemd`, `WAYLAND_DISPLAY=wayland-current`, `XDG_RUNTIME_DIR=/run/user/1000`, and order exactly `sync` then `launch`. This proves the live values are read at dispatch time and `DISPLAY` is not imported.

- [x] **Step 2: Add a failure-path regression test.** Add `launcherRejectsDispatchWhenSystemdEnvironmentSyncFails()`. Make the fake updater print `activation sync failed` to stderr and exit 7; make the launch helper create a marker file. Assert one `launchFailed`, one unsuccessful `launchCompleted`, no `launchSucceeded`, and that the marker file was not created.

- [x] **Step 3: Build and run the new tests before production changes.** Confirm the cache belongs to the approved tree with `realpath /mnt/Aether/Desktop/GitHub/Eclipse-build` and inspect `CMAKE_HOME_DIRECTORY` in its `CMakeCache.txt`. Create `/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp` and run `TMPDIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target shell-runtime-test` followed by `TMPDIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^shell-runtime-test$' --output-on-failure`. The test executable must build, then fail because activation synchronization is not implemented.

- [x] **Step 4: Implement the smallest production change.** Build the update arguments from `QProcessEnvironment::systemEnvironment()`:

  ```cpp
  QStringList args{QStringLiteral("--systemd")};
  for (const QString &name : {QStringLiteral("WAYLAND_DISPLAY"),
                               QStringLiteral("XDG_RUNTIME_DIR")}) {
      if (environment.contains(name))
          args.append(name + QLatin1Char('=') + environment.value(name));
  }
  ```

  Start `dbus-update-activation-environment` with `QProcess`, wait for completion, and return a useful error on start failure, timeout, or non-zero exit. Call the helper from `runSupervised()` before creating/starting the launch-helper process for the fallback systemd/GIO path. On synchronization failure, emit `launchFailed` and `launchCompleted(..., false)` and return without emitting `launchAccepted` or starting the helper. Leave the Typhon control bridge path unchanged.

- [x] **Step 5: Rebuild and run focused verification.** Recheck that `realpath /mnt/Aether/Desktop/GitHub/Eclipse-build` is exactly `/mnt/Aether/Desktop/GitHub/Eclipse-build`, then rebuild and run CTest with `TMPDIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp` as in Step 3. Both new tests and the existing shell runtime tests must pass.

- [x] **Step 6: Review and commit.** Run `git diff --check`, inspect the full diff and status, stage the plan, test, and launcher files, and commit as `fix: refresh Wayland environment before app launches`.
