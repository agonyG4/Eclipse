# Astrea Typography Implementation Plan

> Execute inline using executing-plans. User approved all work; no subagents, branches or worktrees.

**Goal:** Shared semantic typography with theme identity, user precedence and live application propagation.
**Architecture:** Shared C++ resolver and transactional preferences, projected through ThemeController and shared QML typography components. Existing tokens remain compatible.
**Tech Stack:** Qt 6/QML, C++17, CMake, QtTest.
**Spec:** docs/superpowers/specs/2026-11-01-astrea-typography-design.md

## Global Constraints
- Work on main and commit completed logical changes without including unrelated staged changes.
- Build in /mnt/Aether/Desktop/GitHub/Eclipse-build; CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target.
- Preserve theme.json ownership, themes, existing Settings visuals and icon metrics.

## Review Focus
- Theme role family versus user primary inheritance: resolver test.
- Stale processes modifying different keys: transactional controller test.
- Malformed preference document: failed write preserves bytes and published values.
- Unavailable family and invalid numeric values: persistence/validation tests.
- Text scale updates actual consumers without changing icon/spacing tokens: QML propagation test.

### Task 1: Shared resolution and persistence
Files: shared/theme/Typography.hpp/.cpp, shared/theme/ThemeController.hpp/.cpp, shared/theme/ThemePackCatalog.hpp, shared/tests/ThemeTokensTest.cpp, shared/CMakeLists.txt.
Interfaces: Typography::resolve(theme, preferences, proportional, mono) -> QVariantMap; ThemeController typography, fontPreferences, setFontFamilyPreference, setFontSizePreference, setTypographyPreference, setTextScale, resetFontPreferences and typographyError.
- [x] Add behavioral tests and observe failures on the existing implementation.
- [x] Implement semantic roles, compatibility tokens, bounds and independent text scale.
- [x] Implement isolated atomic per-key preference mutations and watcher propagation.
- [x] Run theme token tests, inspect diff, commit scoped changes.

### Task 2: Shared QML and consumer migration
Files: shared/qml/Typography.qml, shared/qml/*Text.qml, shared/cmake shared module definitions, Settings/qml/theme/Tokens.qml, Settings typography wrappers and text consumers, Spotlight/Dock/Shell/Bar/AltTab/Paper QML.
Interfaces: Typography.role(name) -> resolved role map; semantic text types bind font and proportional line height.
- [x] Add live QML role/consumer propagation test and observe failure.
- [x] Register shared types; migrate existing adapters and fixed text metrics while keeping icons independent.
- [x] Run shared consumer and affected application tests; commit scoped changes.

### Task 3: Typography settings and documentation
Files: Settings/qml/pages/appearance/Typography.qml, Settings translations/navigation/tests, themes documentation/example.
- [x] Add Settings page behavior tests for primary/mono/scale/advanced/reset.
- [x] Simplify main panel, add advanced role controls and translated live preview/errors with adaptive height.
- [x] Build affected applications and run relevant CTest suites on Aether.
- [x] Review source audit, document supported theme schema/remaining intentional literals, commit scoped changes.


## Execution record

Implemented inline on main, without subagents or a worktree, as requested. The
shared resolver, isolated preferences and semantic QML types were committed in
`d5dcb22`; legacy-size compatibility and component invalidation in `2ac5021`.

Ruling: typography preferences use a separate typography.json rather than adding
another writer to theme.json. This preserves the Settings ownership contract and
imports the existing font_preferences on the first write.

Ruling: compatibility size aliases retain their existing defaults; migrated text
components select semantic roles explicitly. Theme and user semantic overrides
also project into old aliases so existing consumers continue to update.

Ruling: source inspection supplements the graph. Its Eclipse generation is
2026-10-16T19:37:04Z, and new/changed typography files are not represented by that
generation. Coverage checks and direct source/font-property audits were used;
no completeness claim relies on the stale graph.

Review covered inheritance and override order, independent icons/Mono, malformed
and concurrent writes, watcher recovery, component focus preservation, real QML
bindings, legacy wrappers and remaining intentional glyph/demo metrics. A tooltip
measurement regression test also exposed a pre-existing one-pixel difference
between advance width and shaped text width; sizing now includes the rendered
implicit width and adapts to larger text.

Validation: incremental Debug builds of astrea-settings, astrea-spotlight,
astrea-shell and astrea-dock passed in the existing Aether build tree. Sixteen
relevant CTest suites passed with zero failures, including theme tokens,
consumers, Settings navigation/font policy/full QML/components/Audio/Internet/
Bluetooth, Spotlight, Bar, Dock and Shell context menus. Rendered the Typography
page in dark at 1050 px and light at 800 px. Build logs and captures remain under
/mnt/Aether/Desktop/GitHub/Eclipse-build. The Explorer source is absent; Paper and
Downloads have no text-bearing QML to migrate in this checkout.
