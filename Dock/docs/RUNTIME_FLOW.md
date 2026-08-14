# Dock Runtime Flow

```text
startup
  -> runtime paths
  -> config
  -> desktop-entry catalog
  -> model (configured pins)
  -> one Typhon toplevel connection
  -> authoritative runtime projection (pins + live resolved apps)
  -> QML
  -> Layer Shell mapping
  -> click
  -> exact Typhon activation or shared launcher
  -> Typhon shell-control launch path
```

At startup, `DockRuntimePaths` resolves `ASTREA_ROOT` and the two AstreaOS
configuration paths. `DockConfigWatcher` loads validated defaults or the user
file, while `DesktopEntryCatalog` publishes an immutable XDG-priority
snapshot. The controller gives the model the configured pin order and QML
receives the model through a context property.

The application configures the content-sized QQuickWindow through the required
LayerShellQt integration and updates its exclusive zone when the panel height
changes. Configuration and component toggles are debounced and re-applied
without restarting the process. A Layer Shell setup or mapping failure is
reported as a shell failure; the Dock is never shown as an ordinary Qt window.
A click calls `DockController::launch`. If authoritative Typhon
runtime state identifies a live window, the controller submits one exact
`activate` action for the most recent focus-serial candidate, including
minimized windows. The shared Typhon connection owns authentication, pending
state, completion, and generation cleanup. If no live window is known, the
existing controller and shared supervised launcher path is used. An
unavailable or failed action is reconciled without launching on that same
click.

The Dock owns one Typhon toplevel connection. After the initial snapshot
commits, `DockApplicationStateProjector` emits only resolved live applications
and a deterministic encounter order. `DockAppModel` unions that projection
with configured pins: pins remain first, and new runtime-only applications
append without focus-driven reordering. A missing runtime entry removes a
runtime-only row but leaves a configured pin as a known stopped row.

If the connection is unavailable, each pinned item exposes
`runtimeKnown=false` and neutral runtime booleans; runtime-only rows are
removed. A resolved running item uses exact Typhon activation rather than a
duplicate launch; stale or unavailable targets reconcile state and never fall
through to a same-click launch.
