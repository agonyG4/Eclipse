# M7-E Shared Desktop Entry Catalog Correctness Closure

## Baseline and objective

This design applies to Eclipse commit `04a10e4` on `main`, with a clean working tree. The objective is to close the desktop-entry semantic regression introduced by M7-D while preserving the unified shell ownership model. `DesktopEntryCatalog` remains the sole production owner of XDG application discovery, parsing, precedence, snapshot publication, revisioning, and filesystem watching. Spotlight receives serialized authoritative records and applies only its searchable-result projection.

The task changes no QML, Typhon source or protocol, compositor behavior, launch supervision, systemd topology, or Rust ranking algorithm. The existing Rust filesystem index remains available only for standalone/focused fallback tests when no external catalog is attached.

## Responsibility boundaries

`DesktopEntryParser` is a pure, independently testable C++ parser for one desktop file. It reads only the `[Desktop Entry]` group, applies exact-key matching, decodes desktop-entry escapes, parses localized fields and escaped-semicolon lists, validates `Type=Application`, and rejects missing `Name`. It has no Qt object lifetime, watcher, XDG-root, revision, locale-selection, Spotlight-policy, or launch responsibilities.

`DesktopEntryCatalog` discovers the ordered application roots, cleans and deduplicates them without changing priority, recursively scans each root to depth 5 without following directory symlinks, computes path-relative desktop-file IDs, applies deterministic lexical ordering and the 10,000-file-per-root limit, resolves effective IDs in priority order, treats valid `Hidden=true` records as tombstones, constructs immutable snapshots and secondary indexes, reconciles recursive directory watches, debounces rebuilds at 250 ms, and serializes raw metadata for Spotlight.

The catalog's record model preserves base and localized `Name`, `GenericName`, `Comment`, and `Keywords`, plus launch, identity, visibility, and list metadata. `NoDisplay` remains in the raw snapshot because Dock, AltTab, and Typhon identity consumers may need it; it is not a tombstone.

The Rust external-catalog projection selects effective locale values without mutating the C++ snapshot. It applies locale fallback to `Name`, `GenericName`, `Comment`, and `Keywords`, excludes `NoDisplay`, enforces `OnlyShowIn`/`NotShowIn` from literal `XDG_CURRENT_DESKTOP` tokens, and validates `TryExec` as an executable regular file either by absolute path or through `PATH`. The same projection is rebuilt on creation, `setCatalog()`, and external-mode `reload()`. External mode never starts or performs an XDG scan.

## Data flow

```text
desktop file
    -> DesktopEntryParser::parse(path, applicationRoot)
    -> ordered DesktopEntryRecord candidates
    -> DesktopEntryCatalog effective-ID resolution
    -> immutable DesktopEntrySnapshot + indexes + raw JSON
    -> Rust external-catalog locale/visibility/TryExec projection
    -> existing Spotlight ranking/search
```

The computed identity for a nested path is stable and launch-compatible: `vendor/tools/example.desktop` becomes `vendor-tools-example.desktop` and ID `vendor-tools-example`. Top-level IDs remain unchanged. All snapshot indexes are built from the same `entries` vector before publication under the existing read/write lock; `indexUpdated()` is emitted only after the new shared pointer is visible.

## Watcher and recovery behavior

After every rebuild, the catalog watches every existing application root and every discovered subdirectory through depth 5. For an absent root it watches the nearest existing ancestor. The watcher set is deduplicated and fully reconciled, so removed directories do not leave stale paths. Any watched directory change schedules the existing single-shot 250 ms rebuild. Root creation, deletion, recreation, nested-directory creation/deletion, and desktop-file changes therefore converge through the same rebuild path.

The recursive collector uses deterministic lexical directory/file ordering, does not follow directory symlinks, and stops deterministically after 10,000 desktop files in a root. Scanning remains synchronous as required by M7-E.

## Testing strategy

The C++ catalog test target first specifies parser fields, type/name rejection, escapes, localized keywords, nested IDs, precedence, Hidden tombstones, NoDisplay preservation, snapshot invariants, recursive watches, nested-directory creation, and root recovery. Tests use `QSignalSpy`/event-driven `QTRY_*` assertions for watcher completion and verify that each rebuild changes the revision once and replaces the snapshot atomically.

Rust unit tests exercise the external projection directly through `AstreaSpotlightBackend::new_with_catalog`, `set_catalog`, and `reload`, covering `pt_BR.UTF-8`, `pt_PT.UTF-8`, `sr_RS.UTF-8@latin`, unknown-locale fallback, localized keywords, `NoDisplay`, literal desktop tokens, `TryExec` absolute/PATH success and failure, catalog replacement, and external reload. Environment variables and PATH are restored in every test.

The C++ Spotlight test adds a temporary-XDG end-to-end bridge fixture containing nested, localized, filtered, and TryExec-valid/invalid records. It proves that the raw catalog intentionally contains identity metadata that the searchable projection excludes and that the nested search result carries the computed desktop-file ID.

Focused catalog, Spotlight, Typhon matcher, Dock projector/controller, AltTab identity, shell runtime, and unified-runtime tests are followed by the available Debug, Release, Clang, ASan, UBSan, and no-Typhon serial CTest matrix, direct Rust backend tests, `git diff --check`, and a source/diff ownership audit. Any unavailable matrix entry is recorded as a blocker rather than reported as passing.

## Acceptance invariants

- One production `DesktopEntryCatalog` and one application-directory watcher owner remain in the unified shell.
- Only explicit `Type=Application` records with a non-empty `Name` enter the catalog.
- Higher-priority valid IDs consume lower-priority IDs; `Hidden=true` consumes without publishing.
- Localized raw fields survive C++ serialization under stable existing JSON keys plus `localized_keywords`.
- External Spotlight projection is locale- and policy-correct and does not rescan XDG roots.
- Every published index points into its own immutable snapshot.
- Existing top-level desktop IDs, Dock behavior, Typhon matching, AltTab identity, QML behavior, and M7-D ownership remain unchanged.
