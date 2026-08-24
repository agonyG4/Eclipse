# Astrea Paper Wallpaper API v1.1.1 — Completion Report

Date: 2026-08-19  
Scope: Eclipse Paper renderer integration, Settings fit authority, factory asset provenance, timeout semantics, source watching, Typhon client behavior, and native qualification.

## Outcome

The v1.1.1 integration closure is implemented in the existing dirty Eclipse and Typhon worktrees. Paper remains authoritative; the existing transactional service, configured/default/effective separation, latest-request-wins behavior, asynchronous clients, secure IPC, and two-slot renderer were preserved.

No files were reset, restored, discarded, staged, or committed. Unrelated dirty work was preserved.

## 1. Baseline Eclipse/Typhon status

At final inspection Eclipse had 290 dirty tracked/untracked status entries and Typhon had 113. The worktrees contain substantial unrelated M8/B and v1.1 changes. The v1.1.1 Paper/Settings additions remain untracked, as requested; no unrelated files were staged.

## 2–5. Same-path renderer reload

**CONFIRMED root cause:** Paper advanced `wallpaperGeneration`, but `WallpaperSurface.qml` assigned the unchanged physical URL to a cached `Image` slot. Qt Quick could therefore retain the old decoded image even though the file contents had changed.

**DESIGN DECISION:** QML now derives a renderer-only source such as:

```text
file:///path/wallpaper.png?astreaGeneration=21
```

The authoritative descriptor remains unchanged and `Image.cache: true` remains enabled. Each front/back slot records its requested source and generation. A Ready or Error callback only promotes/retries when it still matches the newest source and generation. A newer request waits behind an in-flight inactive-slot load, and the visible slot is not cleared during replacement.

Diagnostics expose load-start/Ready/Error counts, requested front/back generations, promotion count, and visible generation without per-frame logging.

The offscreen QML regression writes distinct revisions to one exact path, advances generations 1→2→3→4, checks that the load count increases, checks the active renderer URL contains `astreaGeneration=2`, and confirms visible generation 4. Result: `paper-surface-manager-test` passed 5/5.

## 6–7. Settings fit authority

**CONFIRMED root cause:** `SettingsWallpaperController` previously projected source/state/fallback/generation but omitted `configured.fit` and `effective.fit`; the QML selector defaulted to Cover and could overwrite a Paper-owned fit.

The controller now exposes `configuredFit` and `effectiveFit`, validates all five current values, rejects unsupported mutation input before transport, and synchronizes the QML selector from configured fit when present or effective fit otherwise. Source and fit controls remain disabled while an asynchronous request is pending.

The Settings tests cover fallback projection (`configured=contain`, `effective=cover`), all five fit round trips (`cover`, `contain`, `stretch`, `center`, `tile`), unsupported-fit rejection, and delayed completion. Result: `settings-wallpaper-controller-test` passed 11/11.

This prevents `astreactl --fit contain` from being silently changed to Cover by opening or applying Settings; Paper remains the only persistent fit authority.

## 8–9. Shared factory asset and cross-process preview

The normal factory source is now the physical asset:

```text
${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers/default.jpg
```

The resolver searches explicit overrides, `ASTREA_WALLPAPER_DEFAULT`, installed XDG data, the Eclipse source-tree development asset, legacy `ASTREA_ROOT` locations, and then the independent embedded emergency asset. The normal factory descriptor retains logical ID `astrea://wallpaper/default`; its resolved source is physical and readable, never shell-local `qrc:` data.

The resolver regression passed 8/8 and verified an absolute existing `default.jpg` path. A staging install placed the asset at `share/AstreaOS/wallpapers/default.jpg`; the overall install command also reported the pre-existing build-tree blocker that `Spotlight/astrea-spotlight` was absent.

The Settings cross-process preview fixture returned the physical source and successfully opened it with `QImageReader`, without a `qrc:` URL. Result: passed as part of the 11/11 controller suite. Emergency fallback remains separately tested and independent.

## 10–11. Transport and operation deadlines

`shared/platform/paper/PaperProtocol.hpp` documents one contract:

| Boundary | Deadline |
| --- | ---: |
| Transport connect/write | 1000 ms |
| Paper operation validation/persistence | 5000 ms |
| Client completion margin | 1000 ms |
| Settings/CLI operation wait | 6000 ms |

Paper owns the 5000 ms operation timer and emits terminal `timed-out` results, invalidating late worker tokens. The server waits through the 6000 ms client deadline. Settings starts with the 1000 ms transport timer and switches to 6000 ms after the command is written. `astreactl` keeps explicit `--timeout` overrides, defaults wallpaper commands to 6000 ms, uses a 1000 ms write timeout, and uses the operation timeout for the final response.

During verification, a real Qt ordering defect was fixed: `QLocalSocket::connectToServer()` can emit `connected` synchronously, so the transport timer must start before the connect call or it can overwrite the newly-started operation timer.

## 12. Delayed-operation results

The Settings fake server delayed its final response by 1500 ms. It no longer failed at the old 1000 ms deadline and completed successfully. The Rust fake Paper listener used the same 1500 ms delay and completed successfully with the default 6000 ms wallpaper timeout.

Focused Typhon wallpaper tests passed 5/5, binary parser tests passed 2/2, and `cargo check --bin astreactl` passed.

