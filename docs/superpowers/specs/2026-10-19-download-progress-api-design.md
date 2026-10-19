# Local download progress API

Implement the approved API-only first step: a headless Eclipse service that
receives download snapshots and exposes them to terminal clients and future UI.
Browser extensions and dock rendering are subsequent integrations. Nothing is
automatically captured from Zen or Brave in this step.

## Contract

- `astrea-downloads serve` runs a Qt Core/Network local socket service.
- JSON Lines protocol version 1: `update`, `list`, `watch`, `remove`.
- Each update is a complete snapshot with string `source`, `id`, `filename`,
  `state`, integer `receivedBytes`, and integer or null `totalBytes`.
- Identity is `(source, id)` so browser/profile producers can use distinct sources.
- States: `downloading`, `paused`, `completed`, `cancelled`, `failed`.
- Output adds `progress`: fraction clamped to [0,1], null for unknown/zero total,
  and 1 for completed downloads. Completion is explicit, never inferred from bytes.
- Bytes are nonnegative JSON-safe integers (at most 9007199254740991).
- A watch starts with a snapshot, then ordered update/removal events. All mutations
  are serialized in the service thread. Invalid input leaves state unchanged.
- Removal explicitly dismisses retained records. State is in memory and resets
  when the service stops. At most 4096 records and 1 MiB of serialized metadata are
  retained; updates that fit remain possible at capacity. No automatic expiry hides
  active downloads.

## Transport and command line

Use a user-only Unix socket under Qt's runtime directory, with `--socket PATH`
for explicit endpoints. A lock prevents a second instance from replacing a live
endpoint and permits recovery after a crashed instance. No TCP listener.
Bound frames (64 KiB), clients (64), and slow-client output (2 MiB), including space
for an initial full snapshot.
The client prints compact JSON Lines. `update JSON`, `list`, `watch`, and
`remove SOURCE ID` support shell-driven producers and terminal inspection.
No filesystem action is performed on received filenames.

## Validation

Test progress, unknown totals, explicit states, invalid snapshots, independent
sources, removal and capacity. Exercise real local sockets for initial watch
snapshots, subsequent updates, malformed frames, duplicate instances and CLI
errors. All build, test, temporary and generated output goes under the stable
`/mnt/Aether/Desktop/GitHub/Eclipse-build` directory. Commit only this feature's
files on main; preserve unrelated work.
