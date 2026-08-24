# Astrea Paper Wallpaper Core v1 — Design

**Status:** Approved implementation design for the current Eclipse/AstreaOS source tree.

**Scope:** Global static-image wallpaper authority. Dynamic, video, slideshow, per-output, per-workspace, lockscreen-specific selection, and Regulus implementation remain future work unless current-source evidence later supplies production infrastructure.

## Current source evidence

The audited Eclipse baseline is `0afe2696bfb72e7a459f1f5c4f7872054039230b`. The working tree is intentionally dirty; the current files, including untracked Paper files, are the real baseline.

`Shell/runtime/ShellRuntime.cpp` constructs `Paper::WallpaperService`, initializes it, and starts `Paper::WallpaperControlServer`. `Paper::WallpaperService` currently owns the in-memory snapshot, asynchronous validation, persistence calls, fallback resolution, generation, source watching, and operation completion. This is the existing authority to evolve, not a parallel service to replace.

`Paper/core/WallpaperPersistence.cpp` atomically writes `AstreaOS/paper.ini` with `QSaveFile`. It currently persists a descriptor containing a source path, logical ID, kind, source kind, fit, and scope. `migrateLegacy()` recognizes a legacy symlink, but does not import it into a managed library or persist a stable catalog identity.

`Paper/core/WallpaperResolver.cpp` validates local/file-URI image sources, resolves the packaged/default candidates, and provides an emergency QRC fallback. It is a resolver, not a catalog. `WallpaperDescriptor::externalFile()` currently uses the source path as the logical ID.

`Paper/platform/wayland/WallpaperSurfaceManager` subscribes to `effectiveWallpaperChanged`; `WallpaperSurfaceBundle` passes the effective descriptor to the asynchronous, two-slot, generation-guarded `Paper/qml/WallpaperSurface.qml`. The renderer surface does not own persistence.

Settings QML calls native `SettingsWallpaperController`; it does not directly mutate files. Paper owns the current writable mutation path through the dedicated `$XDG_RUNTIME_DIR/astrea-shell/wallpaper.sock`. The server exposes `get`, `list`, `import`, stable-ID `set`, `reset`, and `default`. A path-oriented `set` is a transport convenience implemented as managed `import + select`; it never persists the caller's absolute path as the public identity. Typhon's `astreactl` client talks directly to this Paper endpoint and does not carry wallpaper semantics in Typhon's compositor protocol.

No current lockscreen implementation, Paper thumbnail/blur cache, `WallpaperCatalog`, managed user library, or stable path-independent `WallpaperId` was found. Historical translation keys and the supplied archive mention older Paper library/lockscreen/Python helper names, but those are historical evidence only.

## Selected architecture

Paper is consolidated into these logical responsibilities:

```text
WallpaperCatalog
  list(), resolve(id), contains(id), import(path)

WallpaperSelection
  optional configured WallpaperId + presentation mode

WallpaperSelectionStore
  load(), save(), clear()

EffectiveWallpaperResolver
  configured -> system default -> built-in fallback

WallpaperController / existing WallpaperService
  serialized transactional mutations and committed events

DerivedAssetCache
  thumbnails, previews, and blur derivatives only
```

The existing `WallpaperService` remains the controller compatibility name. New catalog and selection responsibilities are injected into or composed beneath it rather than creating another writable authority.

### Stable identity and descriptor

The existing `logicalId` field becomes the stable public identity. System entries use stable IDs such as `astrea://wallpaper/default` and `astrea://wallpaper/emergency`; imported entries use `astrea://wallpaper/user/<sha256>`. User IDs are content-addressed, so repeated imports of identical content reuse one catalog entry.

Descriptors gain explicit origin (`system` or `user`), display name, and stable identity metadata while retaining source/resolved-source fields for resolver and renderer use. Absolute paths are never the public identity contract. Only static image kind and global scope are executable in v1; future enum values may remain representable but are rejected at the controller boundary.

The supported public presentation modes are the current equivalent concepts `cover`, `contain`, `stretch`, and `center`. Existing `tile` compatibility is retained only where required by current clients, not expanded as a new v1 feature.

### Catalog, import, and storage

