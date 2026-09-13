# Appearance Settings Visual Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Improve density, preview legibility, selection treatment, and material communication on `Settings > Customization > Appearance` without changing state ownership or icon rendering architecture.

**Architecture:** Keep the existing local `ChoiceCard` abstraction and its activation flow. Add small group-specific layout properties, deterministic preview-only layers, and subtle selection overlays inside `Appearance.qml`; leave `Theme`, `State`, `ThemeController`, `Shared.AstreaAppIcon`, and all shell consumers unchanged.

**Tech Stack:** Qt Quick, Qt Quick Layouts, Astrea Settings QML theme tokens, existing QML integration/structure tests, CMake/CTest, qmllint.

## Global Constraints

- Modify primarily `Settings/qml/pages/appearance/Appearance.qml`.
- Preserve all existing object names, keyboard/pointer interaction, live theme mutation, persistence, and reverse watcher flow.
- Preserve `Shared.AstreaAppIcon` with `appearanceOverride` and `tintColorOverride` for icon previews.
- Do not modify the shared icon renderer, provider, theme resolver, Dock, Spotlight, or AltTab.
- Keep `Form.ScrollPage` margins `32` and `maxWidth` `760`.
- Reuse the existing `build` directory and compile with `--parallel 4`.

### Task 1: Inspect and baseline the appearance page

**Files:**
- Read: `Settings/qml/pages/appearance/Appearance.qml`
- Read: `Settings/docs/TESTING.md`

- [x] Confirm the current `ChoiceCard`, preview, section spacing, and stable object names.
- [x] Confirm the documented source audits and verification commands.

### Task 2: Refine the shared ChoiceCard density and selection compositing

**Files:**
- Modify: `Settings/qml/pages/appearance/Appearance.qml`

- [x] Add small local layout properties so appearance/interface cards retain a roughly 164–170 px height while icon cards target roughly 136–144 px.
- [x] Reduce only the icon preview frame height and increase its three production-rendered icon previews to about 36–38 px with 9–11 px spacing.
- [x] Keep the base card surface on `Components.Theme.cardBg`; add a subtle 4–6% accent selection wash while retaining the 2 px border and check indicator.
- [x] Preserve existing hover, press scale, focus, activation, and object names.

### Task 3: Improve deterministic preview legibility

**Files:**
- Modify: `Settings/qml/pages/appearance/Appearance.qml`

- [x] Give Interface Style previews a deterministic multi-color backdrop with blue, purple/pink, teal/cyan, and neutral shapes.
- [x] Keep Default substantially opaque, Transparent visibly translucent, and Frosted softly translucent with reduced contrast, without blur effects or gradients.
- [x] Split Automatic into clipped light and dark halves so background, surface, chrome dots, and text details match each half.
- [x] Keep standalone Light and Dark previews visually aligned with the corresponding Automatic halves.

### Task 4: Tighten page rhythm without changing the section hierarchy

**Files:**
- Modify: `Settings/qml/pages/appearance/Appearance.qml`

- [x] Reduce the three-choice card bottom margins and related section gaps modestly while keeping clear section separation and comfortable click targets.
- [x] Leave the Accent Color row compact and behaviorally unchanged, making only alignment-level spacing adjustments if needed.

### Task 5: Verify behavior and source policy

**Files:**
- Verify: `Settings/qml/pages/appearance/Appearance.qml`
- Verify: existing Settings and shared tests

- [x] Run `cmake --build build --parallel 4`.
- [x] Run the documented focused CTest regex.
- [x] Build `astrea-settings-ui_qmllint` and record registered/linted counts plus warning/error counts.
- [x] Run the documented Settings source audits.
- [x] Run full CTest and `git diff --check`.
- [x] Report the built Settings executable path and leave manual desktop qualification to the user per `Settings/docs/TESTING.md`.
