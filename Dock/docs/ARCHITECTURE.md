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

Application icons follow the shared resolution-aware pipeline. Desktop-entry
identity is passed to `AstreaIconProvider`, which uses the active Qt/Freedesktop
theme engine and returns a physical-pixel source selected from the logical
presentation extent and effective device-pixel ratio. `AstreaAppIcon` owns that
source-quality policy; the Dock supplies the configured maximum presentation
scale while `DockAppDelegate` applies the animated magnification as a visual
transform. Hover frames therefore change presentation scale and translation
without changing the icon URL, source size, or provider cache key.

Representation selection remains inside `QIcon::pixmap()`. The provider does
not use `availableSizes()` or implement a second `Scale`, `Type`, `Threshold`,
scalable-range, or inheritance resolver. The shared theme setup presents
`~/.icons` before XDG data and Flatpak export roots in Freedesktop priority
order, while preserving existing Qt paths and deduplicating exact roots. A
narrow Astrea-owned mutex serializes those Qt global theme lookups and updates;
positive and negative cache locks are never held during icon rendering.

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
continuous rather than a per-icon contains-mouse switch. After a structural
model move, the panel defers one geometry refresh so its hover target and
per-icon arrays are rebuilt from the current delegates rather than a parallel
row-identity cache.

The panel derives a fixed transparent surface envelope from the resting Dock
width, the bounded magnification neighborhood, and the maximum magnification,
lift, and drag headroom. Its width and height depend only on structural state
(the materialized row and configuration), never on the current pointer frame.
The bottom-anchored `dockChrome` remains exactly the resting height and is
centered inside that envelope; only its explicit visual width animates from the
resting width to the current magnification width. The Row's resting centers
remain panel-center-relative, so changing chrome width cannot move the output
baseline or invalidate pointer coordinates.

Configured pins use a Qt Quick drag handler with a system-sized threshold. The
dragged delegate is raised, lifted, and scaled; neighboring pinned delegates
stay on the resting vertical baseline while using an ephemeral index preview.
Runtime-only delegates are not draggable. At the threshold, `DockPanel` captures
the source delegate's rendered/transformed center before suspending
magnification, then keeps the drag center relative to the panel center so
centered surface-width animation cannot move the drag origin or target. QML
suspends magnification while reordering and uses the handler's exclusive grab
transitions: normal exclusive ungrab emits one identity-based reorder request,
while canceled exclusive grabs emit no finish request. Active drag updates map
the handler centroid into the stable panel-local coordinate system; the last
valid panel point is restored when Qt resets the centroid during ungrab so hover
remains under the release pointer or becomes inactive when that point is
outside. A click below
the threshold still activates and a drag release never activates. QML never
mutates the model or writes configuration during the drag. Pointer handlers use
a panel-level transparent target whose rectangle follows the transformed icon
exactly; it does not expand the actionable area into unrelated headroom.

The Dock deliberately has three separate geometries. The visual `QQuickWindow`
surface is a fixed maximum envelope containing possible magnification or drag
headroom. The Qt `QWindow` input region is a cached union of the actual mapped
`dockChrome` rectangle and every current transformed delegate interaction
rectangle reported by QML; it excludes empty transparent envelope space and is
refreshed as the transforms animate. The Layer Shell exclusive zone is the
fixed resting reservation. `DockInputRegionPolicy` owns bounded, finite,
normalized integer clipping, while `DockInputRegionBridge` applies the cached
region to the QQuickWindow. QML remains responsible for rendered geometry and
its semantic pointer boundary; C++ does not duplicate the magnification formula.
A pointer ignored by the Dock's semantic boundary is eligible to reach the
underlying application surface only when the compositor's input routing also
places it there; an ignored Dock activation alone is not proof that the
application received the click.

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
exclusive zone equal to the normal resting Dock height. The requested surface
width and height remain constant through pointer entry, magnification, hover
exit, and reorder; neither is used as a transient clearance signal. Maximized
or tiled windows therefore do not move. An empty or disabled Dock is unmapped
and reserves no positive zone.
