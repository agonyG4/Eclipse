# Astrea Paper Wallpaper API v1 — Design

**Date:** 2026-08-19  
**Status:** Approved for implementation from the attached authoritative request  
**Scope:** Eclipse native shell, Settings, and the `astreactl` client adapter

## 1. Decision summary

Paper is the single owner of wallpaper configuration, validation, persistence, fallback resolution, and effective-state publication. Eclipse owns rendering. Settings, the native lockscreen when it exists, and `astreactl` are consumers or clients.

The v1 implementation is native C++/Qt in Eclipse. It supports one global static image with typed presentation fit. It has one configured descriptor, one factory default descriptor, and one effective descriptor. A configured source that is missing or unreadable remains configured; the effective source falls back to the factory default. Reset removes the configured override and immediately resolves the factory default.

The implementation will add:

- `Eclipse/Paper` core service, typed descriptors, resolver, persistence adapter, and tests;
- a transparent-input Wayland background surface manager that mirrors the effective source on every current output;
- a native Settings controller/page;
- a secure runtime socket dedicated to Paper wallpaper commands;
- a thin Typhon `astreactl wallpaper` client route that never stores wallpaper state.

The existing AstreaOS Quickshell/Python wallpaper manager remains a legacy compatibility path while the native Eclipse session is brought up. It is not continuously synchronized with the new service, and the new service will not create or repair its symlinks. Eclipse does not currently contain a native lockscreen surface, so this design exposes the same effective-source contract for that future consumer and records the current Quickshell lockscreen as compatibility-only; it does not claim lockscreen migration or native lockscreen qualification.

## 2. Baseline and repository boundaries

The implementation was inventoried before code changes.

| Repository | Baseline | Relevant role | Working-tree constraint |
| --- | --- | --- | --- |
| Eclipse | `35d484decb287220775acf9cd02e52352144780d` (`35d484d`) | Native Qt shell and Settings target | Existing Bar/Bench/Shell/Spotlight edits are preserved; new work is additive except required build/test wiring. |
| Typhon | `0ef9f7b99fa38d0fc04bf5ffa8f494db5a6eade6` (`0ef9f7b`) | Compositor and existing `astreactl` client | Existing compositor/control changes are out of scope; only the CLI client route is extended. |
| AstreaOS | `c70bb69b3c2089ce792b7ca6869109dc8418c22a` (`c70bb69`) | Current Quickshell/Python compatibility implementation | Existing wallpaper manager, QML, autostart, and tests are dirty user work and will not be overwritten. |

No Regulus implementation was found in Eclipse, Typhon, or the inspected AstreaOS source. The persistence boundary therefore remains replaceable and v1 uses the smallest existing-platform-compatible XDG backend rather than inventing a `wallpaper.json` schema or claiming Regulus support.

## 3. Current implementation inventory

The following classification is deliberately about observed code, not intended architecture.

| Area | Observed implementation | Classification | v1 treatment |
| --- | --- | --- | --- |
| AstreaOS `src/Core/bridge/wallpaper/wallpaper_manager.py` | Python CLI manages library scans, user imports, symlink state, thumbnails, blur variants, and `awww` application | CURRENT in the Quickshell session; LEGACY for native Eclipse | Preserve as compatibility during migration; do not make it a second Paper owner. |
| AstreaOS `src/Apps/Settings/pages/paper/Wallpaper.qml` | QML launches Python processes, scans paths, and applies wallpaper | CURRENT in the Quickshell session; LEGACY for native Eclipse | Replace only in the native Settings route; leave dirty compatibility files intact. |
| AstreaOS `src/Apps/Settings/pages/paper/Lockscreen.qml` | Separate lockscreen scope and Python state path | CURRENT/LEGACY compatibility | No continuous sync; future consumer reads Paper effective state. |
| AstreaOS `src/Features/Paper/lockscreen/Lockscreen.qml` | Reads `~/.config/AstreaOS/user/paper/lockscreen/wallpaper.jpg` directly | LEGACY | Do not use as authority or renderer input in Eclipse. |
| AstreaOS `src/Apps/Wallpapers/main.qml` | Quickshell wallpaper application UI around the Python manager | CURRENT/LEGACY compatibility | No native Eclipse dependency. |
| AstreaOS `config/hypr/system/autostart.conf` | Starts `awww` and invokes the Python restore command | CURRENT/LEGACY compatibility | Not a v1 authority or required startup path for Eclipse. |
| AstreaOS `config/AstreaOS/ui/wallpaper.json` | Empty historical configuration file | DEAD/LEGACY | Do not extend it. |
| AstreaOS `Old/Settings` copies | Historical Settings implementations | DEAD | Do not touch. |
| Eclipse | No existing Paper service, wallpaper renderer, or native lockscreen | UNKNOWN/NOT IMPLEMENTED | Add the native v1 boundary. |
| Eclipse `Shell/platform/ipc/ShellIpcServer` | Existing `/tmp/astrea-shell-v1` line transport for shell features | CURRENT for existing shell commands; INSECURE for new wallpaper control | Keep existing endpoint unchanged; add a separate secure Paper endpoint. |
| Typhon `astreactl` | Typhon-only discovery and framed control protocol | CURRENT for compositor commands | Add a client adapter for Paper; do not move ownership into Typhon. |

