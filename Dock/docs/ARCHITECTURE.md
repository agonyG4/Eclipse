# Astrea Dock Architecture

`astrea-dock` is an independent resident Qt 6 process. `app/` performs
bootstrap and dependency wiring; `core/` owns the stable Dock model and launch
policy; `services/` owns validated configuration; `platform/` owns runtime
paths, IPC, and Layer Shell; `qml/` only presents state and forwards clicks.

`DockAppModel` uses the full desktop filename as its stable row key. It keeps
configured order, retains unresolved pins, and exposes truthful future-facing
running, active, and window-count roles. `DockController` applies config,
coordinates the model, and tracks pending launches independently per pin.
`ApplicationLauncher` is shared with Spotlight and invokes `astrea-launch`
without a shell or GUI-thread blocking.

QML is presentation-only. It does not parse JSON, read files, inspect
processes, launch applications, invoke shell commands, or speak Wayland. The
controller supplies all display data and owns interaction decisions.

Typhon currently does not expose a public window-management protocol. The
Dock therefore does not invent running indicators, active state, minimized
windows, or process-based guesses. The existing model roles are the extension
seam for a future Typhon window integration that can publish stable window
identities and state through a real protocol.

The Dock Layer Shell policy is explicit: scope `astrea-dock`, top layer,
bottom-only anchor, no keyboard interactivity, configured bottom margin, and
exclusive zone equal to the panel surface height. An empty or disabled Dock is
unmapped and reserves no positive zone.
