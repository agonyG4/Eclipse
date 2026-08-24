# Test and Validation Plan

The port is not complete until the following gates are covered.

## 1. Pure unit tests

### BarSurfacePolicy

Test exact Layer Shell policy values:

- reserve: Top, top+left+right, exclusive 45;
- launcher: Top, top+left, margins left 8/top 5, exclusive -1;
- status: Top, top+right, margins right 6/top 5, exclusive -1;
- popup overlay: Overlay, all anchors, exclusive -1.

### Bar geometry

For representative logical widths such as 1024, 1280, 1920, 2560, and 3440:

- launcher and status regions never overlap;
- minimum intended gap is respected when both regions fit;
- status width is capped rather than pushed off-screen;
- popup `anchorX` clamps card geometry inside side padding.

### WorkspaceModel

With fake data:

- numeric ordering is deterministic;
- active state changes exactly one intended item;
- role updates emit correct model notifications;
- empty production data is valid;
- negative/special IDs are not assumed unless the future protocol defines them.

### BarClockService

Test:

- minute-boundary update calculation;
- stable time formatting;
- day/month formatting under at least `en_US` and the currently supported native locale path;
- no child process/file dependency.

### BarPopupController

Test:

- closed -> open;
- one popup replaces another;
- close is idempotent;
- state is isolated by output/bundle identity;
- output teardown clears active popup state.

## 2. QML smoke/static tests

Load every new Bar QML component with fake/native context objects in an offscreen or test engine where possible.

Add a static source test that fails if production Bar QML contains prohibited legacy imports/commands from `docs/05_LEGACY_REPLACEMENT_MATRIX.md`.

Verify the Astrea logo is a compiled resource and does not require `ASTREA_ROOT` file paths.

## 3. Application integration tests

Verify:

- `ShellRuntime` owns exactly one BarController/ClockService;
- Search menu action routes to the existing SpotlightController;
- Settings action uses the existing launcher/catalog path when the desktop entry is present;
- shell status JSON reports Bar enabled/surface count/popup state without exposing unstable pointers;
- disabling/destroying Bar state does not stop Dock, Alt+Tab, Spotlight, or Typhon connection.

## 4. Layer Shell/live compositor qualification

This requires a real Wayland compositor exposing `zwlr_layer_shell_v1`.

On a single output:

- verify 45 px top exclusive zone;
- verify reserve surface click-through;
- verify visible pill geometry;
- verify popup overlay outside-click behavior;
- verify popup is unmapped when closed;
- verify no ordinary Qt titlebar/window fallback can appear.

On two outputs when available:

- one complete Bar appears on each;
- each reserves only its own output;
- popup opens on the output whose indicator was clicked;
- unplug/replug does not duplicate surfaces;
- changing primary output does not move the other Bar.

## 5. Regression commands

Use the repository's existing presets/build conventions. Reuse a single build directory per configuration instead of repeatedly creating fresh build trees.

At minimum run the equivalent of:

```text
cmake --build <existing-build-dir>
ctest --test-dir <existing-build-dir> --output-on-failure
```

Run the existing CI-equivalent formatting/static/sanitizer gates that are available in the repository.

Do not report success based only on the new Bar tests. Existing Shell/Dock/AltTab/Spotlight/shared tests must remain green.

## 6. Stress cases

At minimum exercise repeated cycles of:

- popup open/close 100 times;
- screen bundle create/destroy in the lifecycle model 100 times;
- Bar enable/disable where the runtime supports it;
- Spotlight opening from the Astrea menu while other shell surfaces are already active.

Check for duplicate signals, leaked windows, stale `QScreen` pointers, and Layer Shell configuration performed after first map.

## 7. Completion evidence

The agent's final report must include:

- files changed;
- architectural decisions actually implemented;
- test commands and exact pass/fail results;
- live Wayland qualification performed or explicitly not performed;
- any dependency/runtime limitation that prevented a test;
- remaining follow-up items, without claiming M8-B+ functionality was completed.
