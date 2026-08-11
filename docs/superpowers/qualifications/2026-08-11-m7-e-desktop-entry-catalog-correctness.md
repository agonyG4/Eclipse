# M7-E Shared Desktop Entry Catalog Correctness Closure Qualification

## Result

M7-E PASS.

The shared C++ `DesktopEntryCatalog` is the authoritative production catalog for XDG application metadata. It now parses desktop-entry semantics, discovers nested entries deterministically, resolves precedence and tombstones, publishes immutable indexed snapshots, and reconciles recursive filesystem watches. Spotlight's external-catalog path applies locale and searchable-policy projection without performing an XDG scan.

## Repository state

- Repository: Eclipse
- Branch: `main`
- Reviewed baseline: `04a10e4 fix(launcher): preserve Typhon session environment`
- Design commit: `809b17c docs(shell): design M7-E catalog correctness closure`
- Implementation commit: `8e56432 fix(catalog): close desktop entry correctness`
- Native Typhon compositor qualification: not required for M7-E, per the approved scope; this task changes metadata discovery and search projection, not compositor protocol behavior.

## Implemented architecture

- Added the pure `shared/apps/DesktopEntryParser` boundary. It parses only `[Desktop Entry]`, validates explicit `Type=Application` and `Name`, decodes ordinary escapes, parses escaped-semicolon lists, preserves localized names/generic names/comments/keywords, and carries launch, identity, visibility, and source-path metadata.
- Kept `DesktopEntryCatalog` as the sole production XDG owner. It recursively scans ordered XDG and Flatpak roots, skips directory symlinks, uses lexical traversal, caps each root at 10,000 desktop files, computes nested IDs, applies `Hidden=true` tombstones, preserves `NoDisplay` in raw snapshots, and serializes `localized_keywords`.
- Preserved immutable snapshot publication and same-snapshot secondary indexes, with one revision increment per rebuild and post-publication `indexUpdated()` emission.
- Reconciled watches for roots, discovered subdirectories, files, and nearest existing ancestors for missing roots through the existing 250 ms debounce path.
- Added the Rust external projection for locale fallback, localized keywords, `NoDisplay`, direct `XDG_CURRENT_DESKTOP` token matching, `OnlyShowIn`/`NotShowIn`, and executable-regular-file `TryExec` resolution. Creation, `setCatalog()`, and external `reload()` use the same projection and do not invoke the internal XDG scan.

## Semantic cases proven

- Explicit application type and required name; Link, Directory, unknown, missing-Type, and missing-Name records are rejected.
- Top-level and nested desktop-file identity, including `vendor/tools/example.desktop` becoming `vendor-tools-example.desktop` and ID `vendor-tools-example`.
- Discovery through numeric recursion depth 5, directory-symlink exclusion, deterministic ordering, and the per-root file cap.
- XDG root precedence and valid higher-priority Hidden tombstones.
- Raw `NoDisplay` preservation for identity consumers and Spotlight exclusion during projection.
- `\\s`, `\\n`, `\\t`, `\\r`, `\\\\`, escaped semicolons, localized fields, and localized keywords.
- Nested create/change/delete, nested directory creation, root deletion/recreation, debounce, watcher reconciliation, and snapshot pointer/revision/index invariants.
- Locale cases `pt_BR.UTF-8`, `pt_PT.UTF-8`, `sr_RS.UTF-8@latin`, unknown-locale fallback, and localized keyword search.
- Visibility cases for `OnlyShowIn`, `NotShowIn`, exclusion precedence, and `NoDisplay`.
- `TryExec`: absolute executable, absolute missing, absolute non-executable, PATH executable, and missing PATH command.
- External catalog replacement and reload, including active locale/policy preservation and absence of internal-scanner records.
- End-to-end temporary-XDG tree to C++ snapshot JSON to Rust search, including nested computed IDs and raw-versus-searchable record differences.

## Verification

Focused C++ and Rust checks:

```text
cmake --build build/debug --target desktop-entry-catalog-test spotlight-tests -j1
build/debug/shared/desktop-entry-catalog-test -o -,txt
  16 passed, 0 failed

cargo fmt --manifest-path Spotlight/backend/Cargo.toml -- --check
cargo test --manifest-path Spotlight/backend/Cargo.toml
  29 passed, 0 failed; doc-tests: 0 passed, 0 failed
```

The complete configured CTest matrix was rebuilt and run serially with `-j1` after the final source changes:

| Configuration | Result |
|---|---|
| Debug | 49/49 passed |
| Release | 49/49 passed |
| Clang | 49/49 passed |
| ASan | 49/49 passed; no sanitizer diagnostics |
| UBSan | 49/49 passed; no sanitizer diagnostics |
| no-Typhon | 49/49 passed; expected skips only |

The no-Typhon skips were the existing Typhon-dependent `typhon-protocol-integration-test`, `typhon-shortcut-protocol-integration-test`, and `shell-unified-runtime-integration-test`. All other tests passed.

Additional verification:

- `git diff --check`: passed before implementation commit.
- Rust formatting check: passed.
- No QML files changed.
- No Typhon source, protocol XML, authentication, or compositor files changed.

An exploratory attempt that launched all six CTest suites concurrently produced unrelated fixed-local-socket collisions in IPC tests. The affected tests passed immediately when run serially, and the complete final matrix above was rerun serially as required. No source fix was made for that environmental interference.

## Ownership audit

The production search confirms one shared `QFileSystemWatcher` owner in `DesktopEntryCatalog`. The Rust `DesktopEntryIndex` directory scanner remains only in the no-external-catalog fallback path. The external backend path calls projection and ranking only; it does not call `DesktopEntryIndex::reload()`.

The final implementation contains no Typhon or QML changes, no second production catalog, no asynchronous scanning worker, no ranking rewrite, and no unrelated visual or protocol changes.

## Intentional clarification

The approved requirement says entries must be discoverable through depth 5. The final collector therefore includes directories at numeric depth 5 (`depth > 5` is the stop condition), and the fallback Rust scanner uses the same boundary. This makes the acceptance wording explicit while retaining the approved synchronous, bounded traversal design.
