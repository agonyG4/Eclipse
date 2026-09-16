# Eclipse Runtime Task-Key Lifetime Stability Implementation Plan

> **For agentic workers:** Execute this plan inline with `superpowers:executing-plans`; subagents are prohibited by the user. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve each live Typhon runtime task's `taskKey` across catalog and metadata changes, reconcile matching pins onto that key, and retain exact runtime actions and anchors.

**Architecture:** Add a stateful `RuntimeTaskIdentityTracker` owned by `DockController`, keyed by current-generation `WindowId` assignments and live non-empty app-ID cohorts. Keep `DockApplicationStateProjector` pure by passing assignments into it, and make `DockAppModel` choose live runtime keys for configured pins by launcher filename.

**Tech Stack:** C++20, Qt 6 Core/Test/Qml, CMake presets, Qt model signals, Typhon snapshot types, existing Dock test binaries.

## Global Constraints

- Build artifacts stay in the repository's existing build directories.
- Do not modify Typhon protocols or any Typhon source outside the Dock/shared consumer changes required here.
- Do not add Steam-, Proton-, Wine-, or `steamwebhelper`-specific Dock identity behavior.
- Do not persist `taskKey`; persist only configured `desktopFileName` values.
- Exact runtime actions remain `taskKey + WindowId` validated against authoritative live state.
- Minimize anchors remain keyed by the stable live `taskKey`.
- Do not use titles or PIDs as task identity, and do not make `AppIdentityResolver::stableKey` authoritative.
- Use TDD: regression tests must fail before production implementation and pass after it.
- Use `rtk` wrappers for repository commands where available, and commit each coherent checkpoint.

---

### Task 1: Add the red regression coverage

**Files:**
- Modify: `shared/tests/DockApplicationStateProjectorTest.cpp`
- Modify: `Dock/tests/DockAppModelTest.cpp`
- Modify: `Dock/tests/DockControllerTest.cpp`
- Modify: `Dock/tests/DockTyphonRuntimeIntegrationTest.cpp`

**Interfaces:**
- Consumes: existing `DockApplicationStateProjector`, `DockAppModel`, `DockController`, and fake Typhon adapter APIs.
- Produces: failing tests that describe stable assignment, pin reconciliation, lifecycle reset, helper lookup, exact actions, and anchors.

- [ ] **Step 1: Add projector-level lifetime cases**

Extend `DockApplicationStateProjectorTest` with a test helper that creates a
catalog containing a selected desktop record and snapshots with an explicit
`connectionGeneration`. Add tests named
`catalogGainKeepsLiveTaskKey`, `catalogLossKeepsLiveTaskKey`,
`metadataChangeKeepsLiveTaskKey`, `newWindowJoinsExistingAppCohort`,
`generationChangeStartsFreshTaskLifetime`, `authorityResetStartsFreshTaskLifetime`,
and `emptyAppIdWindowsRemainSeparate`. Exercise the future tracker-backed
projector path through the public projector/tracker interface and assert that:

```cpp
QCOMPARE(first.states.keys(), QStringList{QStringLiteral("app:late-app")});
QCOMPARE(afterCatalog.states.keys(), QStringList{QStringLiteral("app:late-app")});
QCOMPARE(afterCatalog.states.value(QStringLiteral("app:late-app")).desktopFileName,
         QStringLiteral("late.desktop"));
```

The metadata test must retain the original key after changing `appId` with the
same `WindowId` and generation. The cohort test must produce one state with two
window IDs after a second same-app window appears. The generation and reset
tests must permit a new catalog-derived key only after the old lifetime is
ended/reset. Empty-app-ID windows must remain two `window:` states.

- [ ] **Step 2: Add model pin-merge and lifecycle cases**

Extend `DockAppModelTest` with a runtime state whose key is `app:late-app` and
whose `desktopFileName` is `late.desktop`. Add tests named
`configuredPinUsesLiveStableRuntimeKey`,
`livePinnedRuntimeFallsBackToDesktopKeyAfterLastWindow`, and
`unpinnedStableRuntimeDisappearsAfterLastWindow`. Assert one row, the stable
key, `PinnedRole=true`, `desktopFileName=late.desktop`, and no
`desktop:late.desktop` duplicate while live. After applying an empty runtime
projection, assert that a pinned row is exactly `desktop:late.desktop` and an
unpinned row is absent.

- [ ] **Step 3: Add controller catalog, metadata, cohort, and persistence cases**

