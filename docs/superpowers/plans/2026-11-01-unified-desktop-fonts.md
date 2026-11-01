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
- [ ] Add failing live tests for topbar clock and Spotlight surface metrics, including primary-family precedence.
- [ ] Add shared font() factory, surface routing and consistent fallback/rendering. Remove hardcoded text weight/tracking and Display-family use in the clock. Preserve icon metrics.
- [ ] Measure menus with the same QFont objects rendered by QML, retaining the old measurement API as a compatibility entry point.
- [ ] Build affected applications, run consumer/Bar/Spotlight/Dock/context menu/Settings suites and inspect typography artifacts.
- [ ] Document a complete theme example, review diffs and commit logical changes.
