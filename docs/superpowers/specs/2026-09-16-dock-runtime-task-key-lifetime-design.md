# Eclipse Runtime Task-Key Lifetime Stability

## Goal

Keep each Dock runtime task's `taskKey` stable for the complete lifetime of
its live Typhon toplevels, while allowing launcher metadata and presentation
identity to change independently.

## Context and constraints

- A committed Typhon snapshot is authoritative for live runtime tasks.
- `.desktop` data is optional launcher metadata and must not own runtime task
  identity.
- Pin configuration persists only `desktopFileName` values.
- Generic runtime operations use `taskKey`; exact compositor operations also
  validate the exact `WindowId`.
- Typhon protocols, the shared application-identity resolver's authority, and
  the broader Dock identity design are out of scope.
- No Steam-, Proton-, Wine-, or `steamwebhelper`-specific behavior is added.

## Architecture

### Runtime identity ownership

Introduce `RuntimeTaskIdentityTracker` in the shared Typhon platform layer.
`DockController` owns one tracker instance because it owns the authoritative
runtime lifetime. The tracker receives each Typhon snapshot and the current
desktop catalog and returns a `WindowId → taskKey` assignment for that
snapshot.

The tracker keeps:

1. the assigned key for every currently known live `WindowId`;
2. the current Typhon connection generation; and
3. a mapping from each non-empty normalized runtime `app_id` cohort to its
   currently live task key.

For an existing `WindowId`, the stored key always wins, regardless of title,
PID, app ID, catalog contents, or launcher match changes. For a newly seen
window, an existing live non-empty app-ID cohort wins. Otherwise the tracker
assigns the deterministic initial key (`desktop:`, `app:`, or `window:`) from
the current catalog and metadata. Empty-app-ID windows never create a shared
cohort. Missing windows are pruned; when the last window for a task disappears,
its cohort mapping is released. A generation change or explicit authority loss
clears all assignments so a later connection starts a new runtime lifetime.

`DockApplicationStateProjector` remains deterministic and stateless. It keeps
its current matcher and presentation-enrichment logic, but accepts the
tracker's assignments when available. Its output key comes from the assignment
map; the catalog can therefore update `desktopFileName`, `desktopId`, name, and
icon fields without changing the key.

### Model reconciliation

`DockAppModel::reconcileRows()` builds configured-pin rows as follows:

1. for each configured desktop filename, select the live runtime state whose
   `desktopFileName` matches it, when one exists;
2. use that live state's stable runtime key for the row;
3. otherwise use the normal stopped synthetic `desktop:<desktopFileName>` key;
4. append unpinned live runtime tasks in their existing first-observed order.

The live matching row is marked pinned from its launcher filename, so a stable
`app:` or `window:` runtime key can be pinned without a duplicate synthetic
row. When the last live window disappears, no runtime state is available and a
configured pin naturally returns to its synthetic `desktop:` key; an unpinned
runtime row is removed.

### Compatibility and actions

`windowsForDesktopFileName()` searches authoritative live runtime states for a
matching launcher filename and delegates to `windowsForTaskKey()` using the
found stable key. It never reconstructs a runtime key from a desktop filename.
Minimize anchors remain keyed by the live task key, and exact activation/close
operations continue to validate both that key and the requested live
`WindowId`.

## Tests

Add failing regression tests before implementation for:

- catalog gain and loss during a live task;
- launcher pin merge, stopped pinned transition, unpinned disappearance, and
  a fresh key after the old lifetime ends;
- app-ID mutation and a new same-app window joining an existing cohort;
- generation changes and authority loss clearing the tracker lifetime;
- exact action lookup, desktop-filename compatibility lookup, and minimize
  anchor publication after catalog rebuild;
- model row order, duplicate prevention, launcher metadata, and structural
  signal stability.

Run the focused Dock/shared tests first, then the available broad suite. Build
artifacts stay in the repository's existing build directories.
