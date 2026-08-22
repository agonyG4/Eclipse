# M8-C.1 StatusNotifier Protocol Correctness and Interoperability Design

## Scope

M8-C.1 hardens the existing native `StatusNotifierService` without changing
the global-service/per-output-presentation boundary. It covers protocol name
validation, watcher and host ownership, real item wire decoding, action
semantics, shared output coordinates, live DBusMenu behavior, and isolated
session-bus integration tests. It does not add M8-D, Control Center,
Notification History, XEmbed, or workspace behavior.

## Architecture

`ShellRuntime` continues to own one `StatusNotifierService`. The service owns
the watcher bridge, item proxies, item model, icon store/provider, and DBusMenu
clients. `StatusNotifierWatcherBridge` is the sole authority for watcher
selection, host ownership, registration lifetime, and generation invalidation.
The local watcher adaptor exposes both supported protocol aliases from one
authoritative registry of item and host records.

The item wire boundary uses registered Qt D-Bus structures for `a(iiay)`
pixmaps and the four-field `ToolTip` structure. Item actions remain asynchronous
and are routed through the proxy; `ContextMenu` is added beside Activate,
SecondaryActivate, and Scroll. All incoming payloads remain bounded.

DBusMenu keeps a typed node tree keyed by stable node IDs. The client subscribes
to live layout/property signals, applies root or subtree replacements without
promoting a subtree to the root, rejects stale revisions, and invokes
`AboutToShow` before root/submenu presentation. Auxiliary menu icons are kept in
the existing in-memory icon provider. QML presents visible rows, separators,
toggle states, icon sources, and cascading child cards using the existing
PopupCard/MenuItem/MenuSeparator primitives.

Tray interaction coordinates are produced by one `BarLayoutMetrics::trayAnchor`
helper. The helper returns both output-local popup coordinates and screen/global
action coordinates from output origin, status-surface origin, and delegate
center. The same result feeds Activate-family actions, popup anchoring, and
tooltip anchoring on every output.

## Verification boundary

Pure tests cover validators, wire parsers, bounds, revisions, reconciliation,
and geometry. A dedicated test-only fake StatusNotifierItem and fake DBusMenu
export real D-Bus objects on the test process's isolated session bus. The
fixture exercises path-only registration, host ownership, ToolTip, all three
pixmap properties, item signals/actions, menu layout/property/update signals,
AboutToShow, Event, owner removal, and start-stop-start behavior. Production
code uses Qt D-Bus only; no helper process, shell command, or alternate tray
backend is introduced.