Extend `DockControllerTest` with a catalog helper that can contain
`late.desktop` and tests named
`catalogGainWhileLiveKeepsStableTaskKeyAndAvoidsRowChurn`,
`catalogLossWhileLiveKeepsStableTaskKey`,
`runtimeMetadataChangeKeepsStableTaskKey`,
`newSameAppWindowJoinsStableTaskAfterCatalogGain`,
`lateResolvedRuntimeCanBePinnedWithoutDuplicate`,
`newLifecycleCanUseDesktopKeyAfterOldTaskEnds`, and
`desktopFilenameLookupFindsStableRuntimeTask`. Use `QSignalSpy` on
`rowsInserted`/`rowsRemoved` around catalog rebuilds and assert zero structural
signals for in-lifetime gain/loss. Verify pin persistence receives exactly
`late.desktop`, never a task key. Verify a new snapshot after an empty
same-generation snapshot can use `desktop:late.desktop`.

- [ ] **Step 4: Add exact-action and anchor regression cases**

Extend `DockTyphonRuntimeIntegrationTest` with
`catalogRebuildKeepsExactActionsAndMinimizeAnchorOnStableTask`. Publish an
unmatched live window, record `app:late-app`, set an anchor through that key,
rebuild the catalog with `late.desktop`, publish the catalog snapshot without
changing the live window, and assert:

```cpp
QVERIFY(controller.windowsForTaskKey(oldKey).size() == 1);
QVERIFY(controller.activateWindow(oldKey, QStringLiteral("7")));
QVERIFY(controller.closeWindow(oldKey, QStringLiteral("7")));
QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(7));
```

The test must also assert the model row still has `oldKey`, and that a close or
activate request with another task key is rejected.

- [ ] **Step 5: Run the red focused tests**

Run:

```bash
rtk cmake --build --preset debug --target dock-application-state-projector-test dock-app-model-test dock-controller-test dock-typhon-runtime-integration-test
rtk ctest --preset debug --output-on-failure -R 'dock-application-state-projector-test|dock-app-model-test|dock-controller-test|dock-typhon-runtime-integration-test'
```

Expected: the new tests fail because catalog re-projection changes runtime
keys and model reconciliation still inserts both stable and synthetic pin
rows. Fix test setup errors until the failures identify those behaviors.

- [ ] **Step 6: Commit the red tests**

```bash
rtk git add shared/tests/DockApplicationStateProjectorTest.cpp Dock/tests/DockAppModelTest.cpp Dock/tests/DockControllerTest.cpp Dock/tests/DockTyphonRuntimeIntegrationTest.cpp
rtk git commit -m "test: cover dock runtime task key lifetime"
```

### Task 2: Implement sticky runtime identity and keep projection pure

**Files:**
- Create: `shared/platform/typhon/RuntimeTaskIdentityTracker.hpp`
- Create: `shared/platform/typhon/RuntimeTaskIdentityTracker.cpp`
- Modify: `shared/platform/typhon/DockApplicationStateProjector.hpp`
- Modify: `shared/platform/typhon/DockApplicationStateProjector.cpp`
- Modify: `Dock/core/DockController.hpp`
- Modify: `Dock/core/DockController.cpp`
- Modify: `shared/CMakeLists.txt`

**Interfaces:**
- Consumes: `Snapshot`, `Toplevel`, `DesktopEntrySnapshot`, and `TyphonAppMatcher`.
- Produces: `RuntimeTaskIdentityTracker::update(snapshot, catalog)` returning `QHash<QString, QString>` assignments, `reset()`, and a projector overload accepting those assignments.

- [ ] **Step 1: Add the tracker public contract and source target entries**

Declare:

```cpp
class RuntimeTaskIdentityTracker final {
public:
    QHash<QString, QString> update(
        const Snapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries);
    void reset();

private:
    std::optional<quint64> m_generation;
    QHash<QString, QString> m_windowTaskKeys;
    QHash<QString, QString> m_appTaskKeys;
};
```

Include the new `.cpp`/`.hpp` in `astrea-shared-typhon` in
`shared/CMakeLists.txt`. Include `<QHash>`, `<memory>`, and `<optional>` in the
header.

- [ ] **Step 2: Implement generation and lifetime pruning**

At the start of `update`, call `reset()` when the optional stored generation
does not equal `snapshot.connectionGeneration`, then store that generation.
Build the current live-window ID set and remove absent IDs from
`m_windowTaskKeys`. Build the set of task keys still referenced by those
assignments and remove `m_appTaskKeys` entries whose task key is no longer
live. `reset()` clears both maps and the stored generation.

- [ ] **Step 3: Implement sticky assignment and cohort joining**

