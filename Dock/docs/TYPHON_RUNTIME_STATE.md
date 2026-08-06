# Typhon Dock Runtime State

M6 provides a reusable projection from one immutable Typhon snapshot to Dock
application runtime state. The live Dock process is not connected to a second
Typhon source yet.

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

`DockAppModel::applyRuntimeStates()` updates only `running`, `active`, and
`windowCount` for existing items. Missing runtime entries reset those fields to
zero and false when windows close. Pins, resolved metadata, launching state,
launch errors, and click behavior are preserved.

No window action, activation request, thumbnail, icon transport, workspace, or
output behavior is part of M6. The shared connection is suitable for later Dock
use, but M6 does not create a second live Dock connection.

No real Typhon session qualification has been performed.