The catalog is the only component that scans system and managed user wallpaper directories. System assets continue to use the existing installed/package conventions recognized by the resolver. Managed user assets live beneath the XDG generic data location, for example `AstreaOS/Paper/wallpapers`; the exact path is private and derived from `QStandardPaths`.

Import is separate from selection:

```text
importWallpaper(path) -> WallpaperId
selectWallpaper(WallpaperId)
resetWallpaper()
```

Import accepts only a readable regular file, validates a supported image with bounded dimensions/resource usage, computes a content digest, copies to a temporary file inside the managed directory, validates the copy, atomically renames it into its final content-addressed name, and only then publishes the catalog entry. Directories, special files, malformed images, unsupported images, failed copies, and failed renames cannot change the current selection or appear as catalog entries. A path-oriented `set` request remains a compatibility convenience implemented as `import + select`; the native domain API is ID-oriented.

### Persistence and effective resolution

`WallpaperSelectionStore` is the persistence boundary. The current atomic INI backend is retained behind it because no usable Regulus implementation was found in the current repository. Regulus is not introduced as a new dependency, and Paper never stores image bytes or derived cache data in configuration. Existing descriptor persistence remains readable for migration/compatibility, while newly committed state stores the stable ID and fit as the authority.

Selection is serialized and transactional:

```text
resolve ID -> validate asset -> persist selection -> commit state
-> increment generation -> emit one committed change event
```

If persistence fails, the previous configured/effective state remains authoritative and no success event is emitted. Request tokens/generation prevent stale asynchronous work from overwriting newer state.

Effective resolution is explicit:

```text
valid configured ID -> configured wallpaper
otherwise          -> valid system default
otherwise          -> built-in resource-free background fallback
```

When a configured asset disappears, configured intent remains recorded as a missing ID while the effective wallpaper falls back. The desktop cannot become blank solely because an asset disappeared.

### Events and consumers

Paper emits a committed `WallpaperChanged`-equivalent event containing previous/effective descriptors, generation, and reason. Existing Qt property signals remain compatibility notifications emitted from the same commit path. Startup publishes an initial snapshot, equal state produces no duplicate event, and failed mutations produce no success event.

Desktop consumes only the effective snapshot. It does not know QSettings/Regulus, symlinks, legacy files, or catalog paths. Settings receives catalog snapshots and invokes native controller operations. QML owns presentation only. No current lockscreen consumer exists; a future lockscreen consumes the same effective wallpaper unless a deliberate separate policy is introduced. Blur and thumbnails are disposable derived assets.

### Migration, external control, and performance

Confirmed legacy migration is the current symlink path handled by `XdgWallpaperPersistence`. Migration validates, imports, persists the stable ID, and leaves legacy data untouched. It is idempotent because the new selection store is authoritative; missing/invalid legacy data falls back safely and can be retried.

The dedicated Paper local socket is retained as the transport adapter. After the domain API exists it exposes `get`, `list`, `import`, ID-oriented `set`, and `reset`. No wallpaper RPC is added to Typhon's compositor protocol and no `wallpaperctl` is created. `set <path>` remains a compatibility convenience that may import then select.

Paper has no polling loop, periodic decode, or periodic catalog scan. The existing source watcher is retained only for concrete disappearance/reappearance reconciliation. Renderer image loading remains asynchronous and generation-guarded. Derived caches use stable ID plus source fingerprint, are bounded, and are disposable.

## Rejected alternatives and future work

QML-only persistence, symlink-as-public-API, a Regulus-first rewrite, Typhon protocol coupling, recreation of historical dynamic/lockscreen Python systems, and unfinished per-output/workspace APIs are rejected for v1. Dynamic policy, video, slideshow, time-of-day switching, intentional separate lockscreen selection, per-output/workspace targets, and a Regulus-backed store are later milestones.

## Verification strategy

The implemented tests cover stable IDs, system/user catalog listing, valid/invalid import, deterministic duplicates, atomic publication, persistence/reconstruction, persistence failure, reset, default/configured/fallback resolution, missing asset behavior, rapid serialized mutations, event cardinality, legacy-symlink migration, Settings/controller integration, and dedicated IPC. Existing renderer tests continue to cover asynchronous surface propagation and generation guards. Special-file import fuzzing, a dedicated stale-completion fixture for every consumer, and native-session qualification remain follow-up verification because the repository-level CMake regeneration and native session are not available in this working tree.
