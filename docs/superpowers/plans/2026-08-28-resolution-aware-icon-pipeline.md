# Resolution-Aware App Icon Pipeline Implementation Plan

## Execution constraints

- Repository: `/home/agony/GitHub/Eclipse` only.
- Preserve all pre-existing and concurrent worktree changes; stage only files belonging to this plan, using path-specific or interactive staging when a file contains unrelated edits.
- Use `rtk` for repository and verification commands.
- Do not use subagents.
- Keep new and modified Markdown in English.

## Stage 1: Establish the normalized render contract

Files:

- Add `shared/icons/IconRenderRequest.hpp`.
- Add the new header to `shared/CMakeLists.txt` if the project lists public sources explicitly.
- Add request-focused cases to `shared/tests/AstreaIconProviderTest.cpp`.

Implementation:

1. Define `IconRenderRequest` with logical extent, normalized DPR, and physical extent.
2. Provide a checked constructor/factory accepting real-valued logical extent and DPR. Reject non-finite and non-positive values, clamp finite values to the documented logical, DPR, and physical limits, and calculate physical pixels with a checked ceiling.
3. Quantize DPR to a stable precision for URL and cache-key use.
4. Cover 48×1, 48×2, 64×2, 48×1.6×1.5, invalid values, very large values, and overflow-prone inputs before wiring the provider to the type.

Verification gate: build the shared test target and run the request cases. The new cases must fail before the production implementation is added and pass once the contract exists.

## Stage 2: Consolidate Qt theme/search-path configuration

Files:

- Modify `shared/icons/AstreaIconTheme.hpp`.
- Modify `shared/icons/AstreaIconTheme.cpp`.
- Extend `shared/tests/AstreaIconProviderTest.cpp` with process-global state guards and theme-root fixtures.

Implementation:

1. Preserve the existing theme-source priority: `ASTREA_ICON_THEME`, `QS_ICON_THEME`, qt6ct, platform theme, WhiteSur-dark compatibility, then hicolor.
2. Refactor search-root discovery into a deterministic helper that accepts test data locations and a home path. Include XDG data roots, `~/.icons`, user Flatpak exports, and system Flatpak exports; deduplicate while preserving priority.
3. Make `apply()` merge Astrea roots with the current `QIcon::themeSearchPaths()` and `QIcon::fallbackSearchPaths()` rather than replacing them. Preserve native/resource paths and set the selected theme and hicolor fallback after the merge.
4. Keep `resolveWithSource()` as a compatibility/query helper only; remove its role as the provider’s primary raster resolver.
5. Add an RAII guard that saves and restores theme name, fallback theme name, theme search paths, fallback search paths, and relevant environment variables for every test process.

Verification gate: tests prove current-theme preference, inheritance, hicolor fallback, Flatpak roots, merged paths, and restoration of process-global state.

## Stage 3: Make `AstreaIconProvider` resolution-aware

Files:

- Modify `shared/icons/AstreaIconProvider.hpp`.
- Modify `shared/icons/AstreaIconProvider.cpp`.
- Modify `shared/CMakeLists.txt` if the provider’s source list needs the new header.
- Extend `shared/tests/AstreaIconProviderTest.cpp`.

Implementation:

1. Remove the manual XDG directory-order resolver from the provider (`lookupXdgTheme()`, hand-built subdirectory/extension/prefix selection, and duplicate inheritance traversal). Retain only narrowly scoped direct-file and `/usr/share/pixmaps` compatibility needed by existing callers.
2. Parse `logicalSize`, `dpr`, `pixelSize`, `revision`, and legacy `size` query parameters. Recompute the authoritative physical target from the normalized logical size and DPR; treat `pixelSize` as a checked diagnostic field.
3. Resolve named icons through `QIcon::fromTheme()` and `QIcon::pixmap(QSize(logical, logical), dpr, QIcon::Normal, QIcon::Off)`. Retry a basename for extension-bearing identities where appropriate.
4. Preserve direct absolute file support. Normalize returned pixmaps to their raw physical extent without scaling a smaller representation upward. Allow scalable sources such as SVG to rasterize at the requested target and allow Qt to downsample a larger raster.
5. Include identity, logical extent, quantized DPR, physical extent, and theme revision in positive-cache keys. Keep the existing bounded positive/negative caches, calculate positive cost from actual image bytes when available, and invalidate both caches on theme/search-path changes.
6. Keep provider refresh and cache revision behavior intact, including file/directory watcher invalidation.

Verification gate: the dedicated provider test uses distinguishable 48, 96, and 128 pixel assets and verifies that the correct representation is selected, no smaller asset is upscaled, DPR and logical-size requests have distinct results, SVG is crisp at the target, named/direct/missing/extension-bearing cases behave as expected, cache keys differ, and invalid input stays bounded.

