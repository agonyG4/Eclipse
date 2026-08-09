# Dock Runtime Flow

```text
startup
  -> runtime paths
  -> config
  -> desktop-entry catalog
  -> model
  -> one Typhon toplevel connection
  -> authoritative runtime projection
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

The application configures the content-sized QQuickWindow through LayerShellQt
and updates its exclusive zone when the panel height changes. Configuration
and component toggles are debounced and re-applied without restarting the
process. A click calls `DockController::launch`. If authoritative Typhon
runtime state identifies a live window, the controller submits one exact
`activate` action for the most recent focus-serial candidate, including
minimized windows. The shared Typhon connection owns authentication, pending
state, completion, and generation cleanup. If no live window is known, the
existing controller and shared supervised launcher path is used. An
unavailable or failed action is reconciled without launching on that same
click.

The Dock owns one Typhon toplevel connection. After the initial snapshot
commits, it projects `running`, `active`, and `windowCount` through the
desktop catalog matcher. If the connection is unavailable, each item exposes
`runtimeKnown=false` and the runtime booleans stay neutral. A resolved running
item uses exact Typhon activation rather than a duplicate launch; unresolved
or non-live targets do not fall through to a same-click launch.
