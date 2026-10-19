# Zen native Wayland download prototype

## Approved direction

The user requested a usable prototype after the successful isolated AutoConfig
experiment. Reuse that mechanism and the existing headless API. No extension,
XWayland, normal-profile modification or visual dock component is required.

## Design

- `Downloads/zen/ZenDownloadPublisher.sys.mjs` observes public downloads, maps
  filename/bytes/state and sends acknowledged JSON Lines over a Unix socket.
  Keep at most 128 records, coalesce changes, refresh every second and replay
  retained snapshots every five seconds. Skip stopped history on initial attach.
  Keep IDs stable within one browser session; launcher supplies a unique source.
- `Downloads/zen/astrea-downloads.cfg` loads the module at normal startup through
  AutoConfig. Only the AutoConfig script sandbox is disabled in the isolated copy.
- `Downloads/tools/zen-downloads-prototype.py` provides prepare/run/list/watch.
  Copy the installed Zen runtime into the stable Aether Eclipse output tree;
  preserve an isolated reusable profile and save files there. Protect preparation
  and launch with one process lock; refuse foreign output directories and links.
  Start the existing API when absent; restart an owned API after failure. Stop
  owned processes when the browser exits. Never stop a service reused from outside.
- All mutable output belongs under `/mnt/Aether/Desktop/GitHub/Eclipse-build`.
  Native Wayland is mandatory: set GDK_BACKEND and MOZ_ENABLE_WAYLAND and remove
  DISPLAY. No debugger is enabled. No global desktop variables are changed.

## Alternatives considered

A browser source patch would provide a more durable integration but requires
maintaining a browser build. Reusing the extension bridge would be simpler to
package but does not meet this prototype's extension-free requirement. AutoConfig
reuses the mechanism already demonstrated with the installed Zen version.

## Acceptance and limitations

Observe a real ZIP download through the existing API, including filename,
intermediate progress and completion. Unknown totals remain unknown. Private
windows are excluded. Verify API restart replay and bounded/coalesced state.
Use the existing research probe with the new publisher and launcher, plus focused
checks of the record mapping and launcher preparation. No browser rebuild needed.
This uses internal Gecko APIs; persistent cross-session IDs, distribution packaging
and a dock UI remain out of scope. State classification reflects Gecko's partial
file semantics; test pause/cancel separately before promising exact UX semantics.