## Stage 4: Move source-quality policy into shared QML

Files:

- Modify `shared/qml/AstreaAppIcon.qml`.
- Modify `shared/CMakeLists.txt` only if the QML module source declaration changes.

Implementation:

1. Add `maximumPresentationScale`, `maximumPresentationLogicalSize`, and `devicePixelRatioOverride` properties plus the read-only effective DPR, maximum logical size, source pixel size, and resolved source properties.
2. Prefer an explicit positive maximum logical size; otherwise calculate `iconSize * maximumPresentationScale`.
3. Read `Screen.devicePixelRatio` by default and use the override for deterministic tests. Bound and quantize values consistently with `IconRenderRequest`.
4. Calculate the source target as `ceil(effectiveMaximumLogicalSize * effectiveDevicePixelRatio)`, bounded to the provider maximum.
5. Encode logical size, DPR, physical size, provider revision, and retry nonce in named-icon URLs while retaining direct paths/schemes and existing fallbacks.
6. Set the image `sourceSize` from `effectiveSourcePixelSize` and retain `smooth`, `mipmap`, `asynchronous`, cache, fallback, and existing opacity-mask behavior.

Verification gate: add or extend QML tests for 96, 192, 256, and 116 pixel targets, explicit maximum-size precedence, DPR override behavior, finite-value bounds, and stable source URL/source-size values when only presentation scale changes.

## Stage 5: Migrate presentation consumers

Files:

- Modify `Dock/qml/components/DockAppDelegate.qml`.
- Modify `AltTab/qml/components/AltTabWindowDelegate.qml`.
- Modify `Spotlight/qml/components/SpotlightResultsList.qml`.
- Modify `Dock/tests/DockHoverQmlTest.cpp` without overwriting its concurrent timeout change.
- Modify `AltTab/tests/AltTabQmlSelectionTest.cpp` if delegate inspection is needed.
- Add or extend a shared/consumer QML test for the Spotlight 40 logical-pixel policy.

Implementation:

1. Dock replaces `sourcePixelSize: root.iconSize * 2` with the configured maximum presentation scale. Keep `magnificationScale` solely on the presentation transform. Verify the existing geometry, hit testing, drag, reorder, context-menu, and runtime paths remain untouched.
2. AltTab keeps the 72-to-84 selected presentation animation but uses one maximum logical source target of 84 for both states. Selection changes presentation only.
3. Spotlight uses a maximum logical target of 40 and the effective DPR instead of a fixed 80-pixel source.
4. Update test discovery from the removed fixed `sourcePixelSize` property to `effectiveSourcePixelSize` or a stable object identity. Preserve the user’s unrelated test timeout edit.

Verification gate: Dock proves hover changes visual scale while source URL and effective source size stay unchanged; DPR changes update the quality target once; AltTab proves selection width changes without a source reload; Spotlight proves 40×DPR.

## Stage 6: Documentation and runtime evidence

Files:

- Update the relevant English documentation in `Dock/docs/ARCHITECTURE.md`, `Dock/docs/RUNTIME_FLOW.md`, `Dock/docs/TESTING.md`, `AltTab/docs/ARCHITECTURE.md`, `AltTab/docs/RUNTIME_FLOW.md`, and `Spotlight/docs/RUNTIME_FLOW.md` as needed.

Documentation must describe the shared provider/QIcon path, logical-versus-physical sizing, theme-path merge, cache invalidation, and the separation between source quality and presentation animation. Do not document unsupported live behavior as verified.

Run bounded live startup/visual checks under the existing Wayland session only when the shell process is not already user-owned. Record any inability to produce reliable pointer-hover evidence and any low-resolution limitation.

## Stage 7: Verification and integration

Run, using `rtk`:

1. `git diff --check` and focused source searches for stale fixed source-size policies or the removed manual provider resolver.
2. Debug builds and focused tests for `astrea-icon-provider-test`, Dock hover QML, AltTab QML selection, Spotlight, and the shared QML/plugin targets.
3. Release builds of the affected targets.
4. QML lint with the repository’s Qt 6 tool path.
5. High-DPI test runs at 1x, 1.5x, and 2x where the test harness supports `QT_SCALE_FACTOR`.
6. Relevant broader `ctest` suites, recording unrelated failures separately.
7. Final worktree/diff review to ensure only intended icon-pipeline files are staged and no Typhon file changed.

Use interactive/path-specific staging for files containing concurrent edits. Commit the implementation after all feasible verification passes with a focused message, and report the exact commit, commands, results, high-DPI coverage, live result, unverified cases, and remaining low-resolution limitations.
