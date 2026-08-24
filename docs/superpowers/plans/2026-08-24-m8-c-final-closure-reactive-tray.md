# M8-C Final Closure — Reactive Tray Projection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Close M8-C by making the native tray projection react to committed StatusNotifier presentation changes without rebuilding stable DBusMenu identity or reopening the popup.

**Architecture:** `StatusNotifierService` owns one monotonic `presentationRevision` observable and increments it through one private helper after presentation-visible state commits. Production QML reads that revision as an explicit dependency for service-backed invokable properties, while `menuClientChanged` remains the only identity-reset signal for cascade preparation. The existing DBusMenu client/model, production QRC tests, and isolated D-Bus fixture remain the authorities for menu data and lifecycle behavior.

**Tech Stack:** C++/Qt 6, Qt DBus, Qt Quick/QML, Qt Test, CMake, existing Debug/Release/Clang/sanitizer build trees, RTK shell wrapper.

## Global Constraints

- Work directly in `/home/agony/GitHub/Eclipse` and preserve unrelated dirty-worktree changes.
- Use the existing M8-C.1.4 implementation as the baseline; add no M8-C.1.5 and no unrelated architecture.
- Keep `presentationRevision` monotonic `quint64`; centralize all increments in `bumpPresentationRevision()`.
- Do not expose DBus proxies, clients, snapshots, or per-item view-model objects to QML.
- Preserve the C.1.4 visual values and interaction behavior exactly, including the 28px tooltip, 260px cap, and title fallbacks.
- Add only reactive-gap and exact-title-fallback tests; do not duplicate C.1.4 watcher, bounds, geometry, removal, or visual tests.
- Use the existing build directories; do not create generated build artifacts in the commit.
- Run commands through `/home/agony/.local/bin/rtk run` and edit files with `apply_patch`.
- Create one coherent commit after `git diff --check` and fresh verification.

---

### Task 1: Establish the scoped plan and RED regression coverage

**Files:**
- Create: `docs/superpowers/plans/2026-08-24-m8-c-final-closure-reactive-tray.md`
- Modify: `shared/tests/StatusNotifierTest.cpp`
- Modify: `Bar/tests/BarQmlSmokeTest.cpp`

**Interfaces:**
- Tests consume `StatusNotifierService::presentationRevision()`, the production `displayTitleForItem()` invokable, the existing `upsertTestItem()`/`removeTestItem()` fixture seam, and production QRC components.
- Tests produce executable proof for exact title fallback, revision monotonicity/non-mutating getters, menu identity rebind/removal/appearance, same-menu presentation stability, production header fallback, and live tooltip updates.

- [ ] Add C++ slots for the exact four-case title fallback and presentation-revision behavior. Use one valid `ItemSnapshot` key, keep a stable generation for same-menu updates, assert ordinary invokable/getter calls leave the revision unchanged, assert snapshot/title/menu/item-removal changes advance it, and assert the menu model/client identity changes only for a menu-path replacement.
- [ ] Add production-QRC QML smoke slots that open one tray popup and verify `/MenuA -> /MenuB`, menu removal, menu appearance, same-menu title/icon/tooltip updates, and all four header title fallback cases while the popup stays open.
- [ ] Extend the existing tooltip production-QRC test to show once, update the same item live, verify an empty effective tooltip hides the card, then restore it without changing `itemKey`.
- [ ] Build and run the focused test targets before production edits; confirm the new tests fail for the missing property/reactive bindings/fallback, not because of a test error.

### Task 2: Add centralized StatusNotifier presentation revision

**Files:**
- Modify: `shared/statusnotifier/StatusNotifierService.hpp`
- Modify: `shared/statusnotifier/StatusNotifierService.cpp`

**Interfaces:**
- Produce `Q_PROPERTY(quint64 presentationRevision READ presentationRevision NOTIFY presentationRevisionChanged)` and `quint64 presentationRevision() const`.
- Produce private `void bumpPresentationRevision()` and `void presentationRevisionChanged()`.

