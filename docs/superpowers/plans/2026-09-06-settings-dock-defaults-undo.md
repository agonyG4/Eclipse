# Settings Dock Defaults, Detents, and Undo

## Scope

Extend the existing Settings Dock personalization page with canonical default detents, transient value feedback, Restore Defaults, and one bounded Undo action. Keep `DockConfig::defaults()` as the only source of canonical values, keep `DockConfigStore::writePersonalization()` as the persistence boundary, and preserve native `QQC2.Slider` input ownership.

## Implementation steps

1. Add controller contract tests first.
   - Verify the nine typed default properties are derived from `DockConfig::defaults()`.
   - Verify `isDefault` transitions only on logical personalization changes and ignores pins.
   - Verify Restore Defaults and Undo preserve pins/unknown JSON, capture unflushed drafts, handle external pin changes, converge failed writes, and expire the single undo snapshot.

2. Add Slider interaction tests first.
   - Extend the existing real `QQuickWindow` fixture for the generic no-detent, detent, hysteresis, one-shot latch, keyboard-crossing, and mirrored-marker cases.
   - Exercise native Qt events and inspect the component-level edited-value signal/state rather than introducing a test-only pointer mapper.

3. Implement controller defaults and restore state.
   - Expose nine `CONSTANT` typed defaults from a shared internal canonical-default accessor.
   - Split personalization comparison from full config comparison and publish `isDefault` with transition-only notification.
   - Extract the existing flush body into a bool-returning helper used by public `flush()`, Restore Defaults, and Undo.
   - Snapshot only the pre-restore in-memory personalization, preserve current pins, write immediately through `writePersonalization()`, and arm a four-second single-shot timer only after success.

4. Implement the reusable Slider presentation layer.
   - Add the small detent/value tooltip API and marker using the current track/handle geometry and visual mirroring.
   - Observe native `moved()` output; only pointer/touch-pressed edits may enter hysteretic detent filtering. Keyboard and wheel edits emit the native value and can cross the default.
   - Add a one-shot restrained pulse on detent entry without custom pointer mapping or effects.

5. Update Dock.qml and English translations.
   - Opt the nine existing sliders into the matching controller defaults, route writes through `valueEdited`, preserve ranges/rounding/flush-on-release, and supply compact page-level value formatting.
   - Add the quiet secondary Restore Defaults footer and local non-modal undo toast with stable object names.
   - Add only the three requested Dock action/status keys.

6. Verify and document the public behavior.
   - Update concise architecture/testing/configuration notes where needed.
   - Locate the checkout's existing configured build directory and reuse it for build, CTest, and qmllint. Do not create feature-specific or alternate Debug/Release directories; if no configured build directory exists, report that build verification cannot be performed.
   - Run the available Settings tests, qmllint, focused tests, source audits, and inspect the final diff.
   - Report the exact executable and leave manual light/dark/input/visual qualification to the user.

## Verification commands

Reuse the existing configured build directory reported by the checkout (currently `build`, whose configured type is recorded in `build/CMakeCache.txt`). Run `cmake --build <build> --parallel 4`, the complete available test suite, `cmake --build <build> --target astrea-settings-ui_qmllint --parallel 4`, the four focused tests, and source audits for the nine slider consumers, defaults, persistence boundary, and registered QML count. Do not create a new build directory or delete the existing one for fresh-build evidence.

## Execution choice

Execute inline on the current `main` checkout. No subagents, worktrees, or branch changes are used.
