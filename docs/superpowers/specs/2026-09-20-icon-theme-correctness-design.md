# Settings Icon Theme Correctness Design

## Approved architecture

Keep `Themes.qml` as presentation, `SettingsThemesController` as a thin CXX-Qt
projection, Rust as owner of icon-theme discovery, metadata, preview lookup,
selection, persistence, and worker logic, and shared C++ as the active QIcon
rendering integration. `system_icon_theme` remains separate from legacy
`icon_theme`; the existing `AstreaIconProvider` watcher and cache invalidation
pipeline remains authoritative.

## Discovery and lookup

Discovery will collect all existing roots for each theme ID in base-directory
priority order, then use metadata and directory declarations from only the first
valid `index.theme`. A theme with no valid index is omitted. Preview lookup will
search exact size and scale matches across every root before doing closest-size
fallback. Closest candidates use Freedesktop size distance, then declared
directory order and root priority as tie breakers. Theme inheritance is visited
after local lookup fails, with bounded cycle protection and a final `hicolor`
lookup. Requested nominal size and scale are separate.

Missing directory `Type` means `Threshold`; scaled thresholds and scalable
bounds use pixel-space distance. A valid, safely validated `Example` icon name
is tried as an additional theme preview before semantic representatives.

## Projection and page states

Each catalog refresh reloads the persisted `system_icon_theme` preference in
the worker and reconciles the projected selection against the refreshed catalog.
Missing configured themes display System Default without changing the stored
preference, allowing later reappearance. Worker startup failure becomes
`lastError` while leaving the controller usable. Installed themes sort by
case-insensitive display name and then ID; QML continues to inject System
Default first. The default card renders a representative active icon through
the registered Astrea icon image provider. The page gains an installed-catalog
empty state alongside the existing search-results state.

## Verification

Add behavioral Rust regressions for split roots, first-index authority,
root/directory ordering, exact-before-closest, size versus scale, directory
distances, Example fallback, preference reconciliation, worker startup failure,
and deterministic ordering. Exercise QML empty and search-empty visibility as
observable items. Preserve existing persistence, theme precedence, watcher,
navigation, selection, accessibility, and responsive layout tests. Validate
with the requested Rust formatting, test, Clippy, Qt/CXX-Qt Settings and shared
CTest suites, and Settings static/structure tests.