The current AstreaOS wallpaper state is represented by symlinks such as `~/.config/AstreaOS/user/paper/wallpaper/wallpaper.jpg`. Those links are useful migration evidence, but they are not part of the v1 API and no v1 operation creates one.

## 4. Existing data flow and authority violations

The current compatibility flow is:

```text
Settings QML / Wallpapers QML
        -> Quickshell Process
        -> wallpaper_manager.py
        -> managed user file + symlink state
        -> awww / lockscreen path reads
```

Display preview QML and the lockscreen read paths directly. Autostart invokes a restore command. This makes filesystem layout, symlink targets, Python availability, process spawning, and `awww` state observable authorities. It also gives wallpaper and lockscreen separate scopes without a single effective-state contract.

The v1 flow is:

```text
Settings / astreactl / future lockscreen client
        -> Paper API or secure Paper socket
        -> PaperService
           -> resolver + bounded async validator
           -> persistence adapter
           -> effective snapshot
        -> Eclipse WallpaperSurfaceManager
        -> one background surface per output
```

Only `PaperService` decides configured, default, effective, loading, fallback, and error state. The renderer never reads settings files or symlink paths. Consumers receive a snapshot, not a state path.

## 5. External study and constraints

The design takes the following bounded lessons from established implementations:

- [Hyprpaper](https://github.com/hyprwm/hyprpaper) demonstrates that a wallpaper helper benefits from a small IPC surface, explicit fit modes, and output-aware rendering. Paper adopts the explicit fit vocabulary and future output extension, but keeps state and rendering in Eclipse rather than depending on a separate daemon.
- KDE’s [Plasma image wallpaper package](https://github.com/KDE/plasma-workspace/blob/master/wallpapers/image/imagepackage/contents/ui/main.qml) separates the wallpaper item from configuration and treats image loading as a presentation concern. Paper follows the source/presentation split while keeping configuration and persistence in C++.
- [Plasma Workspace](https://github.com/KDE/plasma-workspace) and [Plasma Desktop](https://github.com/kde/plasma-desktop) show the value of a stable wallpaper item contract even when future modes such as slideshows or plugins are added. Those modes are deliberately not smuggled into v1.

These are design references, not claims that Eclipse currently has equivalent multi-output, slideshow, or lockscreen coverage.

## 6. API model

### 6.1 Typed vocabulary

The core types are Qt value types with `Q_GADGET`/`Q_ENUM` metadata where QML or IPC needs the enum.

```text
WallpaperKind:       Image | Dynamic | Video | Slideshow
WallpaperSourceKind: SystemResource | ExternalFile
WallpaperFit:        Cover | Contain | Stretch | Center | Tile
WallpaperScope:      Global | Output
WallpaperState:      Ready | Loading | Fallback | Error
WallpaperFallback:   None | SourceMissing | SourceUnreadable |
                     UnsupportedKind | UnsupportedScope | InvalidDescriptor |
                     FactoryDefaultUnavailable | EmergencyFallback
```

Only `Image` and `Global` are accepted in v1. The other enum values are deliberate forward-compatible vocabulary, not implemented behavior. Unsupported values fail with a typed error and do not mutate configuration.

### 6.2 Descriptor

`WallpaperDescriptor` carries source identity and presentation separately:

```text
kind:           WallpaperKind
sourceKind:     WallpaperSourceKind
logicalId:      stable API identity, e.g. astrea://wallpaper/default or file id
source:         user/API source string, including file URI or path
resolvedSource: canonical validated local file path, never a symlink API
fit:            WallpaperFit
scope:          WallpaperScope
```

`source` is the configured input. `resolvedSource` is a runtime resolution result and is not persisted as the only identity. A configured external file can be replaced in place without changing its API identity; validation then republishes the effective snapshot.

The service publishes a `WallpaperSnapshot` containing:

```text
configured:        optional WallpaperDescriptor
factoryDefault:    WallpaperDescriptor
effective:         WallpaperDescriptor
state:             WallpaperState
fallback:          WallpaperFallback
generation:        monotonically increasing effective-state generation
lastError:         typed code plus user-safe message, if any
```

The snapshot is the only renderer input. The renderer may use `resolvedSource` for a local `Image`, but may not infer authority from it.

## 7. Configured/default/effective semantics

The three meanings are distinct:

| Meaning | Definition | Persistence |
| --- | --- | --- |
| Configured | The user’s accepted override descriptor, even if its current source is unavailable | Persisted until reset; missing source does not erase it |
| Factory default | Product-provided image chosen by the resolver/resource provider | Never written as the user override |
| Effective | The source currently safe for consumers to render | Derived; default or emergency fallback when configured source cannot render |

Startup loads the configured descriptor, resolves the factory default, and publishes an initial safe snapshot. If configured validation is pending, the service can publish the factory default as effective with `Loading` metadata, then publish the configured image only after successful validation. This prevents the renderer from receiving an empty or invalid source.

`set` is transactional: validate the requested descriptor, and only after successful validation write it to persistence and publish it as configured/effective. A failed set leaves the previous configured and effective values intact, while exposing a typed error.

If a persisted configured source is missing at startup, configured remains present, effective is the factory default, state is `Fallback`, and the fallback reason is `SourceMissing` or `SourceUnreadable`. A later explicit reload or file watcher event can recover it. `reset` removes the configured override from persistence and resolves the factory default; it does not delete user files.

`generation` changes only when the effective descriptor changes. Loading/error metadata changes use their own signals and do not manufacture a new image generation.

## 8. Resolution and validation

The resolver is the single boundary for path and descriptor validation.

Accepted v1 inputs:

- absolute local paths;
- `file://` URIs, decoded with Qt URL handling;
- `~` at the beginning of a path, expanded by the API boundary;
- Unicode and whitespace in path components;
- symlinked files as ordinary filesystem inputs, provided the target resolves to a readable regular image file.

Relative paths are resolved against the caller’s documented process working directory only at the API boundary and are then represented by the canonical resolved file. The service does not persist a process-relative path as authority.

The resolver rejects directories, missing paths, unreadable paths, unsupported image formats, non-image kinds, non-global scopes, malformed URIs, and paths that cannot be canonicalized safely. Image decoding uses `QImageReader` in the validation worker. A successful decode is required before persistence. File names are never passed to a shell.

Factory default lookup is a resource-provider concern. It tries the packaged product default in the configured Astrea resource locations, then the Eclipse resource abstraction, then a small embedded emergency image. The emergency asset is not treated as normal factory artwork. If product packaging does not expose a stable factory artwork path, the result is reported as `FactoryDefaultUnavailable` with `EmergencyFallback`, rather than being silently labeled as a qualified factory default.

## 9. Persistence

`WallpaperPersistence` is an interface with a file-backed v1 adapter. The adapter writes a small INI-style group under the user XDG configuration directory, for example:

```text
$XDG_CONFIG_HOME/AstreaOS/paper.ini
[wallpaper]
source=...
kind=image
fit=cover
scope=global
```

The exact path is owned by the adapter and covered by tests; no public consumer depends on it. Writes use a same-directory temporary file, restrictive permissions, flush/close, and atomic rename. The persisted record contains the configured source and typed presentation fields, not a symlink state path and not a transient resolved path.

Regulus is not available in the inspected repositories, so v1 does not claim Regulus persistence. The interface is intentionally narrow so a real Regulus adapter can replace the file adapter without changing Paper semantics. A future adapter must preserve transactional set, reset, and configured-source retention semantics.

An optional one-time compatibility migration may read the known AstreaOS legacy wallpaper link only when the new Paper record is absent. It resolves the link target, validates it through the same resolver, persists the external source through Paper, and never creates or repairs a link. Stale or ambiguous legacy state is ignored with a diagnostic. Migration is idempotent and is not continuous synchronization.

## 10. Async validation and concurrency

Validation and image decode run away from the UI thread. The worker has one active request and one replaceable pending request. A new `set` supersedes a pending request; if an active request is in progress, its result is discarded when its request token is no longer current. This gives latest-request-wins behavior without queueing an unbounded number of jobs.

Every completion carries a monotonically increasing request token. The service checks the token and descriptor identity on the owning thread before committing persistence or effective state. A stale success cannot overwrite a newer selection; a stale failure cannot replace a newer effective image.

The service owns shutdown ordering: stop accepting requests, stop the worker, disconnect queued completions, and release renderer references after the service has stopped publishing. Tests cover rapid sequences such as `A -> B -> C`, stale failure after success, reset during validation, and destruction during an active decode.

## 11. Eclipse renderer

`WallpaperSurfaceManager` creates one `QQuickWindow` per current `QScreen` using the existing LayerShellQt background-layer pattern. Each surface is:

- anchored to all edges;
- configured on the Background layer;
- transparent to pointer and keyboard input;
- assigned to the corresponding screen;
- not an exclusive-zone consumer;
- driven only by the `effectiveWallpaper` snapshot.

The QML surface contains a background color and two asynchronous image slots. A new image loads into the inactive slot. The active image remains visible until the new image reaches `Ready`; only then does the surface swap. A load failure leaves the previous renderable image or the emergency image visible. There is no blank frame requirement and no filesystem polling in QML.

The manager mirrors the global effective descriptor across all current outputs. Output-specific selection is not implemented in v1. Screen add/remove and geometry changes are handled using the existing surface-manager lifecycle patterns. The manager does not inspect or write Paper persistence.

## 12. Settings consumer

Settings receives a native `WallpaperController`/service client through `SettingsController` context properties. C++ owns the service client, validation result, persistence calls, and status model. QML owns layout, interaction, and visual presentation.

The page exposes:

- current effective preview;
- configured-source status, including fallback explanation;
- file selection through Qt’s presentation API;
- typed fit selection;
- apply/set, reset, and retry/reload actions;
- user-safe error text.

QML will not import `Quickshell.Io` for wallpaper, spawn Python, invoke `zenity`, inspect symlink paths, read `wallpaper.json`, or call a shell command. The page is added to the existing Appearance/Settings navigation without changing unrelated visual routes. Unit and offscreen QML tests cover route registration, context properties, set/reset wiring, and fallback status.

## 13. Lockscreen contract

The Paper API’s `effectiveWallpaper` and generation are suitable for a lockscreen consumer. The native lockscreen surface is not currently present in Eclipse, so the implementation adds no speculative lockscreen window or authentication behavior. The current AstreaOS Quickshell lockscreen remains `LEGACY/COMPATIBILITY` and continues to read its old path only while that session is active.

The handoff contract for a future native lockscreen is explicit: subscribe to the Paper snapshot, use the effective descriptor, and keep any lockscreen-specific blur/overlay presentation in the lockscreen consumer. It must not read or write Paper persistence and must not introduce a second wallpaper source.

## 14. Runtime control API

Paper exposes a separate local endpoint under the user runtime directory, for example:

```text
$XDG_RUNTIME_DIR/astrea-shell/wallpaper.sock
```

The parent directory is created with mode `0700`; the socket is owned by the current user and is not placed in world-writable `/tmp`. The existing `/tmp/astrea-shell-v1` endpoint remains for current shell features and is not reused for this security-sensitive API.

The transport is line-oriented with a JSON argument, bounded to the existing command size limit:

```text
wallpaper get {}
wallpaper set {"source":"file:///home/me/Pictures/a b.png","kind":"image","fit":"cover","scope":"global"}
wallpaper reset {}
wallpaper default {}
```

The server accepts only the four Paper actions, validates JSON schema and enum values, and returns a bounded JSON snapshot/error. It does not evaluate shell syntax, interpolate environment fragments, or accept arbitrary feature dispatch. `set` returns an accepted/loading snapshot immediately; the authoritative result is observable through a subsequent `get` and the Settings/service signals.

The endpoint is a native control route, not a new Typhon protocol and not a second state owner. The existing Eclipse shell runtime owns its lifecycle and injects the Paper service into QML.

## 15. `astreactl` adapter

Typhon’s CLI adds:

```text
astreactl wallpaper get
astreactl wallpaper set <source> [--fit <cover|contain|stretch|center|tile>]
astreactl wallpaper reset
astreactl wallpaper default
```

The command connects directly to the secure Paper socket, sends one JSON request, validates the response size, and prints human or JSON output using the existing CLI conventions. `--instance` and `--socket` remain Typhon-only options and are rejected for wallpaper commands unless a future explicit Paper endpoint option is added. The CLI preserves spaces and Unicode by treating the source as one argument and encoding it as JSON. It does not discover or connect to the Typhon compositor socket for wallpaper, and it does not persist local state.

## 16. Migration and compatibility

Migration is phased:

1. Native Eclipse Paper becomes authoritative for native Eclipse consumers.
2. A one-time legacy import may adopt a valid old AstreaOS target into Paper’s configured source.
3. Existing Quickshell/AstreaOS sessions remain compatibility-only until their shell is replaced.
4. Continuous two-way synchronization, symlink repair, and automatic deletion of legacy files are explicitly out of scope.

The implementation will not modify the dirty AstreaOS wallpaper manager, Settings QML, autostart, or lockscreen files. The final report will identify those files and the session boundary so deployment does not mistake the compatibility path for a completed native migration.

## 17. Error and observability contract

Errors are typed and safe for UI/CLI display:

```text
InvalidDescriptor
UnsupportedKind
UnsupportedScope
SourceMissing
SourceUnreadable
UnsupportedImage
PersistenceReadFailed
PersistenceWriteFailed
FactoryDefaultUnavailable
ControlProtocolError
ControlPermissionDenied
StaleRequest
```

The service exposes state, fallback reason, generation, and last error. It emits changes for configured, effective, state, error, and generation. Logs include operation and error code but do not include uncontrolled shell text. Sensitive local paths are not echoed into user-facing messages unless the caller explicitly requested the descriptor.

## 18. Test matrix and acceptance evidence

Unit tests cover descriptor JSON/value conversion, fit/kind/scope validation, path expansion, Unicode/space paths, directories, missing/unreadable files, unsupported images, factory fallback, persistence round-trip, atomic-write failure, configured-source retention, reset, migration idempotence, and generation semantics.

Concurrency tests cover latest-request-wins, stale completion rejection, reset during validation, no unbounded queued work, and shutdown during decode.

Renderer tests cover effective-only input, no blank swap on failed load, screen add/remove, transparent input, background layer, and global replication. Settings tests cover native context wiring and the absence of process/filesystem access in the wallpaper page. Control tests cover secure runtime path/permissions, all four actions, malformed/oversized JSON, injection-shaped source strings, Unicode, spaces, unknown actions, and bounded responses. Typhon tests cover routing without Typhon-state mutation and human/JSON output.

The final report must distinguish tests run successfully from tests unavailable due missing display/session/dependencies. It must not claim multi-output qualification beyond the exercised current-screen manager, native lockscreen qualification, Regulus integration, or product factory-artwork qualification unless those are actually verified.

## 19. Rejected alternatives

- **Keep Python as the owner:** rejected because it leaves authority in a process/filesystem/symlink path and cannot provide the native service contract required by Eclipse.
- **Make Typhon own wallpaper:** rejected because Typhon is the compositor/control plane, while Paper is the product feature and Eclipse owns shell rendering.
- **Reuse `/tmp/astrea-shell-v1`:** rejected for new wallpaper control because its existing location/mode is not a suitable secure endpoint; changing it could also break existing clients.
- **Add a new public `wallpaper.json`:** rejected because no requirement or current abstraction makes it necessary and it would create another schema owner.
- **Use QML `Process`/shell commands:** rejected because it violates the native service boundary and creates injection, quoting, and lifecycle problems.
- **Persist only the resolved path:** rejected because it loses configured intent when a source is temporarily missing and makes replacement/relocation semantics opaque.
- **Implement dynamic/video/slideshow now:** rejected because v1 needs a narrow, testable Image contract; the typed vocabulary leaves a safe extension point.
- **Claim a native lockscreen implementation:** rejected because Eclipse currently has no such surface; the truthful v1 boundary is a consumer contract and explicit compatibility status.

## 20. Implementation handoff

The implementation plan at `docs/superpowers/plans/2026-08-19-paper-wallpaper-api-v1.md` breaks this design into test-first tasks. Each task names exact files, a failing test, minimal implementation, and verification boundary. Existing dirty work is preserved throughout.
