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
7. Unresolved.

Titles and PIDs are never used as identity. First-party applications must set
their canonical desktop ID, for example Explorer publishes `astrea-explorer`
for `astrea-explorer.desktop`. Unresolved windows do not enter the Dock
projection.

Resolved windows are grouped into one state per desktop filename. Minimized
windows remain running, active is true when any grouped window is active, and
duplicate PIDs remain separate windows. `windowIds` are ordered by descending
Typhon focus serial, so the first ID is the exact activation candidate.

## Model membership and ordering

The model owns two inputs:

```text
configured pins + resolved running runtime-only applications
```

Pins retain configured order. The projector's `encounterOrder` appends newly
observed runtime-only applications to a model-owned dynamic order. Focus-only
updates change runtime roles but do not reorder existing dynamic rows. A
runtime-only application becomes one pinned row when configured, and a running
pin becomes a dynamic row when unpinned; neither transition duplicates or loses
its runtime state. A runtime-only row is removed after its last live window
closes. A pinned row remains with `runtimeKnown=true` and stopped values.

## Authority and activation

While Typhon is authoritative, a resolved pinned application missing from the
projection is known stopped. On disconnect, degradation, unsupported protocol,
or another authority loss, pins remain visible with neutral unknown values and
runtime-only rows are removed. Stale runtime-only order is never retained.

When `runtimeKnown && running` is true, a click uses the retained exact
`WindowId` and sends Typhon `Activate`, including for minimized and multi-window
applications. Accepted and no-change results do not launch. Unavailable or
failed actions reconcile the snapshot and never launch a duplicate application
on that same click.
