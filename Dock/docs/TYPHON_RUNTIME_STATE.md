# Typhon Dock Runtime State

Shell owns one Typhon toplevel connection. `TyphonDockRuntimeBackend` adapts
its committed snapshots into neutral Dock runtime records; those snapshots are
the only source of `running`, `active`, and `windowCount` state. Runtime
authority is distinct from action readiness. Launch-helper success is separate
`launching` state and never implies that a window exists.

## Matching and grouping

Dock-owned `DockAppMatcher` resolves a neutral runtime window's app ID in this
order:

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

`DockRuntimeTaskIdentityTracker` assigns one sticky runtime task key per live
window ID and runtime generation. Its initial candidate is:

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

Windows are grouped into one state per task key by the Dock-owned
`DockApplicationStateProjector`. Minimized windows remain running, active is
true when any grouped window is active, and duplicate PIDs remain separate
windows. `windowIds` are ordered by descending focus serial, so the first ID is
the exact activation candidate. `desktopFileName` and
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

When `runtimeKnown && running` is true, a click uses the retained exact window
ID and sends a neutral `Activate` request through `DockRuntimeBackend`, including
for minimized and multi-window applications. `TyphonDockRuntimeBackend`
translates it to the Typhon connection. Otherwise a real desktop launcher is required to launch; a
runtime-only task has no launch fallback. Exact activate/close operations first
validate that the requested `WindowId` still belongs to the task key.
Accepted and no-change results do not launch. Unavailable or failed actions
reconcile the snapshot and never launch a duplicate application on that same
click. Minimize anchors are stored by task key and published to every live
WindowId in the group.

## Minimize anchor publication

The QML dock reports the resting icon rectangle in global compositor coordinates.
The controller accepts a valid rectangle into its task-key cache even while
runtime action readiness is pending. The `setMinimizeAnchor()` boolean reports this
local acceptance only; it does not report transport delivery or compositor
acknowledgment.

When the adapter reports action readiness, the controller replays cached
rectangles to the current exact window handles without waiting for another snapshot. A
successful client send is remembered for idempotence; transport errors remain
eligible for a later runtime projection or capability-ready replay. Reconnects,
handle changes, task migration, and rectangle changes are keyed against the live
connection generation and handle token. Anchor clearing remains scoped to the
exact live window.

For bounded publication diagnostics, launch the Dock with
`ASTREA_TYPHON_ANCHOR_DIAGNOSTICS=1`. Events use stable names such as
`anchor_cached`, `publish_attempted`, `send_queued`,
`deferred_not_authenticated`, `missing_handle`, `send_failed`, and
`anchor_removed`. Output is rate-limited; `send_queued` means the client accepted
the request for transport and is not a compositor acknowledgment. Authentication
credentials are never logged.
