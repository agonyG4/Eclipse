# Astrea Paper Wallpaper Core v1 — Closure Report

**Audit date:** 2026-08-20  
**Repositories:** `/home/agony/GitHub/Eclipse`, `/home/agony/GitHub/Typhon`  
**Scope:** Global static-image wallpaper authority, managed imports, stable selection, Paper control, Settings integration, and direct `astreactl` access.

## Repository baseline

- Eclipse baseline: `0afe2696bfb72e7a459f1f5c4f7872054039230b`.
- Typhon baseline: `0ef9f7b99fa38d0fc04bf5ffa8f494db5a6eade6`.
- AstreaOS compatibility reference: `c70bb69b3c2089ce792b7ca6869109dc8418c22a`.

All three worktrees were already dirty. Existing changes, build outputs, compatibility sources, and unrelated untracked artifacts were preserved; this closure work only touched the Paper-related implementation, its regression tests, and the requested design/plan/report documents.

## Result

Paper now owns wallpaper state, catalog access, stable selection persistence, effective resolution, transactional mutation, change events, and the local control surface. Desktop rendering and Settings consume Paper state; neither is the persistence authority.

The implementation deliberately does not add dynamic/video/slideshow modes, per-output or per-workspace selection, a separate lockscreen policy, Regulus, `wallpaperctl`, or wallpaper semantics to Typhon's compositor protocol. The path-oriented `set` compatibility surface is implemented as managed `import` followed by stable-ID selection, so absolute external paths are not persisted as wallpaper identity.

The repository audit found no current native lockscreen consumer, no thumbnail/blur cache, and no usable Regulus integration in scope. Those remain explicit follow-up work rather than undocumented assumptions.

## Authority map

```text
Settings / astreactl / future clients
              |
              v
       Paper local socket adapter
              |
              v
       WallpaperService controller
        /       |        \
       v        v         v
   Catalog   Selection   Events
       |        |         |
       v        v         v
  Resolver  QSaveFile   Desktop consumers
       |
       v
  Effective wallpaper
```

The existing `Paper::WallpaperService` remains the single mutation and snapshot authority. `WallpaperCatalog` owns discovery and managed imports. `WallpaperResolver` validates and resolves a descriptor. `XdgWallpaperPersistence` stores only the configured stable ID and fit for new selections; image bytes and library assets remain outside configuration.

## Implemented model

- `WallpaperDescriptor` now carries origin (`system`/`user`) and optional display metadata while retaining the existing source, resolved-source, fit, scope, kind, and logical-ID fields.
- System identities include `astrea://wallpaper/default` and `astrea://wallpaper/emergency`.
- Imported identities are content-addressed as `astrea://wallpaper/user/<sha256>` and resolve to a managed copy under the XDG data directory.
- v1 executes image/global descriptors only. Existing `tile` behavior remains accepted for compatibility.
- The effective resolution rule is configured valid wallpaper, then factory default, then the existing emergency fallback. Missing configured assets preserve configured intent and never blank the desktop.

## Catalog and import behavior

`WallpaperCatalog` scans the configured system and managed user roots, lists descriptors, resolves stable IDs, and imports only readable regular image files. Imports are bounded by file size and image dimensions, hashed once, copied into a temporary managed path, validated, and atomically renamed before publication. Duplicate content reuses the stable identity. Catalog scans inspect image headers rather than fully decoding every asset, avoiding repeated idle decoding.

Legacy symlinks remain untouched as compatibility input. When a valid legacy symlink is found and the catalog is available, Paper imports the target into the managed library, validates it, and persists the resulting stable ID. If managed import cannot proceed, the legacy descriptor remains usable as a compatibility fallback.

## Persistence and transaction semantics

New selections use an atomic `paper.ini` selection record containing the stable logical ID and fit. Existing descriptor-form persistence remains readable for compatibility and migration. The commit sequence is:

```text
resolve -> validate -> persist -> publish snapshot -> increment generation -> emit event
```

Persistence failure retains the previous authoritative state and produces no success completion. Existing request-token, supersession, timeout, and source-reconciliation logic remains in place.

Paper emits `wallpaperChanged(previous, current, generation, reason)` from the effective-state commit path. The renderer continues to receive the effective descriptor through the existing asynchronous, generation-guarded surface pipeline.

## External API

The dedicated Paper socket supports:

