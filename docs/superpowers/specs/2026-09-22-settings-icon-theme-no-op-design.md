# Eclipse Settings Icon Theme v1 No-Op Correction

## Goal

Make icon-theme persistence distinguish the projected/effective selection from
the persisted configured intent, so explicitly selecting System Default clears
an unavailable configured theme without changing the existing optimistic
selection behavior.

## Design

`SettingsThemesControllerRust` will own a small no-op predicate used by
`queue_selection_persistence()`:

- When `pending_selection` exists, a request is a no-op only if it matches the
  pending desired selection. This preserves newest-request-wins semantics.
- When no request is pending, a request is a no-op only if it matches both
  `configured_selection` and the committed `ThemeSelection` value. The second
  comparison prevents stale configured state from suppressing an explicit
  action that would change the committed selection.

All generation allocation, pending projection, Qt signal emission, worker
submission, rollback, stale completion handling, and asynchronous persistence
remain unchanged.

## Verification

Add controller regression coverage for an unavailable configured theme that is
explicitly replaced by System Default, including successful completion,
configured-state clearing, persistence removal, and non-reselection after the
theme reappears. Preserve coverage for the true System Default no-op and the
existing optimistic, rollback, ordering, signal, worker, and refresh tests.

Validate with the complete Rust Themes commands and the Settings/shared CMake
build plus the relevant CTest suites, using build output under
`/mnt/Aether/Desktop/GitHub`.