Use `TyphonAppMatcher` to derive the initial candidate key with the existing
rules: `desktop:<desktopFileName>` for a match, otherwise
`app:<case-folded trimmed appId>` for non-empty app ID, otherwise
`window:<WindowId>`. For each snapshot window, choose in this order:

1. existing `m_windowTaskKeys[window.id]`;
2. existing `m_appTaskKeys[normalizedAppId]` for non-empty app IDs;
3. the current catalog-derived candidate.

Store the chosen key by window ID. Register a non-empty normalized app ID in
`m_appTaskKeys` only when a new task was created from that app ID or matched
desktop entry; never create a shared app cohort for an empty app ID. Return
assignments for every current window. Do not use title or PID.

- [ ] **Step 4: Pass assignments into the pure projector**

Change `DockApplicationStateProjector::project` to accept an optional
`const QHash<QString, QString> &taskKeysByWindowId = {}`. In its loop, retain
the existing matcher for presentation fields, but choose the assigned key for
the current `WindowId` when present and call the old deterministic helper only
when no assignment exists. Leave the one- and two-argument call behavior
compatible for existing projector tests.

- [ ] **Step 5: Own and reset the tracker in the controller**

Add `RuntimeTaskIdentityTracker m_runtimeTaskIdentityTracker` to
`DockController`. In `projectRuntime()`, call `update` with the current
snapshot/catalog and pass its assignments to the projector. In
`clearTyphonRuntime()`, call `m_runtimeTaskIdentityTracker.reset()` before
clearing model runtime state. Keep catalog rebuilds routed through
`projectRuntime()` so they update metadata without creating new assignments.

- [ ] **Step 6: Run the focused red-green tests**

Run:

```bash
rtk cmake --build --preset debug --target dock-application-state-projector-test dock-controller-test dock-typhon-runtime-integration-test
rtk ctest --preset debug --output-on-failure -R 'dock-application-state-projector-test|dock-controller-test|dock-typhon-runtime-integration-test'
```

Expected: the tracker/projector/controller lifetime tests pass; model pin
merge tests may still fail until Task 3 is complete.

- [ ] **Step 7: Commit the runtime identity implementation**

```bash
rtk git add shared/platform/typhon/RuntimeTaskIdentityTracker.hpp shared/platform/typhon/RuntimeTaskIdentityTracker.cpp shared/platform/typhon/DockApplicationStateProjector.hpp shared/platform/typhon/DockApplicationStateProjector.cpp Dock/core/DockController.hpp Dock/core/DockController.cpp shared/CMakeLists.txt
rtk git commit -m "fix: keep dock runtime task keys stable"
```

### Task 3: Reconcile configured pins onto live runtime keys

**Files:**
- Modify: `Dock/core/DockAppModel.hpp`
- Modify: `Dock/core/DockAppModel.cpp`

**Interfaces:**
- Consumes: live `m_runtimeStates`, projection encounter order, configured `m_pins`, and catalog metadata.
- Produces: one visible model row per configured pin/runtime task, with live matching pins represented by their stable runtime key.

- [ ] **Step 1: Track projection encounter order**

Add `QStringList m_runtimeEncounterOrder` and assign it from
`projection.encounterOrder` in `applyRuntimeProjection`. Clear it in
`clearRuntimeProjection`. Use it when selecting a live state for a configured
pin so the selection is deterministic.

- [ ] **Step 2: Add live launcher matching helpers**

Declare private helpers:

```cpp
QString runtimeTaskKeyForDesktopFileName(const QString &desktopFileName) const;
bool isPinnedTaskKey(const QString &taskKey) const;
```

Implement `runtimeTaskKeyForDesktopFileName` by scanning encounter order and
then remaining runtime states for a `running` state whose
`desktopFileName` equals the configured filename. Extend `isPinnedTaskKey` so
it returns true for a synthetic desktop key or for a live runtime state whose
launcher filename is configured. Do not infer a runtime key from the desktop
filename for any other purpose.

- [ ] **Step 3: Build desired rows with live runtime keys**

In `reconcileRows()`, append each configured pin's matching live runtime key
when available; otherwise append `desktop:<desktopFileName>`. Deduplicate the
desired list. Append existing dynamic runtime keys only when they are running
and not represented by a configured pin. Keep the existing structural
remove/move/insert algorithm and `makeItem` metadata update path unchanged.

- [ ] **Step 4: Keep dynamic order and pin changes consistent**

Update `setPins()` and `applyRuntimeProjection()` to use the extended
`isPinnedTaskKey`. Re-add only live runtime keys not represented by configured
pins. A stable live row with a matching pin must remain in the configured pin
position and retain `PinnedRole=true`; a live task without a launcher must not
become pinnable.

