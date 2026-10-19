# Download progress API

`astrea-downloads` is a headless, local API for producers to publish download
progress. This first version does not capture browser downloads automatically:
Zen/Brave extensions or another producer must send snapshots. It neither starts
downloads nor opens received filenames.

## Build

Reuse the Eclipse build tree:

```sh
realpath /mnt/Aether/Desktop/GitHub/Eclipse-build
export TMPDIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/tmp
export CARGO_TARGET_DIR=/mnt/Aether/Desktop/GitHub/Eclipse-build/cargo-target
cmake -S /home/agony/GitHub/Eclipse -B /mnt/Aether/Desktop/GitHub/Eclipse-build
cmake --build /mnt/Aether/Desktop/GitHub/Eclipse-build --target astrea-downloads download-progress-test
ctest --test-dir /mnt/Aether/Desktop/GitHub/Eclipse-build -R '^download-' --output-on-failure
```

## Terminal usage

Use the installed `astrea-downloads` or
`/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/astrea-downloads`.
Start the service, then run the other commands in another terminal:

```sh
astrea-downloads serve
astrea-downloads watch
```

Publish a complete snapshot (this example simulates a download at 62%):

```sh
astrea-downloads update '{"source":"brave:default","id":"123","filename":"arquivo.zip","state":"downloading","receivedBytes":62,"totalBytes":100}'
astrea-downloads list
astrea-downloads update '{"source":"brave:default","id":"123","filename":"arquivo.zip","state":"completed","receivedBytes":100,"totalBytes":100}'
astrea-downloads remove brave:default 123
```

The commands print one compact JSON object per line. `watch` prints an initial
snapshot, followed by `updated` and `removed` events, until interrupted or the
service disconnects. One-shot commands exit 0 on success and 1 on errors.
`--socket /absolute/path/api.sock` overrides the endpoint for every command.
The default is `astrea-downloads.sock` in Qt's user runtime directory.
Only the service user can access the socket. Stop the service with Ctrl+C;
the next startup can recover the stale socket and companion lock.

## Protocol version 1

Connect to the Unix socket and send UTF-8 JSON objects separated by newlines:

```json
{"version":1,"op":"update","download":{"source":"zen:default","id":"123","filename":"arquivo.zip","state":"downloading","receivedBytes":62,"totalBytes":100}}
{"version":1,"op":"list"}
{"version":1,"op":"watch"}
{"version":1,"op":"remove","source":"zen:default","id":"123"}
```

All replies and events include `version: 1` and `ok`. An update response contains
`download`, a list response contains `downloads`, and a watch starts with
`event: "snapshot"` and `downloads`. Update events contain `event: "updated"`
and `download`; removal events contain `event: "removed"`, `source`, and `id`.
Errors contain `ok: false`, `error` (machine-readable), and `message`.
Operation replies precede any resulting event on the same connection.

Each update supplies all six fields. Unknown input fields are discarded, including
any producer-supplied `progress`. The service computes a fraction in [0,1] from
the byte counts; `totalBytes: null` or `0` produces `progress: null`. Only the
explicit `completed` state forces `progress: 1`. Reaching the total does not
change the producer's state. Supported states are `downloading`, `paused`,
`completed`, `cancelled`, and `failed`. Browser adapters must translate their
native states and unknown sizes to this contract.

Byte counts must be integers between 0 and 9007199254740991. `source`, `id`, and
`filename` must be nonblank strings with no NUL characters and at most 128, 256,
and 4096 UTF-16 code units respectively. Use a separate `source` for each browser
profile/session to avoid ID collisions, e.g. `zen:work` and `brave:personal`.

The service keeps snapshots in memory until removal or restart, including terminal
states. Producer disconnection does not imply completion or failure. Consumers
should treat these as last reported states. A restarted producer should reconcile
its records; this release has no automatic stale-record detection or persistent
history. Reconnect a watcher to obtain the current snapshot.

Input frames are limited to 64 KiB excluding the newline, concurrent clients to
64, and retained metadata to 4096 records or 1 MiB of compact JSON (whichever is
reached first). At capacity, existing records can still be replaced if the result
fits; `remove` frees capacity. Slow clients are disconnected when their pending
output would exceed 2 MiB. A list/snapshot can exceed 64 KiB but fits that output
bound. Validation failures preserve the previous snapshot and emit no update.
