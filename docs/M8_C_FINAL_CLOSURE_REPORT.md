# M8-C Final Closure — Reactive Tray Projection and Final Qualification

Date: 2026-08-24

Baseline: `21e0c73` (`M8-C.1.4: final native tray runtime closure`)

Status: **M8-C COMPLETE**

## Scope

This closure is limited to the native StatusNotifier tray projection. It adds no M8-C.1.5, no Typhon work, no Control Center, no Notification History, no XEmbed, and no unrelated architecture changes. Existing unrelated Settings, Shell, Paper, Bench, and CI worktree changes were preserved and were not staged.

## Implementation

`StatusNotifierService` now exposes one monotonic `quint64 presentationRevision` Q_PROPERTY. Every increment is centralized in `bumpPresentationRevision()`, which emits `presentationRevisionChanged()` after committed presentation-visible state changes. The revision advances for item snapshots, effective icon-store changes, menu client creation/removal/replacement, DBusMenu lifecycle/content changes, item removal, and the final stopped projection. Watcher health-only changes and getter calls do not mutate it. Stop/start cycles remain monotonic.

`TrayContextMenu` exposes one `serviceRevision` dependency and explicitly binds `menuModel`, `hasRemoteMenu`, and `remoteMenuState` through it. The open popup rebinds `/MenuA -> /MenuB`, updates menu removal/appearance, and keeps the existing popup/context. `menuClientChanged` remains the identity-reset signal; same-menu title/icon/tooltip changes do not reset the model, client, or cascade state. `presentationSerial` was removed.

`TrayTooltipSurface.tooltipTitle` now reads `presentationRevision`, so an already-visible tooltip updates without pointer re-entry. Its compact fallback remains `tooltipTitle || title || ""`; empty effective text hides the card while restoring the same item key updates it again. C.1.4 visual values remain unchanged: immediate hover behavior, 260px maximum width, implicit text width plus 18px, 28px height, small radius, background/border, active text color, theme font, caption size, transparent input, and 43px placement.

`displayTitleForItem()` now uses the exact production order:

```text
tooltipTitle || title || id || "Tray item"
```

The hover tooltip does not use the id or `Tray item` fallback.

## Regression coverage

New and extended production-path coverage includes:

- C++ title fallback coverage for all four cases.
- C++ revision monotonicity for snapshots, title changes, menu identity, icon source, removal, stop/start, and non-mutating getters.
- Production-QRC menu path rebind, menu removal, menu appearance, same-menu identity/cascade preservation, live header title/icon updates, and exact header fallbacks.
- Production-QRC tooltip live update, empty-card hiding, restoration, and same-item-key retention.
- Existing isolated D-Bus fixture extended in place with real icon refresh and `/org/test/MenuA` to `/org/test/MenuB` identity replacement.

Feature-case counts exclude QTest `initTestCase` and `cleanupTestCase`:

| Target | Feature cases | Result |
|---|---:|---|
| `statusnotifier-test` | 24 | 24 passed |
| `statusnotifier-dbus-integration-test` | 2 | 2 passed |
| `bar-core-test` | 23 | 23 passed |
| `bar-qml-smoke-test` | 29 | 29 passed |
| `bar-qml-legacy-guard` | 1 CTest scenario | passed |

The four QTest targets therefore provide 78 feature cases, plus one legacy-production guard scenario.

## Qualification matrix

The required focused order was run as `statusnotifier-test`, `statusnotifier-dbus-integration-test`, `bar-core-test`, `bar-qml-smoke-test`, and `bar-qml-legacy-guard`.

| Configuration | Focused result |
|---|---|
| Debug | 5/5 passed |
| Release | 5/5 passed |
| Clang | 5/5 passed |
| no-Typhon | 5/5 passed |
| no-layer-shell | 5/5 passed |
| UBSan | 5/5 passed |

The broad Debug CTest suite had 66 entries: 62 passed, two stale missing executables were Not Run (`typhon-workspace-state-test` and `Paper/paper-surface-policy-test`), and two unrelated dirty-worktree tests failed with their pre-existing symptoms: `settings-navigation-model-test` (catalog count/route expectation) and `shell-unified-runtime-integration-test` (`QLocalServer::listen: Name error`). The StatusNotifier and Bar targets passed in that run.

ASan executed all five feature scenarios and all feature assertions passed, including 26 StatusNotifier cases, 4 integration cases, 25 BarCore cases, 31 BarQML cases, and the legacy guard. CTest reported the four QTest processes failed only because LeakSanitizer detected the known 183-byte direct/indirect leak chain in external `/usr/lib/libnvidia-glcore.so.610.57.04`; no Eclipse-owned stack appeared. This is reported as an external sanitizer limitation, not as sanitizer success.

The legacy guard passed, and the production Bar QML tree contains no `QProcess`, `qdbus`, `gdbus`, `dbus-send`, `Process`, `quickshell`, `waybar`, or Python helper usage.

## Manual qualification

Manual visual qualification: **Not Run**. A reliable live display qualification session was not available in this run; automated production-QRC geometry and visual assertions passed.

Real third-party StatusNotifier applications: **Not Run**. The isolated D-Bus fixture was run and passed, but no external tray application was launched.

No generated build directories or build artifacts are part of the closure scope.

## Final assessment

The native tray projection now has one reactive invalidation source, preserves DBusMenu identity for same-menu presentation updates, rebinds menu identity and empty/appearing states while open, updates header and tooltip content live, and keeps the exact reference title fallback and C.1.4 visuals. Existing watcher, bounds, removal, geometry, and legacy protections remain green.

**M8-C COMPLETE**
