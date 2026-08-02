# Dock Runtime Flow

```text
startup
  -> runtime paths
  -> config
  -> desktop-entry catalog
  -> model
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

`running`, `active`, and `windowCount` remain false, false, and zero in Stage 1
because Typhon has no public window-management protocol for Eclipse.
