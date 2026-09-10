# Eclipse Animation Control Plane v1 Integration Plan

## Constraints and checkpoints

- Preserve all existing dirty Settings, shared, Dock, and user documentation files.
- Reuse the existing configured Eclipse build directory; do not create a build variant.
- Keep `WindowId` and protocol-handle mapping in C++.
- Keep the shared protocol XML semantically identical to Typhon’s copy.
- Add focused tests before implementation seams and use the repository’s current CMake/test workflow.

## 1. Add native control transport

Implement a focused reusable Qt Typhon control client in the shared Typhon
layer only if its dependency remains appropriate; otherwise keep the helper in
Settings. Use `QLocalSocket`, bounded newline framing, request IDs, strict
protocol/version and success/error validation, timeout handling, and secure
runtime-instance discovery mirroring `astreactl`. Add temporary-directory and
local-server tests for framing, limits, malformed responses, mismatches,
errors, timeouts, secure selection, and ambiguity. Register sources in the
existing CMake target without a new build directory.

## 2. Add SettingsAnimationController

Create the controller under Settings services, expose it as the
`SettingsController.animations` property, and make it own a native control
client. Project authoritative snapshots into QML-facing properties and slot /
preset capability lists. Serialize one mutation at a time, debounce speed
changes, flush on release, restore the last confirmed snapshot on failure, and
adopt the returned snapshot on success. Add controller tests for refresh,
mutations, overrides/reset, planned-effect rejection, unavailable transport,
busy coalescing, and authoritative projection.

## 3. Add navigation, page, and i18n

Register the `animations` child page under Customization, add a clean vector
icon through the existing resolver, add the QML source to the existing QML
module list, and add English source strings to the current catalog without
overwriting unrelated edits. Build `pages/appearance/Animations.qml` from the
existing Form cards/rows/toggle/selector/slider. Bind all controls to the
controller, derive options from the authoritative catalog, show Lamp as
planned with effective None, and provide explicit reset/restore actions. Remove
the fake Compositor-local `animationsEnabled` ownership and replace it with a
navigation row if the page layout supports it. Add static and QML smoke
coverage for routing, bindings, page load, no duplicate local state, and
planned-effect honesty.

## 4. Extend the shared v3 protocol client

Copy the Typhon XML extension byte-for-byte semantically into
`shared/platform/typhon/protocols/`, regenerate through the existing CMake
workflow, and add typed anchor methods to the protocol adapter and
`TyphonToplevelConnection`. Gate publication on authenticated manager version
3, retain reconnect semantics, and report bounded transport/protocol errors.
Add shared protocol contract and integration coverage for v2 read/action
compatibility and v3 anchor requests.

## 5. Add stable Dock anchor geometry and projection

Extend `DockSurfaceGeometry` with the resting unscaled icon calculation and
global output-origin conversion. Reuse current placement formulas and test
bottom/left/right edges, positive/negative origins, icon size, reorder, and
hover-scale invariance. Add DockController cache/publication methods keyed by
desktop file name; project every current runtime `WindowId`, publish cached
anchors to newly appearing windows, clear local publication bookkeeping for
removed windows/disconnects, and republish after reconnect. Coalesce identical
rectangles. Add controller/model/runtime tests for multi-window app
projection and association changes.

## 6. Wire structural QML events only

Pass output origins into DockPanel/delegates. Have delegates report stable
desktop-file identity and resting icon rectangles through DockController.
Publish on structural geometry/model/output changes and delegate insertion or
removal. Do not use `interactionRegion`, visual scale, hover, pointer, lift,
or drag values. Ensure the new QML path never enumerates `WindowId`s.

## 7. Verify and commit

Use the existing build cache and run focused Settings core/controller tests,
Animations QML smoke/static tests, shared Typhon protocol tests, Dock geometry,
controller, and runtime integration tests, followed by the normal broader
suite when practical. Run configured QML lint/type checks if present. Review
the diff for unrelated files, commit the Eclipse implementation as one focused
change, and report any unrelated pre-existing failures separately.
