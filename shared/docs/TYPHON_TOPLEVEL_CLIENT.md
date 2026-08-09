# Typhon Toplevel Client

Eclipse M7-C extends the shared client boundary for the Typhon M7-B
Astrea Toplevel Management v2 contract. The checked-in XML is copied
byte-for-byte from Typhon commit
`211dfe835d1d6d6faf449e7a0239d6f099945e6`; its SHA-256 is
`0dd3449fda60b1ed183e330e1589093f3d4f8086be117d9ca4baa81bd6bd47e7`.
Version 1 remains read-only and byte/semantic compatible.

## Ownership

`TyphonToplevelConnection` is the single owner of a client connection per
Eclipse process. AltTab and Dock consume immutable typed snapshots and the
same typed action API. They never own generated Wayland objects and never
route events through `astreactl`.

All protocol work belongs on the Qt main thread. The adapter boundary is typed
and contains no generated protocol pointer in its public API. The generated
adapter owns the dedicated Wayland display, registry, manager, handles and Qt
notifier integration. Generated client C bindings are produced in the build
tree by `wayland-scanner` and are private to `astrea-shared-typhon`.

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
invalidate old callbacks, snapshots, and pending actions. Stopping the
connection cancels reconnect, clears pending actions, and destroys all
adapter-owned resources.

## Actions

The generated adapter authenticates the same native `wl_display` before
binding action-capable manager resources. It binds `min(advertised_version, 2)`
and publishes a typed capability state: disconnected, read-only v1,
read-only unauthenticated v2, authenticating v2, action-ready v2, or degraded.
No v2 request is sent unless the manager/resource version and same-connection
authorization checks have succeeded.

`requestAction(WindowId, action, consumerToken)` resolves the exact live
published window ID, reserves manager-owned action state, sends exactly one
generated `activate`, `minimize`, `restore`, or graceful `close` primitive,
and waits for manager-owned `action_done`. The pending table is bounded at 64
entries per connection generation; completed tokens are immediately reusable
and no completed-token history is retained. Unknown, duplicate, stale, or
post-disconnect completions cannot settle another request.

The wire result contract is only `accepted`, `no_change`, and `unavailable`.
`unavailable` means the authorized request was valid at the negotiated
protocol version but the exact target/action could not be carried out (for
example a target that ceased to be actionable, a duplicate pending token, or
the pending bound being full); it does not create pending state. Local client
errors such as unsupported protocol, missing authorization, disconnected
transport, or a stale snapshot target remain typed separately and do not emit
`action_done`. A close result acknowledges issuance of the graceful close
request; it is independent of the later `closed` lifecycle event.

AltTab submits only the selected exact `WindowId` through this API. Dock uses
the existing catalog-aware runtime projection and submits the most recent
focus-serial exact window, including minimized windows. Neither consumer adds
focus, raise, restore, minimize, stacking, XWM, or duplicate-launch policy.

M7-C Native remains `DEFERRED`; real-session qualification is a later gate.

## Build

`ASTREA_ENABLE_TYPHON_BACKEND` defaults to `ON` when `wayland-client` and
`wayland-scanner` development files are available, and otherwise defaults to
`OFF`. The protocol-independent model and tests remain buildable either way.
Generated bindings are build-tree artifacts and are not committed. The fake
Wayland integration fixture is additionally built when `wayland-server` is
available and is explicitly skipped otherwise.

No real Typhon session qualification has been performed by this milestone.