- [ ] Add the property, accessor, signal, and private helper without changing protocol/model authority.
- [ ] Bump after committed ready snapshot updates, effective icon-store updates, menu client create/remove/replace, DBusMenu `changed` notifications, item removal, and stop’s final empty projection. Do not bump for watcher health-only changes or ordinary reads.
- [ ] Preserve existing `itemChanged`, `itemRemoved`, `menuClientChanged`, `menuContentChanged`, and `stateChanged` semantics and ordering, ensuring revision is observable before the corresponding semantic signal is consumed by QML.
- [ ] Implement the exact display title fallback: `tooltipTitle || title || id || "Tray item"`; keep tooltip fallback limited to `tooltipTitle || title || ""`.

### Task 3: Rebind production QML to the service revision

**Files:**
- Modify: `Bar/qml/TrayContextMenu.qml`
- Modify: `Bar/qml/TrayTooltipSurface.qml`

**Interfaces:**
- `TrayContextMenu` produces `serviceRevision`, `remoteMenuState`, revision-backed `menuModel`, and revision-backed `hasRemoteMenu`; `menuClientChanged` remains the identity reset hook.
- `TrayTooltipSurface.tooltipTitle` explicitly depends on the service revision and remains input-transparent with the C.1.4 visual contract.

- [ ] Remove `presentationSerial` and make all service-backed invokable bindings explicitly read `serviceRevision` before lookup.
- [ ] Use `remoteMenuState` for no-actions and pending-cascade logic so Loading/Ready/Empty/Error changes update in place.
- [ ] Keep same-menu presentation updates from resetting cascades or calling `AboutToShow`; only menu identity changes reset and prepare.
- [ ] Add narrow object names needed by production-QRC assertions without exposing new service internals.
- [ ] Keep the tooltip visual dimensions, colors, font, positioning, and fallback behavior unchanged while allowing live title updates and empty-card hiding.

### Task 4: Run the qualification matrix and write the final report

**Files:**
- Create: `docs/M8_C_FINAL_CLOSURE_REPORT.md`

**Interfaces:**
- The report records the exact commit baseline, revision architecture, reactive QML behavior, title/tooltip rules, test counts excluding init/cleanup, isolated D-Bus results, build configurations, sanitizer caveats, manual qualification status, and explicit Not Run reasons.

- [ ] Run focused targets in the required order: `statusnotifier-test`, `statusnotifier-dbus-integration-test`, `bar-core-test`, `bar-qml-smoke-test`, `bar-qml-legacy-guard`.
- [ ] Run the same focused targets in existing Release, Clang, no-Typhon, no-layer-shell, UBSan, and ASan trees as available; report the known external NVIDIA/LSAN issue honestly if it recurs.
- [ ] Run the broad Debug suite and distinguish feature failures, stale/missing unrelated targets, and unrelated dirty-worktree failures from this closure.
- [ ] Run `git diff --check`, inspect the final diff for scope, and verify production QML contains no forbidden legacy process/helper APIs.
- [ ] Mark manual visual qualification and real third-party applications Not Run when no reliable live display/fixture evidence is available; do not claim them tested.

### Task 5: Stage and commit the closure

**Files:**
- Stage only the StatusNotifier/QML/test changes, the new plan, and `docs/M8_C_FINAL_CLOSURE_REPORT.md`.

- [ ] Confirm unrelated Settings, Shell, Paper, Bench, CI, and other user changes remain unstaged.
- [ ] Create one commit with message `M8-C: final reactive tray projection closure`.
- [ ] Record the commit hash and final M8-C status only after fresh verification succeeds.

## Self-review

- The plan covers every acceptance category in the supplied M8-C final-closure spec: one revision source, all visible state transitions, all QML dependencies, stable same-menu identity, exact header/tooltip fallbacks, production-QRC regression coverage, matrix qualification, legacy guard, sanitizer caveat, source scope, report, and one commit.
- No new fixture architecture or production test-only setters are introduced; existing test seams and the C.1.4 D-Bus fixture remain in use.
- There are no placeholder steps or unspecified file responsibilities.
- The revision accessor is a `quint64`, QML reads it through a `var` dependency, and menu identity remains represented by the existing `DBusMenuClient`/`DBusMenuModel` objects.
