# Eclipse Settings — Astrea Slider and QML Control Taxonomy Design

**Status:** Approved for inline implementation on `main`.

## Goal

Add a reusable `Astrea.Settings.Slider` for the Settings module, migrate all
nine Dock numeric controls without changing their behavior or persistence, and
move reusable interactive controls out of the structural `form/` category.

## Architecture

`Settings/qml/components/controls/Slider.qml` will derive directly from
`QtQuick.Controls.Slider` through a qualified `QtQuick.Controls as QQC2`
import. It will replace only the `background` and `handle` delegates, using
`visualPosition` for filled progress and handle placement. The control remains
pure QML: inherited range, value, step, live, pressed, moved, focus, keyboard,
touch, mouse, and RTL behavior remain Qt-owned. It will expose no Dock-specific
state or persistence API and will keep its geometry, theme colors, restrained
state styling, and optional endpoint glyphs centralized.

The Dock page will import `../../components/controls` as `Controls` and use
`Controls.Slider`, `Controls.ToggleSwitch`, and `Controls.SelectButton`. All
nine existing slider object names, ranges, enabled bindings, setter calls,
rounding, `onMoved` behavior, and flush-on-release handlers remain unchanged.
Wallpaper and Compositor will use the same `Controls` alias for their existing
ToggleSwitch and SelectButton instances. The three control files move from
`components/form/` to `components/controls/` without internal behavior edits;
`form/` remains the owner of Settings-specific layout and composition.

The Qt Quick Controls `Basic` style remains selected in `Settings/app/main.cpp`.
No global style, backend state, persistence logic, blur, Liquid Glass,
MultiEffect, ShaderEffect, Canvas, or custom drag state is introduced.

## Visual contract

The slider is horizontal with an approximately 4 px neutral rounded track, an
accent-filled minimum-to-thumb segment, and a compact approximately 28 x 18 px
lozenge thumb that is centered on the track and may extend beyond it. The thumb
uses a solid theme-aware surface, subtle border, and cheap non-blurred depth
cue. Optional small/large endpoint glyphs are simple non-interactive QML
rectangles using secondary/tertiary theme colors and dimming when disabled.
Hover/pressed styling may be restrained, but handle position is not animated so
Dock preview feedback remains direct. No ticks or value bubble are included.

## Registration and tests

The QML module list will contain exactly 40 files: the new Slider plus the same
39 existing files with the three moved paths substituted. The component smoke
fixture will instantiate `Settings.Slider` with a named object and
representative range/value, check inherited properties, and exercise
`increase()` when supported by the test environment. The Dock offscreen smoke
test will retain existing object-name checks and a structural invariant will
require the Controls import/use while rejecting direct unqualified Slider
instances. The structure test will require the four new control ownership paths
and reject the old form paths both in registration and on disk.

Documentation will state the `controls/` versus `form/` ownership rule and the
preference for wrapping/deriving Qt Quick Controls semantics instead of
rebuilding pointer, keyboard, or touch interaction with `MouseArea`.

## Verification and manual qualification

Fresh in-repository Debug and Release build directories will be configured and
verified with the Settings build, CTest, and `astrea-settings-ui_qmllint`
commands documented in `Settings/docs/TESTING.md`. Source audits will check
for stale form references, stale registered paths, and raw Dock Slider blocks.
Automated checks will not claim visual parity. The final handoff will provide
the exact freshly built `astrea-settings` executable path and the requested
manual light/dark, accent, range, hover/pressed, disabled-state, live-preview,
and minimum-size inspection checklist.

## Non-goals

No Dock schema/controller semantics, resident Dock QML, Typhon, Shell runtime,
Paper semantics, native persistence, unrelated controls, Wallpaper dialog
buttons, global Qt style, heavy visual effects, vertical sliders, RangeSlider,
or screenshot automation will change.
