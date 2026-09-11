# Astrea Shell Exact-Region Frosted Blur Acceptance Design

## Goal

Harden Eclipse's client-region rectangle budget into a real `<= 96` invariant while preserving exact separate Shell blur regions and the existing same-frame animation synchronization path, then run fresh Eclipse and Typhon deterministic gates followed by native Frosted Shell acceptance.

## Scope and constraints

- Modify only the bounded Eclipse region-budget behavior and its regression unless fresh verification exposes a concrete regression elsewhere.
- Preserve the current split: ordinary invalidation queues `sync(true)`, while `QQuickWindow::afterAnimating` uses `Qt::DirectConnection` and `sync(false)`.
- Keep the existing four-card test proving separate high-radius regions.
- Add an impossible-budget regression with at least 33 non-deduplicating rounded descriptors.
- Reuse `/home/agony/GitHub/Eclipse/build` and `/home/agony/GitHub/Typhon/target`; do not create alternate build trees.
- Do not change Typhon Blur Policy semantics unless its fresh tests fail because of a concrete source regression.

## Design

`AstreaBackdropEffectRegions::resolvedRegion()` will continue to collect visible region geometry and deterministically lower the rounded-corner segment limits until the generated rectangle vector fits the advertised budget or every descriptor reaches its minimum useful decomposition. If the latter state is reached while the result still exceeds 96 rectangles, the method will return an empty vector. The existing `sync()` path already derives the effect-enabled flag from `m_enabled && !region.isEmpty()`, so the empty refusal state disables the effect for that update without sending an oversized protocol region. The bounded diagnostic, if needed, will be emitted at the synchronization boundary with rate limiting rather than once per rectangle or frame.

No arbitrary descriptor dropping, bounding-box merging, fullscreen conversion, or animation-path changes are allowed. A normal four-card layout remains represented by separate rectangles and remains below the limit.

## Tests

The new regression will construct 33 separate rounded descriptors with non-overlapping geometry so deduplication cannot hide the budget pressure. It will verify both that `resolvedRegion()` is empty in the refusal case and that the exposed rectangle count is never greater than 96. Existing tests continue to cover hidden-window resynchronization, ancestor scale and placement changes, unchanged-frame deduplication, and separate four-card geometry.

Fresh deterministic verification will use the already configured Eclipse build and requested `ctest`/focused targets, then the requested Typhon `rtk` commands and focused policy/renderer/output tests. Source checks will confirm no raw `wl_surface_commit()` or `wl_display_connect()` in `shared/platform/wayland/effects` and that the aggregate region output is bounded.

Native acceptance will run the current Typhon and Eclipse Shell with `shellStyle = Frosted`, checking surface existence, exact geometry, same-frame animation behavior, and state transitions. If blur is completely absent, one Dock surface will be instrumented at the specified Eclipse, Typhon protocol, assignment, and effects boundaries; diagnostics will be removed or gated after the first disappearing boundary is identified.

## Success criteria

The Eclipse build and focused/full test gates pass; the Typhon deterministic gates pass with fresh counts; the exact-region budget test proves the hard bound and safe refusal; and native Frosted Shell blur is visible with the requested exact-region, animation, and state checks passing. The result will not be described as full Liquid Glass support or as a 1080p165 performance qualification.
