# Astrea Settings Compositor Preview Design

## Scope

This follow-up preserves the source-preserving native Settings migration and
adds only the requested navigation correction, a visual-only Compositor page,
and focused native foundation hardening. Existing shell geometry, colors,
typography, spacing, controls, profile composition, and navigation visuals are
not redesigned.

## Navigation

The existing navigation catalogue remains the source of truth. Entries with
`kind: "group"` are normal selectable rows, matching the legacy `NavItem`
composition. No current catalogue entry is collapsible. Generic section support
is retained only if it remains used by the model and is not exposed for group
rows.

The catalogue gains `compositor` immediately after `services` and before the
spacer. It uses page index `18`, label key `settings.nav.compositor`, subtitle
`Astrea compositor preferences`, and an existing Nerd Font symbol distinct from
the Components and Services symbols.

## Page Boundary

The current neutral Loader remains the page boundary. `Main.qml` maps the
selected navigation ID to the explicitly registered Compositor QML source and
leaves unimplemented routes neutral. The Compositor page is recreated when the
selection returns to it, so its state is intentionally ephemeral.

## Compositor Page

`Compositor.qml` uses only the existing `ScrollPage`, `SectionHeader`,
`FormCard`, `SettingRow`, `ToggleSwitch`, `SelectButton`, and `Theme`
components. It uses `maxWidth: 900`, existing page margins and tokens, and the
specified four sections and rows. Every preview value is a page-local QML
property with the requested default. Control signals update only those local
properties and never mutate a bound control property.

The page has no backend object and no persistence, filesystem, process, shell,
IPC, compositor protocol, timer, or ThemeController dependency. All visible
strings have Settings translation keys and English fallback strings.

## Native Foundation

`SettingsController::isSudo` uses direct libc/NSS group lookup for `wheel` and
`sudo`, treating lookup failure as false. The group-name decision is isolated
in a pure helper for tests. `ThemeControllerTest` is a dedicated CTest target.
An offscreen QML smoke test constructs the native controllers, loads the
registered module, checks one root object, selects `compositor`, verifies the
page object, and fails on fatal QML warnings.

## Verification

Verification uses fresh Debug, Release, ASan, and UBSan build directories. It
includes full CTest, `ctest -N`, Settings model/controller and ThemeController
tests, the offscreen QML smoke test, `qmllint` for every registered QML file,
forbidden-dependency scans, `git diff --check`, and live qualification of the
fresh native executable under Hyprland. No Typhon session is launched.
