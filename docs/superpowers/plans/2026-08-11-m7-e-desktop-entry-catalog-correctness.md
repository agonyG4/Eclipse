# M7-E Shared Desktop Entry Catalog Correctness Closure Implementation Plan

> **For agentic workers:** Execute this plan inline in the existing Eclipse checkout. Work directly on `main`; do not use branches, worktrees, reset, clean, revert, amend, or history rewrite.

**Goal:** Make the shared C++ desktop-entry catalog authoritative and semantically correct for recursive XDG discovery, then project its raw records correctly into external Spotlight search.

**Architecture:** Extract one pure `DesktopEntryParser` from `DesktopEntryCatalog`; keep catalog discovery, precedence, tombstones, immutable snapshots, indexes, recursive watchers, debounce, revisioning, and JSON serialization in C++. Add locale and searchable-policy projection at the Rust external-catalog boundary without restoring production XDG scanning or a second watcher.

**Tech Stack:** C++20, Qt 6 Core/Test, QFileSystemWatcher, Rust 2021, serde/serde_json, existing CMake/Cargo test targets, CTest.

## Global Constraints

- Work only in `/home/agony/GitHub/Eclipse` on `main`; preserve newer changes if encountered.
- Do not modify Typhon, QML, protocol XML, systemd topology, launcher supervision, or the Rust ranking algorithm.
- Preserve one production `DesktopEntryCatalog`, one directory-watcher owner, immutable snapshot publication, and existing top-level desktop IDs.
- Recursive scan depth is exactly 5; do not follow directory symlinks; cap each root at 10,000 desktop files with deterministic ordering.
- Rebuilds remain synchronous and use the existing 250 ms debounce path.
- Hidden is an effective-ID tombstone; NoDisplay remains in authoritative raw records and is filtered only by Spotlight projection.
- All new design, plan, qualification, comments, and Markdown text is English.
- Use `apply_patch` for source/document edits and verify every production change with a failing test first.

## File map

- Create `shared/apps/DesktopEntryParser.hpp/.cpp`: pure one-file parser and escape/list helpers.
- Modify `shared/apps/DesktopEntryCatalog.hpp/.cpp`: record localized keywords, recursive effective-ID construction, precedence, immutable indexes, recursive watch reconciliation, and JSON field.
- Modify `shared/tests/DesktopEntryCatalogTest.cpp`: C++ semantic, precedence, snapshot, and watcher coverage.
- Modify `shared/CMakeLists.txt`: add parser source to the shared library and focused test target.
- Modify `Spotlight/backend/src/desktop/entries.rs`: external-catalog data model, locale fallback, literal desktop-token filtering, executable `TryExec`, and fallback-index cleanup only where needed for shared semantics.
- Modify `Spotlight/backend/src/lib.rs` and `Spotlight/backend/src/search/ranking.rs` only as needed to build a filtered locale-selected external projection while leaving scoring behavior unchanged.
- Modify `Spotlight/backend/src/tests.rs`: external projection and replacement/reload tests.
- Modify `Spotlight/tests/SpotlightBackendTest.cpp`: catalog-to-Rust bridge integration fixture.
- Create `docs/superpowers/qualifications/2026-08-11-m7-e-desktop-entry-catalog-correctness.md` after verification.

### Task 1: Specify the C++ parser and catalog semantics

**Files:** Modify `shared/tests/DesktopEntryCatalogTest.cpp`; modify `shared/CMakeLists.txt` only if the focused target needs parser linkage.

- [ ] Add one failing test fixture containing base/localized fields, escaped ordinary values, escaped-semicolon keywords, visibility lists, `TryExec`, and `localizedKeywords`; assert exact decoded values.
- [ ] Add failing tests proving `Type=Link`, `Directory`, unknown, missing `Type`, and missing `Name` are absent.
- [ ] Add failing nested-ID and deterministic precedence tests, including a higher-priority `Hidden=true` tombstone and preserved `NoDisplay=true` record.
- [ ] Add failing snapshot-pointer/revision/index assertions and JSON `localized_keywords` assertions.
- [ ] Build and run `desktop-entry-catalog-test`; record the expected failures before production implementation.

### Task 2: Implement the pure parser

**Files:** Create `shared/apps/DesktopEntryParser.hpp`; create `shared/apps/DesktopEntryParser.cpp`; modify `shared/apps/DesktopEntryCatalog.hpp`; modify `shared/CMakeLists.txt`.

- [ ] Define a parser result that distinguishes a valid `Type=Application` record from an unusable file and accepts the source path plus application root.
- [ ] Parse only `[Desktop Entry]` with exact case-sensitive keys, ignore comments/unrelated groups, trim optional key/value delimiter whitespace, and reject missing/invalid `Type` or empty `Name`.
- [ ] Decode `\\s`, `\\n`, `\\t`, `\\r`, and `\\\\` for ordinary values.
- [ ] Split lists in one escape-aware pass so `\\;` is data, not a separator; decode the supported escapes after separator recognition and remove terminal empty list elements.
- [ ] Populate all required base/localized fields including `localizedKeywords`, launch identity, visibility, booleans, and source path. Preserve raw `Hidden` and `NoDisplay` flags.
- [ ] Run the focused parser/catalog tests; fix production code until all new parser assertions pass.

### Task 3: Implement recursive discovery, precedence, and watcher reconciliation

**Files:** Modify `shared/apps/DesktopEntryCatalog.cpp`; modify `shared/tests/DesktopEntryCatalogTest.cpp`.

