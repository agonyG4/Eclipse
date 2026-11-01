# Unified Desktop Fonts Implementation Plan

Approved as a continuation of the Astrea Typography System; execute inline on main without subagents.

**Goal:** Topbar, Spotlight and other built-in consumers inherit one primary family and consume complete resolved styles, with declarative theme fine tuning.
**Architecture:** Extend the existing resolver with optional theme surfaces (`topbar`, `spotlight`, `dock`, `menu`, `settings`). Each surface refines the same six semantic roles. Global user overrides apply after theme surface overrides, and scale applies once. Shared Typography exposes surface-aware role() and font() accessors. No per-application proportional-font fallback remains.
**Spec:** docs/superpowers/specs/2026-11-01-astrea-typography-design.md, plus this approved follow-up.

## Constraints and review focus
- Existing theme/prefs formats remain compatible; new JSON shape is style.typography.surfaces.<surface>.<role>.<field>.
- Preserve user overrides, icon and Mono independence, and safe numeric validation.
- Fontconfig fallback and one shared SDK fallback supply missing families.
- Use the existing Aether Debug build tree, explicit Cargo target and TMPDIR on Aether. Release for use, if prepared, uses the exact local release commands and preset only.
- No desktop process restart is implicit in source/build work: the running Shell is an older, deleted executable and requires replacement to show compiled changes.

### Task 1: Theme surface resolution
Files: shared/theme/Typography.hpp/.cpp, shared/theme/ThemePackCatalog.hpp, shared/tests/ThemeTokensTest.cpp.
- [x] Add and observe a failing test for surface isolation, invalid values, family inheritance, user precedence and scale-once.
- [x] Validate the five optional surface maps; resolve each with the existing global rules and project typographySurfaces.
- [x] Run token tests and commit the engine change.

### Task 2: Actual consumer unification
Files: shared/qml/Typography.qml, ThemeTokens.qml, ShellMaterialTheme.qml; Settings theme adapter; Bar/Spotlight/Dock/Shell QML; shared/tests/ThemeConsumersQmlTest.cpp; Shell/core/ContextMenuModel.hpp/.cpp and tests.
- [x] Add failing live tests for topbar clock and Spotlight surface metrics, including primary-family precedence.
- [x] Add shared font() factory, surface routing and consistent fallback/rendering. Remove hardcoded text weight/tracking and Display-family use in the clock. Preserve icon metrics.
- [x] Measure menus with the same QFont objects rendered by QML, retaining the old measurement API as a compatibility entry point.
- [x] Build affected applications, run consumer/Bar/Spotlight/Dock/context menu/Settings suites and inspect typography artifacts.
- [x] Document a complete theme example, review diffs and commit logical changes.


## Validation and review record

Engine change committed as ffab6ed. The initial token test failed because surface
styles were absent; the live clock/Spotlight test then reproduced the legacy
clock size and weight. The new menu test initially failed because no QFont-aware
measurement API existed. All now pass, including theme-file live reload,
user-family/role precedence, scale-once, and native font rendering.

Fourteen relevant CTest suites passed in /mnt/Aether/Desktop/GitHub/Eclipse-build
with zero failures. An additional capture run passed with current test sources;
inspected tmp/unified-typography/topbar-spotlight.png. The Qt font factory
quantizes tracking, so rendered-font assertions use subpixel tolerance while
resolved token values retain the configured number.

Release for use: verified binaryDir is /home/agony/GitHub/Eclipse/build/release,
then ran exactly cmake --preset release and cmake --build --preset release
-j"$(nproc)" from the home checkout, with no Aether environment setup or output
redirection. Configure and full build both succeeded; Cargo artifacts stayed
local through the Release preset. No live desktop process was restarted. The
running shell still maps the older deleted executable and must be reopened to
load compiled changes.

Review was inline under the user's no-subagent rule. Graph generation remains
2026-10-16T19:37:04Z; checked changed paths/scopes and used current source for
stale, missing and partial coverage. Reviewed surface whitelist/validation,
legacy API compatibility, user priority, icon/Mono separation, QML scope routing,
font/rendering consistency and menu/preview/tooltip geometry.
