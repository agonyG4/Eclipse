# Unified icons implementation plan

Execution: inline, explicitly authorized by the user; no subagents or branches.

**Goal:** Share icon resolution and presentation between Eclipse SDK and Explorer.
**Architecture:** SDK owns the resolver, optional file-icon service/provider and shared QML presentation. Existing app APIs and provider URLs are adapters. Thumbnail generation stays separate.
**Tech Stack:** C++17, Qt6, QML, CMake.
**Spec:** ../specs/2026-11-02-unified-icons-design.md

## Constraints
Build/test output on Aether; preserve public AstreaAppIcon API; no source-tree dependency from Orbit to Eclipse; preserve existing user edits.

## Review focus
Candidate ordering and inheritance, fractional DPR, custom icons and missing emblems, no preview blank frames, installed SDK relocation.

## Task 1: Shared resolver and file service
- [x] Extend independent SDK consumer with an ordered-candidate fixture and optional file-provider check. Run and observe missing API failure.
- [x] Move Explorer Freedesktop catalog, IconThemeService and provider into SDK; export installed headers. Replace Orbit implementations with aliases.
- [x] Route existing SDK named provider through shared catalog, retaining Qt rendering and cache contracts.
- [x] Run SDK provider and moved Explorer fixture tests, including missing icon and invalidation cases.

## Task 2: Common QML presentation
- [x] Add runtime QML tests for AstreaIcon/AstreaFileIcon, preview fallback and unchanged AstreaAppIcon API; observe failure.
- [x] Extract shared AstreaIcon, add thin app/file adapters, register in SDK QML module.
- [x] Migrate Explorer icon/preview presentation while retaining cover/scheduling and emblems.
- [x] Run QML and installed consumer tests.

## Task 3: Orbit integration and completion
- [x] Discover installed SDK through CMake; initialize SDK in Explorer without including unrelated working-tree changes.
- [x] Build/test Orbit and SDK on Aether; inspect diff and installation dependencies.
- [x] Update SDK architecture documentation and commit logical changes in both repositories.

## Execution evidence

- SDK consumer RED: installed headers/API absent; QML RED: AstreaFileIcon not a type.
- Asset watcher RED: provider revision stayed unchanged after atomic nested PNG replacement; GREEN after shared catalog watch paths were registered and Qt pixmaps invalidated.
- Shared SDK provider: 27 QtTest cases passed, covering priority, DPR, thresholds, inheritance, negative cache and concurrent invalidation.
- Common QML presentation: 18 cases passed under XCB/OpenGL, including preserved app API, shader detail/alpha and original file colors under a tinted system theme.
- Explorer: resolver/provider suites and 33 runtime preview/layout cases passed; Python QML suite: 57 tests passed.
- Installed SDK consumer passed at original and relocated prefixes. Installed Explorer self-test loaded QML from Aether install-prefix.
- Offscreen's software backend cannot render the shader pixel tests; graphical qualification uses XCB/OpenGL. Existing source-based native checks were removed from Python; native behavior remains in real C++ fixture tests against the SDK.
- Ruling: preserve distinct named-provider and file-service configuration precedence for compatibility; both use the same resolver. Thumbnail generation/scheduling stays separate, including Explorer's specialized preview upgrade/cover logic.
- Ruling: work inline on main, no agents, as explicitly requested. Existing Orbit .gitignore/app_id edits are excluded from task commits.
