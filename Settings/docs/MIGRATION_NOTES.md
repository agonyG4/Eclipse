# Settings Migration Notes

The legacy Settings shell was treated as visual design input while the native
target adopted Eclipse's existing C++/Qt/QML boundaries. The approved window,
sidebar, theme, transparency, typography, spacing, animation, profile, and
navigation visuals remain source-preserved.

## Preserved

- Inter and JetBrains Mono font names;
- the legacy typography scale, radius family, spacing scale, and animation
  durations;
- dark/light palette intent and glass shell treatment;
- the 256 px sidebar, profile header, navigation row geometry, and selected
  accent treatment;
- native frameless desktop window behavior and `startSystemMove()`;
- the legacy form-card, setting-row, toggle, selector, and section-header
  visual components.

The current navigation catalogue has no collapsible sections. Performance,
Appearance, and More Settings remain ordinary selectable `group` rows.

## Native Boundaries

- lifecycle and QML startup are owned by `SettingsApplication`;
- navigation and filtering are owned by `SettingsNavigationModel`;
- account metadata, avatar/icon resolution, and administrative-group detection
  are owned by `SettingsController` and use native libc/NSS APIs without
  subprocesses;
- theme configuration is owned by `ThemeController`;
- bundled English translation lookup is owned by
  `SettingsTranslationController`;
- presentation and interaction remain in QML.

The first real route is `qml/pages/system/Compositor.qml`, selected by the
`compositor` navigation ID and loaded through the existing Loader boundary.

## Compositor Preview Policy

The Compositor page is deliberately visual-only. Toggle and selector values
are page-local QML state. Leaving the page destroys those values and recreates
the requested defaults on return. Closing and reopening the application also
resets them. No Compositor value is persisted, applied, read from a backend, or
sent through IPC.

Future compositor integration is explicitly deferred. There is currently no
Hyprland integration, compositor protocol, private Typhon protocol, service
backend, shell command, process execution, or IPC boundary for this page.

## Source Policy

The native Settings target contains no Quickshell import, QML process object,
LayerShellQt integration, Hyprland command, shell invocation, or Typhon-private
protocol. Deleted legacy component paths are not registered by the current
QML module. It links only the compositor-independent `astrea-shared-core`
target and its QML plugin. Layer Shell functionality belongs to the separate
`astrea-shared-layer-shell` target used by Dock, Spotlight, and AltTab.
