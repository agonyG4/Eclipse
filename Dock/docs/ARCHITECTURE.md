# Astrea Dock Architecture

The resident Dock is hosted by the unified `astrea-shell` Qt 6 process.
`astrea-dock` is a compatibility IPC client and is not a second resident Dock.
`app/` performs compatibility-client bootstrap; `core/` owns the stable Dock
model and launch/reorder policy; `services/` owns validated configuration and
the narrow pins persistence boundary; `platform/` owns runtime paths, IPC, and
Layer Shell; `qml/` presents state and emits interaction requests.

`DockAppModel` uses the full desktop filename as its stable row key. Its visible
rows are the ordered union of configured pins and resolved applications with
live Typhon toplevels. Configured pins retain their exact configuration order;
runtime-only rows append in first-observed order and do not move when focus
changes. Pinned rows can remain visible while stopped, while a runtime-only row
is removed when its last live window disappears. Structural insert, remove, and
move signals preserve stable QML delegates.

`DockController` applies config, coordinates the model, tracks pending launches
independently per identity, and retains the runtime state needed for
exact-window activation. It is also the only owner of a completed pinned reorder:
it validates the stable `desktopFileName`, asks `DockConfigPersistence` to write
the new pins, and updates the model only after that write succeeds. `pinCount`
describes configured pins, not the total visible row count; `resolvedPinCount`
counts only configured pins that resolve in the desktop catalog.
`ApplicationLauncher` is shared with Spotlight and invokes `astrea-launch`
without a shell or GUI-thread blocking.

QML is presentation-only. It does not parse or write JSON, read files, inspect
processes, launch applications, invoke shell commands, or speak Wayland. The
controller supplies all display data and owns application activation and
persistence decisions.

`DockPanel` keeps a stable resting `Row` and selects one configured hover effect.
`none` leaves resting geometry unchanged. `lift` keeps the lightweight Eclipse
behavior: only the directly hovered delegate scales to about `1.1` and moves
upward. `magnification` performs one linear pass over the materialized
delegates on each pointer update and applies a symmetric raised-cosine (Hann)
scale based on distance from each resting icon center. Prefix sums of the
per-icon extra widths provide visual translations that make room for the
enlarged icons while keeping the strip centered. Magnification icons scale from
their bottom edge; running indicators remain outside that transform. A
panel-level hover handler drives the magnification calculation, so it is
continuous rather than a per-icon contains-mouse switch.

The panel has a stable resting chrome rectangle anchored to its bottom. The
transparent visual surface may temporarily add headroom for magnification or
for the lift/drag bounds at larger icon sizes, but the chrome and Row retain
their resting height. This keeps the visual top inside the surface without
making the Dock background permanently taller. Hover exit and configuration
changes animate all transforms and surface dimensions back to rest.

Configured pins use a Qt Quick drag handler with a system-sized threshold. The
dragged delegate is raised, lifted, and scaled; neighboring pinned delegates
stay on the resting vertical baseline while using an ephemeral index preview.
Runtime-only delegates are not draggable. QML suspends magnification while
reordering and uses the handler's exclusive grab transitions: normal exclusive
ungrab emits one identity-based reorder request, while canceled exclusive grabs
emit no finish request. Drag coordinates are center-relative to the panel, so
centered surface-width animation cannot move the drag origin or target. A click
below the threshold still activates and a drag release never activates. QML
never mutates the model or writes configuration during the drag. Pointer
handlers use a panel-level transparent target whose rectangle follows the
transformed icon exactly; it does not expand the actionable area into unrelated
headroom.

Typhon is the authoritative source for task-relevant toplevels. The projector
matches each published client `app_id` through the immutable desktop catalog,
groups multiple windows into one application state, and retains exact stable
WindowIds ordered by focus serial. Titles, PIDs, process state, and launch
success are never application identity or proof that a window exists. A first-
party application must publish its canonical desktop application ID itself.

When Typhon is authoritative, pinned rows missing from the projection are
known stopped (`runtimeKnown=true`, `running=false`). When authority is lost,
pinned rows become neutral unknown rows and runtime-only rows are removed.

The Dock Layer Shell policy is explicit: scope `astrea-dock`, top layer,
bottom-only anchor, no keyboard interactivity, configured bottom margin, and an
exclusive zone equal to the normal resting Dock height. The visual QQuickWindow
may become taller for magnification, but that height is never used as the
exclusive zone, so maximized or tiled windows do not move. An empty or disabled
Dock is unmapped and reserves no positive zone.