```text
wallpaper get {}
wallpaper list {}
wallpaper import {"path":"/path/image.png","fit":"contain"}
wallpaper set {"id":"astrea://wallpaper/user/<sha256>","fit":"cover"}
wallpaper reset {}
```

Path-oriented `set` remains as a compatibility operation implemented by managed `import` followed by selection. Requests are bounded, validated, serialized through Paper, and return final completed results. The endpoint remains the secure user runtime socket at `astrea-shell/wallpaper.sock`.

`astreactl` uses this endpoint directly:

```text
astreactl wallpaper get
astreactl wallpaper list
astreactl wallpaper import PATH [--fit MODE]
astreactl wallpaper set PATH_OR_ID [--fit MODE]
astreactl wallpaper reset
```

No wallpaper command was added to Typhon's compositor protocol.

## Settings and consumers

The native Settings controller now exposes catalog entries, configured/effective IDs, and native `refreshLibrary`, `selectWallpaper`, and `importWallpaper` operations. QML presents catalog choices and previews; it does not write persistence or library files. The existing path field remains a compatibility import/set entry point.

No current lockscreen implementation was found in the repository. The future lockscreen should consume the same effective Paper wallpaper unless a deliberate separate policy is introduced. No thumbnail, blur, or derived-asset cache currently exists; those should remain disposable, bounded runtime data and must not enter the selection store.

## Verification evidence

Focused Qt 6 harnesses were compiled with the repository sources and passed:

- Descriptor: **8 passed**.
- Persistence: **8 passed**.
- Catalog: **6 passed**.
- Service: **22 passed**.
- Paper control server: **10 passed**.
- Settings wallpaper controller: **13 passed**.
- Paper core source compilation: passed for descriptor, resolver, catalog, persistence, watcher, validation worker, service, and control server.

The configured `no-layer-shell` build also ran the focused CTest selection after the relevant targets were built: **8/8 tests passed** (`paper-*` and `settings-wallpaper`). The service and control-server targets were rebuilt after the managed-import regression tests were added.

Typhon verification passed:

- `cargo fmt --check`
- `cargo test --bin astreactl` — **3 passed**
- `cargo test astreactl::wallpaper --lib` — **6 passed**
- `cargo check --bin astreactl`
- `git diff --check` in both repositories

## Verification limitation

The existing `/home/agony/GitHub/Eclipse/build/no-layer-shell` directory remains an incremental, previously configured build tree. A clean CMake regeneration is still blocked by unrelated dirty work that references the missing file `/home/agony/GitHub/Eclipse/Bar/qml/NetworkPopup.qml`; that source was not recreated or modified. The focused build and CTest selection above use the configured tree and do not constitute a fresh full-repository rebuild.

No native Astrea session was available for visual or multi-monitor qualification. Renderer behavior is covered by the existing Paper surface tests and source audit, but live desktop, lockscreen, and output-specific qualification remain pending.

## Future milestones

- Add a dedicated derived-asset cache for thumbnails/previews with source fingerprints and bounded eviction.
- Add dynamic, slideshow, or video kinds only with explicit resolver and renderer contracts.
- Add per-output/workspace targets without changing the global v1 selection semantics.
- Add a lockscreen consumer and decide whether it shares or intentionally overrides the effective wallpaper.
- Re-run the full CMake/CTest suite after the unrelated missing QML source is restored, then perform native-session qualification.

## Relevant files

- [Paper design](/home/agony/GitHub/Eclipse/docs/superpowers/specs/2026-08-19-paper-wallpaper-core-v1-design.md)
- [Paper implementation plan](/home/agony/GitHub/Eclipse/docs/superpowers/plans/2026-08-19-paper-wallpaper-core-v1-closure.md)
- [Wallpaper catalog](/home/agony/GitHub/Eclipse/Paper/core/WallpaperCatalog.cpp)
- [Wallpaper service](/home/agony/GitHub/Eclipse/Paper/core/WallpaperService.cpp)
- [Paper control server](/home/agony/GitHub/Eclipse/Paper/platform/ipc/WallpaperControlServer.cpp)
- [Settings controller](/home/agony/GitHub/Eclipse/Settings/services/wallpaper/SettingsWallpaperController.cpp)
- [Settings wallpaper page](/home/agony/GitHub/Eclipse/Settings/qml/pages/appearance/Wallpaper.qml)
- [astreactl Paper client](/home/agony/GitHub/Typhon/src/astreactl/wallpaper.rs)
