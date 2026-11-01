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
- [ ] Add behavioral tests and observe failures on the existing implementation.
- [ ] Implement semantic roles, compatibility tokens, bounds and independent text scale.
- [ ] Implement isolated atomic per-key preference mutations and watcher propagation.
- [ ] Run theme token tests, inspect diff, commit scoped changes.

### Task 2: Shared QML and consumer migration
Files: shared/qml/Typography.qml, shared/qml/*Text.qml, shared/cmake shared module definitions, Settings/qml/theme/Tokens.qml, Settings typography wrappers and text consumers, Spotlight/Dock/Shell/Bar/AltTab/Paper QML.
Interfaces: Typography.role(name) -> resolved role map; semantic text types bind font and proportional line height.
- [ ] Add live QML role/consumer propagation test and observe failure.
- [ ] Register shared types; migrate existing adapters and fixed text metrics while keeping icons independent.
- [ ] Run shared consumer and affected application tests; commit scoped changes.

### Task 3: Typography settings and documentation
Files: Settings/qml/pages/appearance/Typography.qml, Settings translations/navigation/tests, themes documentation/example.
- [ ] Add Settings page behavior tests for primary/mono/scale/advanced/reset.
- [ ] Simplify main panel, add advanced role controls and translated live preview/errors with adaptive height.
- [ ] Build affected applications and run relevant CTest suites on Aether.
- [ ] Review source audit, document supported theme schema/remaining intentional literals, commit scoped changes.
