# Typhon Dock Runtime State

M6.1 connects the live Dock process to one `TyphonToplevelConnection`. The
connection is authoritative only after a committed snapshot is available;
while it is starting, disconnected, degraded, unsupported, or stopped, Dock
marks runtime state as unknown.

## Matching And Grouping

`TyphonAppMatcher` resolves a Typhon app ID against the shared desktop catalog
in this order:

1. Exact desktop filename when the app ID ends in `.desktop`.
2. Exact desktop ID.
3. Case-insensitive exact desktop ID.
4. Exact `StartupWMClass`.
5. Case-insensitive `StartupWMClass`.
6. Normalized reverse-DNS desktop ID.
7. Unresolved.

Titles and PIDs are never used as application identity. Visible entries win
over hidden or `NoDisplay` entries, with the lexicographically smallest desktop
filename breaking ties.

Only resolved windows are grouped. Minimized windows remain running, active is
true when any grouped window is active, and duplicate PIDs remain separate
windows. Snapshot order is preserved in each runtime state's window ID list.

## Model Boundary

`DockAppModel::applyRuntimeStates()` sets `runtimeKnown` for resolved items
when the snapshot is authoritative. Missing runtime entries reset `running`,
`active`, and `windowCount` to false/zero when windows close. Unknown runtime
state clears those values without claiming that the application is stopped.
Pins, resolved metadata, launching state, and launch errors are preserved.

When `runtimeKnown && running` is true, Dock suppresses a duplicate launch.
Activation, restore, minimize, and close actions remain outside M6.1 and are
reserved for M7.

No window action, activation request, thumbnail, icon transport, workspace, or
output behavior is part of M6.1.

No real Typhon session qualification has been performed.