A deterministic delayed validation-worker test that waits for the actual Paper 5000 ms service timer remains unproven: the existing `WallpaperResolver`/worker is concrete and normal image validation completes well below that deadline. The service-owned timeout and late-token invalidation are implemented, while this specific slow-worker scenario needs a future injectable harness.

## 13. Symlink retarget

The watcher now rebuilds a deduplicated set containing the configured symlink entry, its parent, the resolved target, and its parent. Obsolete watches are removed before new paths are added; the existing 75 ms event debounce and event-driven model remain.

The service regression retargets one configured symlink from A to B, preserves the configured symlink string, updates the effective canonical target, and verifies a bounded watch count. Result: `paper-service-test` passed 17/17, including atomic replacement, corruption, disappearance, recovery, and rapid-set coverage.

## 14–15. Replacement, recovery, and rapid switching

Component-level evidence is green:

- `WallpaperService::sourceAtomicReplacementAndCorruptionReconcile` and source disappearance/recovery tests pass.
- The QML surface test performs same-path revisions 1→4 and prevents stale generation promotion.
- The service rapid-set test preserves latest-request-wins behavior.
- The renderer leaves the last usable slot visible until a matching replacement reaches Ready.

A single compositor-mediated test combining watcher → service generation → live QML pixels was not available in the native session, so full end-to-end visual pixel proof remains `UNPROVEN` even though each deterministic component regression passes.

## 16. Factory-default native result

**NATIVE-PROVEN, limited:** the rebuilt LayerShell-enabled `astrea-shell` stayed alive in the available `WAYLAND_DISPLAY=wayland-1` session and created `/run/user/1000/astrea-shell/wallpaper.sock`. A live `astreactl wallpaper get` returned `factoryDefault.source` and `factoryDefault.resolvedSource` as the physical Eclipse `Paper/assets/default.jpg`.

The active configured wallpaper in that session was an existing user wallpaper, so this was a live authoritative snapshot/provenance check, not a visual startup proof that the factory image was displayed.

## 17. Settings native result

`astrea-settings` was not visually qualified in the live session. The physical preview and fit-authority paths are covered by the Settings controller, QML smoke, and offscreen tests; live Settings display agreement remains `UNPROVEN`.

## 18. astreactl native result

**NATIVE-PROVEN:** while the rebuilt shell endpoint was live, `astreactl` successfully applied and read back all five fits against the physical factory asset. Each check asserted both configured and effective fit, then the original configured user wallpaper and Cover fit were restored. A live `wallpaper get` also returned a complete authoritative snapshot.

## 19. Performance and idle behavior

The watcher remains event-driven with bounded debounce and no polling loop. The renderer retains caching for unchanged generations; the generation query changes only when Paper publishes a new generation. Diagnostics are counters/properties, not frame logging. No idle process spawning or continuous decode was added.

## 20. Validation results

Passed:

- `cmake --build build/no-layer-shell --target astrea-shell astrea-settings -j2`
- `cmake --build build/release --target astrea-shell -j2` with LayerShellQt 6.7.4
- focused Paper/Settings CTest: 6/6 passed
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R 'paper|wallpaper|settings' --output-on-failure`: all Paper, wallpaper, QML, structure, profile, and component tests passed except the unrelated navigation model test described below
- `astrea-paper-core_qmllint` and `astrea-settings-ui_qmllint`: completed successfully; Settings retains existing warning-level lint output
- `cargo fmt --check`
- `cargo test astreactl::wallpaper --lib`: 5/5
- `cargo test --bin astreactl`: 2/2
- `cargo check --bin astreactl`
- `git diff --check` in both repositories

The focused Paper results were: descriptor 7/7, resolver 8/8, service 17/17, surface 5/5, control server 8/8. Settings controller was 11/11.

## 21. Remaining blockers

- The full Settings filter has one unrelated failure in the already-dirty navigation catalog: `SettingsNavigationModelTest` expects 12 rows but receives 13 and expects an empty route that is now non-empty. This task did not touch that catalog.
- The broader `cargo test --lib astreactl` result was 21/23. Two pre-existing discovery tests fail constructing paths because the current workspace path exceeds Unix `SUN_LEN`; all 5 Paper wallpaper tests pass.
- The build-tree install command is incomplete because the unrelated Spotlight binary is absent, although the Paper asset itself staged correctly.
- Live Settings UI and compositor pixel-level same-path/fallback visual qualification remain unproven.

## 22. Commits

None. Nothing was staged or committed.

## 23. Final Git status

Eclipse remains dirty with the preserved 290 status entries, including the untracked Paper/Settings v1.1 and v1.1.1 work and unrelated work. Typhon remains dirty with the preserved 113 status entries, including the existing client changes and the wallpaper client additions. No destructive Git operation was used.

## Relevant artifacts

- [v1.1.1 design](/home/agony/GitHub/Eclipse/docs/superpowers/specs/2026-08-19-paper-wallpaper-api-v1-1-1-renderer-integration-design.md)
- [v1.1.1 implementation plan](/home/agony/GitHub/Eclipse/docs/superpowers/plans/2026-08-19-paper-wallpaper-api-v1-1-1-renderer-integration.md)
