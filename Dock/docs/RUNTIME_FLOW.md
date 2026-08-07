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
  -> shared launcher
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
process. A click calls `DockController::launch`, which passes the selected
desktop entry to the shared supervised launcher. The launcher invokes
`astrea-launch`; Typhon's shell-control service owns supervised application
startup.

The Dock owns one read-only Typhon toplevel connection. After the initial
snapshot commits, it projects `running`, `active`, and `windowCount` through the
desktop catalog matcher. If the connection is unavailable, each item exposes
`runtimeKnown=false` and the runtime booleans stay neutral. A resolved running
item is not launched again; M7 will add mutable window actions.
