# Implementation Plan: Native Dock Foundation

## Dependency Order

### Phase 1: Shared foundations

1. Add shared launcher API and implementation, preserving the existing
   Spotlight behavior and moving its tests/includes to the shared target.
2. Add shared desktop-entry catalog with full filename identity, XDG priority,
   immutable snapshots, and file/directory watcher recovery; migrate AltTab.
3. Add one authoritative shared icon QML implementation and a configurable
   Layer Shell helper; migrate existing consumers without changing policy.

Checkpoint: existing Spotlight and AltTab tests build and pass.

### Phase 2: Dock value types and deterministic core

4. Add Dock runtime paths and validated configuration watcher with temporary
   file based tests.
5. Add Dock record and stable-keyed model; write model tests first for order,
   duplicates, unresolved rows, exact roles, and launch state.
6. Add Dock controller with an injectable launcher interface and fake-launcher
   tests for per-item suppression and cleanup.

Checkpoint: Dock core tests pass without QML, Wayland, or real processes.

### Phase 3: Runtime and presentation

7. Add versioned Dock IPC, command line handling, JSON status, and lifecycle
   tests including stale sockets and bounded client timeouts.
8. Add Dock Layer Shell surface lifecycle, QML panel/delegate, resident
   application bootstrap, and systemd packaging.
9. Add root CMake orchestration, CTest registration, warnings/sanitizers, and
   install rules.

Checkpoint: root configure/build/test succeeds with LayerShellQt optional for
   developer diagnostics and required at runtime for production mapping.

### Phase 4: Documentation and validation

10. Write Dock architecture, runtime flow, configuration, and testing docs;
    update the root README.
11. Run formatting/static checks, normal CTest, ASan, UBSan, and inspect the
    final diff for out-of-scope changes.
12. Perform a Typhon session smoke test only if a usable session is available;
    report it separately from deterministic checks.

## Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Existing CMake files independently add `shared/` | Duplicate target | Root and component files guard `astrea-shared` creation. |
| LayerShellQt API differs across installations | Build/runtime failure | Keep helper compile-guarded and validate only typed configuration. |
| QFileSystemWatcher drops atomic-replaced files | Stale pins/config | Watch both files and parent directories, remove/re-add paths on refresh. |
| Desktop catalog filters hidden entries needed by explicit pins | Missing user pin | Preserve catalog records and enforce visibility policy at Dock resolution. |
| Launcher lifetime is single-flight | Launch storms | Controller tracks pending desktop filename independently per item; shared launcher reports bounded completion. |
| QML shared component import packaging | Runtime icon failures | Put the canonical component in a shared QML module and retain compatibility wrappers where existing modules require them. |
