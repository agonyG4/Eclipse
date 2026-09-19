# Settings Themes Page Design

## Goal

Add the first production Settings Themes destination for installed Freedesktop icon themes, with selection persisted as `system_icon_theme` and projected from Rust through CXX-Qt.

## Ownership and data flow

`SettingsThemesController` is generated from `Settings/backend/src/themes/qobject.rs` and exposed as `SettingsController.themes`. It projects `themes`, `selectedIconTheme`, `busy`, `lastError`, `refresh()`, `setIconTheme()`, and `useSystemDefault()` without owning domain policy.

Rust owns theme search paths, deduplicated catalog construction, validated `index.theme` metadata, recursive preview lookup, selected-theme validation, conservative atomic persistence, and a single bounded worker. The worker performs discovery and parsing off the Qt GUI thread, then queues one result at a time to the QObject thread using the same CXX-Qt `qt_thread.queue` pattern used by Animations.

The persisted preference is `system_icon_theme` in `~/.config/AstreaOS/ui/theme.json`. Missing or empty means System Default. Rust reads and writes only this key, preserves a valid existing JSON object, removes only this key for System Default, and refuses to fabricate a replacement when an existing document is malformed.

The shared C++ layer remains responsible for Qt/QIcon integration. `AstreaIconTheme::resolveWithSourceUnlocked()` adds the persisted preference between `QS_ICON_THEME` and qt6ct while keeping environment overrides authoritative. `AstreaIconProvider` continues to watch `theme.json`, call `AstreaIconTheme::apply()`, clear caches, and increment `themeRevision`; no second broadcast or process restart is introduced.

## Rust domain units

- `catalog.rs`: ordered search roots, theme discovery, ID deduplication, hidden filtering, split-root merging, and presentation descriptors.
- `config.rs`: `theme.json` path resolution, JSON key preservation, atomic same-directory replacement, and System Default clearing.
- `icon_lookup.rs`: explicit-theme preview resolution with inheritance/cycle bounds, hicolor fallback, directory-type matching, and local file URLs.
- `state.rs`: selected-theme state, validation against the current catalog, busy/error snapshots, and result projection data.
- `worker.rs`: one bounded refresh queue and filesystem work.
- `qobject.rs`: CXX-Qt properties, invokables, signals, and queued worker completion.
- `mod.rs`: module registration and focused public test seams.

## Navigation and QML

The native catalogue gains a stable `themes` Page under `customization`, ordered after Appearance and before Wallpaper, with `sidebarVisible=false` and a route to `pages/appearance/Themes.qml`. The generic Hub continues to render children from descriptors.

`Themes.qml` uses `Form.ScrollPage` with an approximately 900 px maximum width, a Themes heading, Icon Theme section, existing `Controls.SearchField`, and a responsive two-column card grid. System Default is always first. Cards render Rust-provided preview URLs only, use focus/hover/pressed/selected states matching Appearance, and activate on click, Space, or Enter. Loading, error, and empty states are bounded and do not prevent navigation. Appearance’s `iconAppearance` cards remain unchanged.

## Verification strategy

TDD coverage will exercise catalog priority/deduplication/hidden/split roots, metadata, inheritance and preview matching, selection validation, System Default, persistence preservation/atomicity, and bounded worker behavior. C++ tests will cover persisted-theme precedence, environment overrides, invalid fallback, watcher invalidation, and ThemeController unknown-key preservation. Navigation tests will cover route/order/ancestor/history behavior; QML smoke tests will cover page registration/construction, selected card, keyboard activation, filtering, and state projections.

Documentation will explicitly distinguish QML presentation, Rust Themes domain, CXX-Qt projection, shared C++ rendering integration, legacy `icon_theme`, and canonical `system_icon_theme`.
