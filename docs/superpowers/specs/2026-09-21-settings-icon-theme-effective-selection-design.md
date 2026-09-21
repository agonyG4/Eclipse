# Settings Icon Theme Effective Selection Design

## Goal

Make icon-theme card selection visible immediately while keeping persistence asynchronous, bounded, generation-protected, and rollback-safe.

## Design

`ThemeSelection` remains the committed state. `PendingSelection` is the projected state while a persistence request is in flight. The QML-facing `selectedIconTheme` reads the pending optional theme first and falls back to the committed selection otherwise. An empty pending value therefore projects System Default without mutating the committed selection.

Every transition compares the effective value before and after the transition:

- enqueueing a different pending request emits `selectedIconThemeChanged` immediately;
- a newer pending request replaces the projection and emits once if its value differs;
- a matching success commits the request, clears pending state, and emits only if the effective value changed;
- a matching failure clears pending state, restores the committed value, and emits the rollback;
- stale completions are ignored completely;
- refresh reconciliation emits only when no pending projection exists and the effective value changes.

Selecting the current effective value is a no-op. The existing worker request coalescing and generation checks remain authoritative.

The existing combined `busy` state is preserved for operational compatibility. A read-only `refreshing` property mirrors catalog scanning (`refresh_busy`) so the Themes page shows “Loading installed themes…” only during catalog refresh. Selection persistence remains asynchronous and does not disable the page or prevent another selection.

## Error handling

Persistence failures leave `ThemeSelection` and `configured_selection` unchanged, clear the matching pending request, restore the committed effective value, and expose the worker error through `lastError`. A stale failure cannot overwrite a newer request or its error state.

## Verification

Update the Rust controller tests for effective projection, stale completion, rollback, no-op behavior, and effective-value signal effects. Update the CXX-Qt controller tests for blocked immediate theme/System Default projection, rapid newest-request-wins behavior, and signal counts. Add QML smoke coverage that catalog-loading text is not visible when only persistence is pending. Run the complete requested Rust, CXX-Qt, Settings QML, ThemeController, AstreaIconProvider, navigation, and structure suites with build output under `/mnt/Aether/Desktop/GitHub`.
