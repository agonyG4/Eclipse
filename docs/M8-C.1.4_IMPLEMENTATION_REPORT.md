# M8-C.1.4 Implementation Report

## Result

M8-C.1.4 is implemented in the Eclipse workspace. The closure preserves the existing M8-C.1.3 StatusNotifier architecture and completes the remaining watcher-topology, menu-lifetime, DBusMenu-bounds, item-removal, and reference-parity requirements.

Baseline: M8-C.1.3 commit `7077a9f`.

## Runtime closure

- `QDBusServiceWatcher` keeps both `org.freedesktop.StatusNotifierWatcher` and `org.kde.StatusNotifierWatcher` observed for the complete bridge lifetime. Authority switching only detaches the selected signal subscriptions; it no longer removes the alias from topology observation.
- `StatusNotifierItemProxy` emits `menuPathChanged` only for an actual transition. The protocol no-menu root path `/` is normalized to the service’s empty-menu representation. Terminal proxies cannot publish queued status or snapshots after stop.
- Menu-client identity is now `(Menu path, live item generation)`. Same-identity presentation refreshes preserve the client and root/cascade model. Real path transitions replace the client exactly once. The unused visual `closeMenu()` transport API was removed; `DBusMenuClient::stop()` remains terminal.
- Presentation and content signals are separated. Title/icon updates refresh the menu header without resetting the live menu; `menuClientChanged` is reserved for identity changes and `menuContentChanged` for live menu mutations.
- Item removal stops the proxy/menu client, clears model/icon state, closes a visible tray popup through the normal controller lifecycle, and hides/clears a visible tooltip.
- DBusMenu subtree replacement validates the complete live model graph before mutation. Node-count and absolute-depth checks use old-branch subtraction plus candidate-branch addition. Rejected candidates preserve the old tree and do not retry; only a missing target can request a bounded root refresh.

## Reference parity closure

- Tray delegates are 28×28 inside the 36px status row, with the reference radius, transparent idle state, separator hover, literal pressed color `Qt.rgba(1, 1, 1, 0.2)`, fast color animation, and pointing-hand cursor for ready items.
- The 420ms tooltip timer was removed. Tooltip presentation is immediate on hover, uses only the compact title, caps at 260px, uses implicit text width plus 18px, is 28px high, and follows the reference active text/font treatment.
- Tooltip top margin is 43px; transparent input, no-focus, and exclusive-zone `-1` behavior remain unchanged.
- Context menus use native item icon/title accessors, conditional remote-menu separators, the shared `MenuSeparator`, the reference submenu glyph `󰅂`, and the 32px secondary/small no-actions row. Presentation-only updates no longer reset live cascades.

## Feature scenarios versus framework totals

The counts below exclude QTest’s automatic `initTestCase` and `cleanupTestCase` rows. They are feature-scenario counts, not a claim that each test executable contains only those rows.

| Suite | Feature scenarios | QTest total | Result |
|---|---:|---:|---|
| `statusnotifier-test` | 22 | 24 | Pass |
| `statusnotifier-dbus-integration-test` | 2 | 4 | Pass |
| `bar-core-test` | 23 | 25 | Pass |
| `bar-qml-smoke-test` | 27 | 29 | Pass |
| `bar-qml-legacy-guard` | 1 CTest scenario | n/a | Pass |

Focused QTest coverage is therefore 74 feature scenarios across 82 framework rows, plus one legacy-guard CTest scenario.

The integration scenarios cover real session-bus registration/actions/menu lifecycle and the KDE-first/Freedesktop-second alias sequence, including KDE disappearance after the bridge has switched to Freedesktop. They also verify same-path menu-client identity, title-only presentation refresh, empty/path transitions, and exact client replacement count.

## Build and test matrix

All focused builds used existing build directories and RTK-wrapped commands:

```text
cmake --build build/<configuration> --target \
  statusnotifier-test statusnotifier-dbus-integration-test \
  bar-core-test bar-qml-smoke-test -j2

ctest --test-dir build/<configuration> -R \
  '^(statusnotifier-test|statusnotifier-dbus-integration-test|bar-core-test|bar-qml-smoke-test|bar-qml-legacy-guard)$' \
  --output-on-failure
```

| Configuration | Focused build | Focused CTest |
|---|---|---|
| Debug | Pass | 5/5 pass |
| Release | Pass | 5/5 pass |
| Clang | Pass | 5/5 pass |
| no-Typhon | Pass | 5/5 pass |
| no-layer-shell | Pass | 5/5 pass |
| UBSan | Pass | 5/5 pass |
| ASan | Pass | Feature assertions pass; 4 QTest processes report the same exit-time leak below |

ASan reports a 183-byte LeakSanitizer leak from `/usr/lib/libnvidia-glcore.so.610.57.04`, with all affected QTest feature rows passing. This is an external GL-driver shutdown leak, not a sanitizer finding in the changed Eclipse code; the ASan CTest process exit status is nevertheless recorded as failed.

The full Debug CTest matrix ran 66 entries: 62 passed, two were Not Run because the configured build lacked stale executables (`typhon-workspace-state-test` and `Paper/paper-surface-policy-test`), and two unrelated existing dirty-worktree tests failed (`settings-navigation-model-test` and `shell-unified-runtime-integration-test`). The changed StatusNotifier and Bar tests passed in that matrix.

`git diff --check` passed. The legacy tray guard passed, and no legacy daemon/process-helper strings were introduced in the production tray path.

## Visual and application qualification

Production-QRC Bar smoke tests passed, including tray geometry, tooltip cleanup, popup cleanup, menu context identity, recursive cascade bounds, and the 43px surface policy. A live 1920×1080 compositor comparison was Not Run because this environment did not provide the compositor/reference visual harness.

Real application tray qualification was Not Run. The local inventory included `/home/agony/.local/share/SLSsteam/path/steam` and `/usr/bin/discord`; no Telegram executable was available. No live application was launched because the required compositor/reference harness was unavailable.

## Remaining limitations

- The external NVIDIA GL-driver leak prevents a zero exit status for the ASan CTest processes even though their feature assertions pass.
- The unrelated dirty-worktree Debug failures and stale missing test executables remain outside this M8-C.1.4 scope and were preserved unchanged.
- Live compositor/application visual qualification remains pending an environment with the required session/compositor harness.