- [ ] **Step 5: Run model and controller tests**

Run:

```bash
rtk cmake --build --preset debug --target dock-app-model-test dock-controller-test
rtk ctest --preset debug --output-on-failure -R 'dock-app-model-test|dock-controller-test'
```

Expected: all new pin-merge, stopped-transition, unpinned-removal, order, and
existing launch-state tests pass.

- [ ] **Step 6: Commit model reconciliation**

```bash
rtk git add Dock/core/DockAppModel.hpp Dock/core/DockAppModel.cpp
rtk git commit -m "fix: merge pinned dock launchers with live tasks"
```

### Task 4: Fix desktop-filename compatibility lookup and update documentation

**Files:**
- Modify: `Dock/core/DockController.cpp`
- Modify: `Dock/docs/ARCHITECTURE.md`
- Modify: `Dock/docs/RUNTIME_FLOW.md`
- Modify: `Dock/docs/TYPHON_RUNTIME_STATE.md`

**Interfaces:**
- Consumes: authoritative `m_runtimeStates` and `windowsForTaskKey`.
- Produces: compatibility lookup that works for stable `app:`/`window:` keys and documentation matching the implementation.

- [ ] **Step 1: Resolve desktop filenames through runtime association**

Replace `windowsForDesktopFileName`'s reconstructed
`desktop:<desktopFileName>` lookup with a scan of `m_runtimeStates` for a live
state whose `desktopFileName` matches. Return `windowsForTaskKey(it.key())` for
the first matching state and an empty vector when no live association exists.

- [ ] **Step 2: Document the lifetime boundary**

Update the three Dock runtime documents so they state that `WindowId`
assignments are sticky during one live Typhon generation, catalog changes only
enrich launcher metadata, app cohorts admit new same-app windows, and reset or
authority loss ends the lifetime. Update pin reconciliation text to explain
that a matching live runtime key occupies the configured pin row and only the
stopped transition returns to `desktop:<desktopFileName>`.

- [ ] **Step 3: Run compatibility and context-menu coverage**

Run:

```bash
rtk cmake --build --preset debug --target dock-controller-test dock-typhon-runtime-integration-test context-menu-test context-menu-qml-interaction-test dock-hover-qml-test
rtk ctest --preset debug --output-on-failure -R 'dock-controller-test|dock-typhon-runtime-integration-test|context-menu-test|context-menu-qml-interaction-test|dock-hover-qml-test'
```

Expected: stable task keys remain accepted by context-menu validators, exact
actions, QML delegate identity, and minimize anchor publication.

- [ ] **Step 4: Commit helper and docs**

```bash
rtk git add Dock/core/DockController.cpp Dock/docs/ARCHITECTURE.md Dock/docs/RUNTIME_FLOW.md Dock/docs/TYPHON_RUNTIME_STATE.md
rtk git commit -m "docs: describe stable dock runtime task lifetimes"
```

### Task 5: Broad verification and final invariant audit

**Files:**
- No additional source files unless verification exposes a regression in the scoped change.

**Interfaces:**
- Consumes: all implementation and regression commits.
- Produces: fresh build/test evidence and a clean committed worktree.

- [ ] **Step 1: Build the requested debug preset**

```bash
rtk cmake --preset debug
rtk cmake --build --preset debug
```

Record the exit status and any environment failure separately from test
failures.

- [ ] **Step 2: Run the full available preset**

```bash
rtk ctest --preset debug --output-on-failure
```

Report exact failures, including known unrelated layer-shell or Qt-install
infrastructure failures; do not relabel them as identity failures.

- [ ] **Step 3: Check the patch and repository state**

```bash
rtk git diff --check
rtk git status --short
rtk git log -6 --oneline
```

The worktree must be clean after any final scoped commit.

- [ ] **Step 4: Audit the required invariants against source**

Confirm directly in `RuntimeTaskIdentityTracker`, `DockApplicationStateProjector`,
`DockController`, and `DockAppModel` that:

1. live `WindowId` assignments never change `taskKey` during one lifetime;
2. launcher metadata changes independently;
3. configured pins and matching live tasks produce one configured-position row;
4. runtime-only tasks remain valid without a launcher;
5. persistence receives only desktop filenames;
6. exact actions validate `taskKey + WindowId`;
7. minimize anchors use the stable live task key;
8. no Steam/Proton/Wine special case exists; and
9. no Typhon source file was modified.

- [ ] **Step 5: Commit any final scoped verification-only correction**

If and only if a scoped correction is required by verification, run its focused
red-green test cycle and commit it with a specific message. Otherwise leave the
already clean commit history unchanged.
