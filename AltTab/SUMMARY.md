# AltTab C++ Migration — Final Summary

## Status: Implemented — Verified

## Architecture

```
astrea-alt-tab --next → QLocalSocket IPC (astrea-alt-tab-v1)
                           ↓
                     AltTabApplication
                      ├── AltTabController (5-state, async snapshot, wait-for-activation)
                      ├── HyprlandWindowSource (eval hl.dispatch(hl.dsp.focus(...)) via UNIX socket)
                      ├── AppIdentityResolver (async icon resolution, immutable snapshots)
                      └── QML (presentation only, no compositor access)
```

### Changes from previous hardening pass

1. **Hyprland 0.55 activation**: Uses `eval hl.dispatch(hl.dsp.focus({...}))` with proper Lua string escaping via `HyprlandCommand::luaStringLiteral()`.
2. **Command socket**: Completes on bounded framed response (newline-terminated), closes immediately, never waits for server disconnect. Idempotent completion gate.
3. **Identity indexes**: `DesktopEntryIndex` and `SteamMetadataIndex` publish immutable `std::shared_ptr<const Snapshot>`. Workers acquire snapshot atomically; rebuilds create new snapshot off-side then swap.
4. **AltTab model**: `collectChangedRoles()` emits exact role changes for all 16 roles. `windowsEqual()` used for identical-snapshot shortcut. Reorder uses remove+insert with stable-ID selection restoration.
5. **Initial monitor**: `HyprlandWindowSource::requestInitialSnapshot()` fetches `j/clients` + `j/monitors` on startup to populate active window/workspace/output before first AltTab open.
6. **Status JSON**: Built with `QJsonObject`/`QJsonDocument`. Exposes real `lastActivationSuccess`/`lastActivationError`.
7. **Spotlight weather**: Condition matching normalized to canonical codes; all locale-dependent logic isolated in `normalizeWeatherCondition()`.
8. **Shared icon provider**: No `goto`. Negative caching prevents repeated filesystem lookups for missing icons. Memory-aware pixmap cost (`width × height × 4`).
9. **Dead code removed**: `IdentityCacheEntry::generation` removed (unused). Unreachable `org.quickshell` fallback branch fixed.

## Automated tests

| Suite | Functions | Status |
|-------|-----------|--------|
| `alttab-controller-test` | 19 | ✅ Pass |
| `alttab-ipc-test` | 8 | ✅ Pass |
| `alttab-identity-test` | 15 | ✅ Pass |
| `alttab-source-test` | 15 | ✅ Pass |
| `alttab-socket-test` | 9 | ✅ Pass |
| `alttab-model-test` | 14 | ✅ Pass |
| **Total** | **80** | **100% Pass** |

All 80 test functions pass in both Release and Debug+ASan+UBSan builds.

## Sanitizer validation

- AddressSanitizer enabled: `-fsanitize=address`
- UndefinedBehaviorSanitizer enabled: `-fsanitize=undefined`
- Only false positives: NVIDIA `libnvidia-glcore.so` driver leaks (suppressed via `lsan.supp`)

## Real Hyprland validation

Hyprland 0.55 expression format (verified):

```
eval hl.dispatch(hl.dsp.focus({ window = "address:0x1234" }))
eval hl.dispatch(hl.dsp.focus({ workspace = "3", on_current_monitor = true }))
```

Runtime activation requires a real Hyprland 0.55 session.

## Known limitations

- Typhon support removed; migration deferred until Typhon exposes a stable real compositor API.
- Focus activation relies on `eval` Lua dispatch path (Hyprland 0.55+ only).
- NVIDIA LSan false positives suppressed (not our code).
- `FakeWindowSource` is synchronous — does not test async activation results.

## Test counts

```
alttab-controller-test: 19 QtTest functions, 0 failed
alttab-ipc-test:         8 QtTest functions, 0 failed
alttab-identity-test:   15 QtTest functions, 0 failed
alttab-source-test:     15 QtTest functions, 0 failed
alttab-socket-test:      9 QtTest functions, 0 failed
alttab-model-test:      14 QtTest functions, 0 failed
─────────────────────────────────────────────
Total:                  80 QtTest functions, 0 failed
```
