# Typhon Dock Runtime State

The Dock consumes one authoritative Typhon toplevel connection. A committed
snapshot is the only source of `running`, `active`, and `windowCount` state.
Launch-helper success is separate `launching` state and never implies that a
window exists.

## Matching and grouping

`TyphonAppMatcher` resolves a published client app ID in this order:

1. Exact desktop filename when the app ID ends in `.desktop`.
2. Exact desktop ID.
3. Case-insensitive desktop ID.
4. Exact `StartupWMClass`.
5. Case-insensitive `StartupWMClass`.
6. Normalized reverse-DNS desktop ID.
7. Unresolved launcher metadata.

Titles and PIDs are never used as identity. First-party applications should set
their canonical desktop ID, for example Explorer publishes `astrea-explorer`
for `astrea-explorer.desktop`, but launcher resolution is enrichment rather than
task admission. Every eligible Typhon toplevel enters the projection.

The runtime identity tracker assigns one sticky runtime task key per live
`WindowId` and Typhon connection generation. Its initial candidate is:

```text
desktop:<desktopFileName>       deterministic desktop match
app:<case-folded trimmed appId>  otherwise when app_id is non-empty
window:<WindowId>               otherwise
```

Case-folding groups equivalent app IDs; punctuation such as `_` is not rewritten
to `-`. Once assigned, a live window keeps its key through catalog rebuilds,
launcher appearance/disappearance, app-ID changes, title/PID changes, and
repeated snapshots. Newly observed windows with the same normalized non-empty
app ID join an existing live cohort; empty app IDs never create a shared
cohort. A generation change or authority loss clears the assignments, so a
later connection or lifetime may choose a new initial key. PIDs and titles are
never task keys, and asynchronous identity resolution never changes a live
task key. A task uses title, app ID, then `Application` for fallback
presentation until the shared application-identity resolver supplies richer
display or icon metadata.

Windows are grouped into one state per task key. Minimized windows remain
running, active is true when any grouped window is active, and duplicate PIDs
remain separate windows. `windowIds` are ordered by descending Typhon focus
serial, so the first ID is the exact activation candidate. `desktopFileName` and
`desktopId` are optional launcher metadata, not runtime identity.

## Model membership and ordering

The model owns two inputs:

```text
configured pins + running runtime-only task keys
```

Pins remain persisted as raw desktop filenames. While a matching live runtime
state exists, its stable key occupies the configured pin position; only after
the last live window closes does the row transition to
`desktop:<desktopFileName>`. The projector's `encounterOrder` appends newly
observed runtime-only tasks to a model-owned dynamic order. Focus-only updates
change runtime roles but do not reorder existing dynamic rows. `resolved=false`
is valid alongside `runtimeKnown=true` and `running=true`. Runtime-only tasks
without a real launcher identity cannot be pinned; acquiring a launcher makes
them pinnable without changing their live key or persistence format.

## Authority and activation

While Typhon is authoritative, a resolved pinned application missing from the
projection is known stopped. On disconnect, degradation, unsupported protocol,
or another authority loss, pins remain visible with neutral unknown values and
runtime-only rows are removed. Stale runtime-only order is never retained.

When `runtimeKnown && running` is true, a click uses the retained exact
`WindowId` and sends Typhon `Activate`, including for minimized and multi-window
applications. Otherwise a real desktop launcher is required to launch; a
runtime-only task has no launch fallback. Exact activate/close operations first
validate that the requested `WindowId` still belongs to the task key.
Accepted and no-change results do not launch. Unavailable or failed actions
reconcile the snapshot and never launch a duplicate application on that same
click. Minimize anchors are stored by task key and published to every live
WindowId in the group.
