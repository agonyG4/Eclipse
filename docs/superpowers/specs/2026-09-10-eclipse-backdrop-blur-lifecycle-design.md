# Eclipse Backdrop Blur Lifecycle Repair

## Goal

Make existing Shell `QQuickWindow` backdrop regions synchronize across mapping, native surface recreation, capability changes, and ancestor animation while preserving the current exact QML region assignments.

## Design

`AstreaBackdropEffectRegions` observes the attached `QQuickWindow` at the lifecycle boundary. It reconnects `visibleChanged`, `widthChanged`, `heightChanged`, `afterAnimating`, and `AstreaWaylandEffects::availableChanged` whenever the target window changes, and installs an event filter for `QPlatformSurfaceEvent::SurfaceCreated` and `SurfaceAboutToBeDestroyed`. The old window is unfiltered before replacement. Hidden windows clear the active binding; visible windows schedule one queued sync. Surface destruction releases the effect before native teardown, and surface creation schedules a fresh resolution/reapply on the new native surface.

The existing binding remains the sole owner of the protocol proxy. The existing-window sync passes `requestFrame=true`, so a changed protocol state requests one Qt update and lets Qt/QPA perform the commit. Binding-side active-region deduplication prevents a continuous update loop. `afterAnimating` only schedules geometry recomputation; it does not request extra frames.

`AstreaBackdropRegion` stores its item as a `QPointer<QQuickItem>`. Item destruction therefore nulls the source safely, and the controller receives a destruction notification so the next sync clears the region. The current descriptor property connections remain for static changes; the window animation callback covers ancestor transforms and placement without duplicating QML geometry formulas.

## Tests

Expand the existing deterministic test seam for hidden-to-visible resolution, native surface generations, one-update commit requests, ancestor scale and position changes after the normal animation callback, and dynamic item destruction. Keep the existing Wayland lifecycle-state test and current QML exact-region mappings unchanged.