- [ ] Add deterministic recursive collection to depth 5, skipping directory symlinks, sorting directory entries lexically, and enforcing the 10,000-file root cap.
- [ ] Compute the desktop-file name from the path relative to the application root, replacing separators with `-`; derive ID by removing only the final `.desktop` suffix.
- [ ] Resolve IDs in root priority order. A valid non-hidden candidate publishes once; a valid hidden candidate consumes the ID without publishing; all lower-priority candidates are ignored. Invalid candidates do not consume IDs.
- [ ] Build `entries`, `byDesktopId`, `byDesktopFileName`, and `byStartupWmClass` from one next snapshot before lock publication. Preserve the previous revision plus one exactly once per successful rebuild.
- [ ] Reconcile watches with all discovered subdirectories and nearest existing ancestors for absent roots, deduplicating paths and removing stale watches.
- [ ] Add event-driven tests for nested create/change/delete, nested directory creation, and root delete/recreate; assert one debounced update per operation and stable snapshot pointer between rebuilds.
- [ ] Run the complete focused C++ catalog target and `ctest -R desktop-entry-catalog`.

### Task 4: Serialize the raw catalog contract

**Files:** Modify `shared/apps/DesktopEntryCatalog.cpp`; modify `shared/tests/DesktopEntryCatalogTest.cpp`.

- [ ] Add `localized_keywords` as an object mapping locale tags to decoded keyword arrays.
- [ ] Keep every existing JSON key unchanged, including `id`, `name`, `generic_name`, `comment`, localized maps, launch fields, `desktop_file_name`, visibility fields, and `path`.
- [ ] Assert nested computed IDs and localized keyword JSON values in the C++ test.
- [ ] Run the focused target and inspect the JSON object for raw `NoDisplay` and `Hidden` values.

### Task 5: Specify and implement Rust external-catalog projection

**Files:** Modify `Spotlight/backend/src/desktop/entries.rs`; modify `Spotlight/backend/src/lib.rs`; modify `Spotlight/backend/src/search/ranking.rs` only if localized selected data must be folded into the existing metadata string; modify `Spotlight/backend/src/tests.rs`.

- [ ] Extend `DesktopEntry` with `localized_keywords: HashMap<String, Vec<String>>` using serde defaults so old fixtures remain source-compatible.
- [ ] Add locale candidates matching the required order: `sr_RS.UTF-8@latin` -> `sr_RS@latin`, `sr_RS`, `sr@latin`, `sr`, default; `pt_BR.UTF-8` -> `pt_BR`, `pt`, default; `pt` -> `pt`, default. Normalize encoding only for matching and preserve deterministic map lookup.
- [ ] Make external-catalog projection select effective Name, GenericName, Comment, and Keywords from raw maps without mutating the stored external records. Apply `NoDisplay`, `OnlyShowIn`, `NotShowIn`, and executable `TryExec` only to the searchable projection.
- [ ] Match `XDG_CURRENT_DESKTOP` directly as colon-separated trimmed tokens, with exclusion winning if both OnlyShowIn and NotShowIn match. Remove the hardcoded desktop-environment enum from the external policy path.
- [ ] Validate absolute `TryExec` as an executable regular file and non-absolute `TryExec` by searching the test/process PATH for an executable regular file. Do not accept merely existing non-executable files.
- [ ] Ensure `new_with_catalog`, `set_catalog`, and external `reload` all call the same projection; external mode must not call `DesktopEntryIndex::reload()`.
- [ ] Add failing tests first for each locale, localized keywords, visibility rule, five TryExec cases, replacement, and reload; run `cargo test --manifest-path Spotlight/backend/Cargo.toml` to verify red, then implement and rerun until green.

### Task 6: Add the C++/Rust bridge integration test

**Files:** Modify `Spotlight/tests/SpotlightBackendTest.cpp`.

- [ ] Create a temporary XDG home with a nested application, localized Name/Keywords, NoDisplay, incompatible OnlyShowIn/NotShowIn, valid TryExec, and invalid TryExec.
- [ ] Initialize `DesktopEntryCatalog`, pass `snapshotJson()` to `RustSpotlightBackend::createWithCatalog`, set deterministic locale/desktop environment/PATH, and search localized name and keyword.
- [ ] Assert the C++ raw snapshot contains identity metadata for NoDisplay and policy-ineligible records while Rust results exclude them.
- [ ] Assert the nested result contains `id=vendor-tools-example` and `desktopFileName=vendor-tools-example.desktop`, not the source basename.
- [ ] Call `setCatalog` with replacement JSON and `reload`; assert replacement, locale, and policy remain active with no internal XDG scan.
- [ ] Run `spotlight-tests` and the focused C++/Rust tests.

### Task 7: Review and qualify

**Files:** Create `docs/superpowers/qualifications/2026-08-11-m7-e-desktop-entry-catalog-correctness.md`.

- [ ] Inspect the complete diff for duplicate parser logic, second production watcher, basename IDs, stale index references, environment leakage, nondeterministic traversal, unbounded collection, unrelated refactors, QML changes, and Typhon changes.
- [ ] Run `git diff --check` and the focused target set available in the current build: catalog, Spotlight, Typhon matcher, Dock projector/controller, AltTab identity, shell runtime, and unified runtime integration.
- [ ] Run `cargo test --manifest-path Spotlight/backend/Cargo.toml` directly.
- [ ] Run serial CTest for every available Debug, Release, Clang, ASan, UBSan, and no-Typhon tree. Record unavailable trees and exact blockers instead of inventing PASS results.
- [ ] Record baseline HEAD, implementation commit(s), exact commands/results, sanitizer/no-Typhon results, ownership audit, semantic cases, deviations, and native qualification status in the qualification document.
- [ ] Re-run `git status --short`, confirm only intended Eclipse files are present, and commit the coherent completed work directly on `main`.
