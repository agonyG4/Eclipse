# Paper Full-Output Wallpaper Contract Closure Report

Date: 2026-08-20
Repository: Eclipse
Baseline: `0afe2696bfb72e7a459f1f5c4f7872054039230b`

## Outcome

Paper now owns the wallpaper surface contract explicitly. `WallpaperSurfacePolicy::background()` requests a Background layer-shell surface with None keyboard interactivity, all four anchors, `exclusiveZone = -1`, zero margins, and the supplied `QScreen`. `WallpaperSurfaceBundle` consumes that policy, so each bundle remains tied to its current screen and the existing global wallpaper selection path is unchanged.

This closes the deterministic client-policy gap without changing generic Typhon layer-shell semantics, Paper catalog/state APIs, output selection, or shared helper defaults.

## Root cause and evidence

The previous bundle reconstructed its config locally and left `exclusiveZone` at the shared default of `0`. That made the wallpaper surface participate in usable-area arrangement instead of explicitly requesting the complete output. The existing geometry path already uses `QScreen::geometry()`; no `availableGeometry()` substitution was required.

The final policy is covered at field level by `Paper/tests/WallpaperSurfacePolicyTest.cpp` and its use by the bundle is covered by the source-contract CI check. The Typhon protocol regression separately verifies that generic positive reservations remain usable-area reservations while a zone `-1` Background surface remains full-output.

## Changed files

- `Paper/platform/wayland/WallpaperSurfacePolicy.hpp/.cpp`: explicit full-output policy.
- `Paper/platform/wayland/WallpaperSurfaceBundle.cpp`: policy consumption.
- `Paper/tests/WallpaperSurfacePolicyTest.cpp`: deterministic QTest contract.
- `Paper/CMakeLists.txt`: policy source and test registration.
- `tools/ci/tests/test_layer_shell_contract.py`: narrow source defense.
- `docs/superpowers/specs/2026-08-20-paper-full-output-wallpaper-contract-design.md`: design/evidence record.
- `docs/superpowers/plans/2026-08-20-paper-full-output-wallpaper-contract-closure.md`: implementation plan.

## Verification

Fresh focused validation for the closure:

- `cmake --build build/no-layer-shell --target paper-surface-policy-test paper-surface-manager-test paper-descriptor-test paper-catalog-test paper-persistence-test paper-resolver-test paper-service-test paper-control-server-test settings-wallpaper-controller-test astrea-shell astrea-settings -j2` — passed.
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build/no-layer-shell -R "paper-|settings-wallpaper" --output-on-failure` — **9/9 passed**.
- `python3 tools/ci/tests/test_layer_shell_contract.py` — **10/10 passed**.
- `git diff --check` — included in final validation.

The policy test and existing surface-manager test were also run together and passed **2/2**.

## Qualification limits

No native Astrea session was available in this closure pass. Therefore this report makes no native configure-event, screenshot, or visual-coverage claim. The remaining native acceptance is to observe a real Astrea session and confirm that the Paper wallpaper remains full-output beneath Topbar/Dock surfaces while those surfaces continue to define usable geometry.

The worktree contained substantial unrelated dirty changes before this task; they were preserved. No commit or staging operation was requested or performed.

## Final status

Deterministic Paper contract closure: validated by focused build, QTest/CTest, and CI source checks. Native visual/session qualification: pending.
