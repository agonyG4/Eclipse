# Astrea Dock Architecture

`astrea-dock` is an independent resident Qt 6 process. `app/` performs
bootstrap and dependency wiring; `core/` owns the stable Dock model and launch
policy; `services/` owns validated configuration; `platform/` owns runtime
paths, IPC, and Layer Shell; `qml/` only presents state and forwards clicks.

`DockAppModel` uses the full desktop filename as its stable row key. Its visible
rows are the ordered union of configured pins and resolved applications with
live Typhon toplevels. Configured pins retain their exact configuration order;
runtime-only rows append in first-observed order and do not move when focus
changes. Pinned rows can remain visible while stopped, while a runtime-only row
is removed when its last live window disappears. Structural insert, remove, and
move signals preserve stable QML delegates.

`DockController` applies config, coordinates the model, tracks pending launches
independently per row, and retains the runtime state needed for exact-window
activation. `pinCount` describes configured pins, not the total visible row
count; `resolvedPinCount` counts only configured pins that resolve in the
desktop catalog.
`ApplicationLauncher` is shared with Spotlight and invokes `astrea-launch`
without a shell or GUI-thread blocking.

QML is presentation-only. It does not parse JSON, read files, inspect
processes, launch applications, invoke shell commands, or speak Wayland. The
controller supplies all display data and owns interaction decisions.

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
bottom-only anchor, no keyboard interactivity, configured bottom margin, and
exclusive zone equal to the panel surface height. An empty or disabled Dock is
unmapped and reserves no positive zone.
