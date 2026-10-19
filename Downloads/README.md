# Download progress API

`astrea-downloads` is a headless, local API for producers to publish download
progress. The included Zen/Brave extensions capture real downloads after activation
in the browser. The API itself neither starts downloads nor opens received filenames.

## Enable real Zen and Brave downloads

The browser bridge is built alongside `astrea-downloads`. Register the native host:

```sh
python3 /home/agony/GitHub/Eclipse/Downloads/tools/install-browser-bridge.py
```

Then activate the extensions in each browser:

- **Brave:** open `brave://extensions`, enable Developer mode, click **Load unpacked**
  and select `/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/browser/brave`.
- **Zen:** open `about:debugging#/runtime/this-firefox`, click **Load Temporary Add-on**
  and select `/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/browser/zen/manifest.json`.
  This local development add-on is removed on browser restart. A permanent release
  requires Mozilla signing; no signature or browser-security preferences are changed.

Once loaded, the bridge starts the local API automatically. Start a normal download
and observe it in another terminal:

```sh
/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/astrea-downloads watch
```

Progress arrives approximately once per second while downloading. Creation,
completion, pause, cancellation, failure and erasure trigger a fresh query. Downloads
from private windows are skipped. Each browser profile gets a persistent random
source identifier such as `zen:<uuid>`; URLs, cookies, referrers and page contents
are not forwarded. The published filename is the local absolute target path.

Only current downloads and up to 256 tracked IDs are synchronized, rather than the
entire download history. Recent terminal states are replayed when the extension or
API reconnects; older terminal records are dismissed when the tracking window fills.
The bridge polls only while downloads are active (including paused downloads), or
while recovering from a connection error. With no active downloads, a restarted API
is synchronized on the next browser event. API state itself remains in memory.

The host is `org.astrea.downloads`. The installer writes only its user-specific
registration under `~/.mozilla/native-messaging-hosts` and Brave's
`NativeMessagingHosts` directory under `XDG_CONFIG_HOME` (default `~/.config`).
It preserves other native hosts and does not enable browser extensions. Registration
points to the Aether build executable, so keep that build tree available.
Use `--uninstall` to remove the registrations; disable/remove the extensions through
the browser to stop capture. This setup supports the native Linux packages used
here; Flatpak/Snap require additional sandbox integration.

For custom installations use `--build-dir /absolute/path/to/Downloads`. The native
host accepts `ASTREA_DOWNLOADS_SOCKET` and `ASTREA_DOWNLOADS_SERVICE` environment
overrides; by default it uses the user runtime socket and its sibling API executable.
The native protocol accepts `ping`, `update` and `remove`, using 32-bit native-endian
length-prefixed UTF-8 JSON with a 64 KiB input limit. The local API additionally
accepts `ping` as a small health check; the bridge is also compatible with a running
first-release API that predates that operation.

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

## Local socket protocol version 1

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
