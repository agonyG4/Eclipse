# P0 Spotlight Layer-Shell Crash Closure Design

**Goal:** Determine and close the actual cause of Spotlight's full remap and Astrea-menu-to-Spotlight crash paths without hiding the lifecycle defect or disturbing unrelated Typhon work.

**Scope:** The current Eclipse checkout at `eb9d7e47d80feaa86bc91365753a7c56458df084` and the current Typhon checkout at `9d3fb34b45f6ce4ffc4582c3231e220b3643e959`, with all pre-existing Typhon worktree changes preserved. The implementation may produce one focused Eclipse commit, one focused Typhon commit, or one in each repository only when independent contributing defects are demonstrated.

## Observed starting state

- Eclipse is clean and uses an Exclusive, fullscreen Overlay LayerShellQt Spotlight surface.
- Spotlight currently requests activation and search-field focus from both `Window.onVisibleChanged` and the controller's deferred `focusRequested` signal.
- Spotlight sets `surfaceVisible` false only after its 190 ms close timer, so a completed close is a real native unmap.
- The Astrea menu starts its visual close with `popupController.close()` while `surfaceRequired` remains true until the animation calls `completeClose()`; `BarController::showSearch()` currently opens Spotlight immediately.
- Current Typhon source resets `initial_configure_sent`, `initial_configure_acknowledged`, pending configure state, and Exclusive focus selection on layer-surface unmap. This is evidence to verify, not proof that the live dirty checkout is correct for every remap edge case.
- Typhon contains unrelated dirty changes, including changes near compositor focus and layer-shell code. They are user-owned and must not be reset, restored, stashed, cleaned, overwritten, or staged.

## Investigation before implementation

The first phase is read-only and evidence-producing:

1. Record both repository baselines, status, diff statistics, and recent history.
2. Trace the Eclipse Spotlight, Alt+Tab, Bar popup, Shell IPC, and LayerShellQt lifecycle paths.
3. Trace current Typhon layer-surface commit, configure/ACK, unmap, destroy, scene publication, and keyboard-focus paths.
4. Reproduce the user paths with bounded diagnostics, capturing shell PID/exit status, stderr, Typhon logs, supervisor behavior, and a small causal `WAYLAND_DEBUG=client` sequence where available.
5. Run controls for reopen during the close animation, Alt+Tab, Bar popup, weather-disabled Spotlight, and empty-query/no-result Spotlight.

The final report must distinguish a native crash, Qt assertion, Wayland protocol error, disconnect, supervisor action, explicit exit, or another concrete termination cause. A timing change without a causal trace is not sufficient.

## Candidate authority boundaries

The implementation decision is evidence-driven:

- **Typhon-only:** If the real protocol client or live trace proves that remap state, configure serials, stale ACKs, buffer admission, destruction, or Exclusive focus violate the layer-shell contract, fix the existing Typhon state authority and add protocol/focus regressions there.
- **Eclipse-only:** If the live protocol is valid but Spotlight's duplicate activation, transitional focus loss, stale close callback, or open-generation lifecycle causes the failure, fix the smallest Spotlight controller/QML lifecycle boundary and add controller plus real-shell coverage.
- **Independent dual defect:** If both traces prove separate failures, fix each at its authority boundary in one focused commit per repository. Do not use popup serialization to mask a broken ordinary Spotlight remap.
- **Popup handoff:** Serialize Astrea menu Search only if overlapping mapped popup and Exclusive Spotlight surfaces are independently proven unsafe. Use popup lifecycle completion, not an arbitrary timer or sleep, and keep Settings and other menu actions unchanged unless evidence requires otherwise.

## Intended lifecycle invariants

When applicable to the proven cause:

- A layer-surface NULL-buffer unmap clears mapping eligibility and requires a fresh bufferless commit, configure, ACK, and mapped-buffer commit.
- Configure serials are generation-safe; stale or unknown ACKs cannot authorize a later remap, and destruction invalidates pending state.
- No unmapped or destroyed layer surface owns keyboard focus or receives input.
- Spotlight has one open generation, one activation owner, and one deterministic search-field focus request.
- A focus-loss close is armed only after the current generation becomes active; transitional activation loss during opening is not treated as click-away.
- A stale close-animation callback cannot hide a later open generation.
- Spotlight is genuinely unmapped after a completed close; it is never kept as an invisible fullscreen Exclusive surface.

## Regression and qualification strategy

Add the smallest failing regression before each production fix. The Typhon protocol regression must use the existing real Wayland test-client infrastructure, exercise at least 100 map/unmap/remap cycles, and cover negative pre-configure/stale-ACK/destroy/disconnect cases when Typhon is implicated. A two-surface Overlay None → Overlay Exclusive handoff must verify focus and resource lifetime.

Eclipse coverage must include the close/reopen generation contract, the real LayerShell state machine when a runnable session exists, the Astrea popup Search handoff, Alt+Tab and Bar popup controls, weather-disabled and search-backend controls, and the existing Spotlight/Shell/Bar/legacy test gates. No giant raw wire log is committed; the report contains only the bounded causal sequence and explicitly marks unavailable live DRM/KMS or third-party qualification as Not Run.

## Change and commit discipline

- Preserve all unrelated dirty changes in both repositories.
- Stage only P0 evidence, fixes, tests, and English documentation belonging to the responsible repository.
- Verify `git status`, `git diff`, and `git diff --cached` immediately before each commit; the staged diff must contain no unrelated file or hunk.
- Do not touch Dock, Control Center, StatusNotifier, DBusMenu, Dwindle, KMS/O1, direct scanout, or other compositor behavior without direct causal evidence.
