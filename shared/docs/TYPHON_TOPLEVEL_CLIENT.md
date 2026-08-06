# Typhon Toplevel Client

Eclipse M6 defines a shared, read-only client boundary for Astrea Toplevel
Management v1. The client consumes the private `astrea_toplevel_manager_v1`
and `astrea_toplevel_v1` version-1 contract once the finalized Typhon XML is
available. The current foundation deliberately does not copy or generate from
an uncommitted Typhon worktree.

## Ownership

`TyphonToplevelConnection` is the single owner of a client connection per
Eclipse process. AltTab and future Dock integration consume immutable typed
snapshots. They never own generated Wayland objects and never route events
through `astreactl`.

All protocol work belongs on the Qt main thread. The adapter boundary is typed
and contains no generated protocol pointer in its public API. The finalized
adapter will own the dedicated Wayland display, registry, manager, handles and
Qt notifier integration.

## Revision Boundary

Handle metadata is accumulated as pending state. A manager `done` event is the
transaction boundary. Eclipse publishes exactly one `Snapshot` after all live
changed handles have matching revisions. No initial windows are visible before
the first manager `done`, and a revision may span multiple event-loop turns.

Snapshots preserve the decimal Typhon `WindowId` string, manager revision,
reported total, truncation, and connection generation. IDs are never converted
through floating point or used as PIDs.

Committed windows sort by descending focus serial, active state for ties, and
numeric unsigned Typhon ID. Unknown state bits are retained in the raw state
field for forward-compatible consumers.

## Failure And Reconnect

Contradictory event order, duplicate live identifiers, stale revisions, and
incomplete transactions degrade the connection without terminating Eclipse.
The public snapshot is cleared after consumers are notified. Display loss and
terminal manager failure follow the same deterministic cleanup path.

Reconnect uses bounded delays of 250 ms, 500 ms, 1 s, 2 s, and 5 s. The delay
resets after a complete initial manager revision. New connection generations
invalidate old callbacks and snapshots. Stopping the connection cancels
reconnect and destroys all adapter-owned resources.

The protocol scope is read-only in M6. Activation, minimize, restore, close,
maximize, fullscreen, thumbnails, workspaces, outputs, and control-socket
subscriptions are intentionally absent. M7 will add mutable actions after a
separate protocol contract is finalized.

## Build

`ASTREA_ENABLE_TYPHON_BACKEND` defaults to `ON` when `wayland-client` and
`wayland-scanner` development files are available, and otherwise defaults to
`OFF`. The protocol-independent model and tests remain buildable either way.
Generated bindings are build-tree artifacts and are not committed.

No real Typhon session qualification has been performed by this milestone.
