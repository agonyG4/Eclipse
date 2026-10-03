# Progress: Dock App Tooltip

Ruling: stay on `main` and do not create a worktree or branch — the repo instructions require `main` and prohibit branches — cost if wrong: changes share the current checkout.

Ruling: execute inline and self-review the final diff — the repo instructions prohibit subagents — cost if wrong: no independent fresh-context reviewer.

Ruling: use the approved investigation design as the spec authority — the user approved the architecture and implementation plan — cost if wrong: the implementation may need a small design correction.

## Tasks

- [x] Task 1: output-local tooltip geometry and passive Layer Shell policy.
- [x] Task 2: themed tooltip surface and Dock presentation wiring.

Verification: `astrea-shell` built from `/mnt/Aether/Desktop/GitHub/Eclipse-build`; all 14 Dock CTest cases passed, including magnification tracking, Dock interaction suppression, placement fallback, and Layer Shell policy.

## Tooltip lifecycle race hardening (2026-10-02)

- [x] Confirmed the stale callback race before implementation: the geometry-refresh and `modelChanged` regressions both presented replacement target B before its configured delay.
- [x] Split ownership between the 420 ms initial-presentation timer and lifecycle-qualified visible refreshes. Each refresh captures lifecycle generation and task key, coalesces matching requests, and revalidates generation, key, surface existence/visibility, and eligibility before resolving the current target.
- [x] Added deterministic regressions for geometry-triggered and model-triggered stale refreshes; both fail against the old implementation and pass with the lifecycle guard.
- [x] Verified width and height changes produce one settled re-presentation, retained the pending replacement/removal and magnification/interaction regressions, and passed the focused Dock hover QML test.

## Unified tooltip visual hardening (2026-10-03)

- Replaced the rounded card plus rotated-square pointer with one `QtQuick.Shapes` vector bubble: a 72–280 px wide, 34 px high pill body with a 14 px wide, 8 px deep integrated tail for all four edges. The body radius is 17 px, and the closed silhouette owns a single 1 px outline and one soft shadow.
- Kept `DockSurfaceGeometry` as the shared authority for body height, pointer dimensions, and the 3 px pointer-tip target gap. Placement uses 11 px from the body edge to the anchor and clamps the pointer center at `min(edgeLength / 2, bodyHeight / 2 + pointerWidth / 2)` so the tail base clears the pill corner arc.
- Added semantic tooltip surface, border, and shadow colors. Frosted uses a moderate 0.44–0.46-alpha theme-derived tint over compositor blur; Transparent retains its lighter existing-style tint without enabling blur; Default retains the elevated fill.
- Kept the backdrop source as a non-rendering rounded body proxy. The tail shares the translucent visual fill but has no separate glass layer; arbitrary-shape tail blur is outside the current backdrop-region protocol.
- Updated deterministic geometry and QML regressions. `dock-hover-qml-test`, `dock-surface-geometry-test`, and `bar-qml-smoke-test` passed (3/3); complete Dock CTest passed (14/14). `astrea-shell` and all requested targets built in `/mnt/Aether/Desktop/GitHub/Eclipse-build`; `git diff --check` passed.
- Native visual qualification was not performed: no running Astrea Shell or Typhon process was available, and desktop automation could not attach. The body/tail material junction therefore remains visually unqualified on a live compositor.
