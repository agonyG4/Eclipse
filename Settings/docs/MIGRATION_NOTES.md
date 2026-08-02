# Settings Migration Notes

The legacy Settings shell was treated as visual design input, not as a
production architecture. The native target splits lifecycle, model state,
window composition, and reusable controls into Eclipse's existing boundaries.

## Preserved

- Inter and JetBrains Mono font names;
- the legacy typography scale, 8/10/12 radius family, spacing scale, and 100-250 ms animation durations;
- dark/light application palette intent;
- expanded 256 px and collapsed 78 px sidebar states;
- profile header, search field, navigation delegates, title bar, and reusable form controls;
- normal desktop window behavior with frameless native chrome.

## Replaced or deferred

- QML page URLs and `Loader` routing are replaced by stable C++ navigation IDs;
- the manual global-coordinate drag loop is replaced by `startSystemMove()`;
- component-local icon discovery is replaced by `Astrea.Shared` and `astrea-shared`;
- avatar display is read-only and falls back to initials;
- group membership, sudo badges, and all process execution are omitted;
- process/file-backed theme state is replaced by a static in-memory palette;
- concrete pages and all system/service integrations are deferred to separately designed work.

## Source policy

No legacy Quickshell import, QML process object, shell command, Hyprland
command, JSON adapter, file mutation, LayerShellQt integration, or Typhon
private protocol was migrated. The exact source-to-target decisions remain in
the package manifest used to prepare this target.
