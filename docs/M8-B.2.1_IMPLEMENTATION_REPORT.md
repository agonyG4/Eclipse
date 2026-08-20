# M8-B.2.1 — Final Native System Services Correctness Closure

## Scope and source baseline

This closure was implemented in the Eclipse repository at source baseline
`0afe2696bfb72e7a459f1f5c4f7872054039230b` on 2026-08-20. The accepted M8-B.2
architecture was preserved:

```text
NetworkService   -> NetworkManagerBackend -> NetworkManagerState / NetworkScanState
BluetoothService -> BluezBackend          -> BluezObjectStore / BluezDiscoveryState
AudioService     -> PipeWireAudioBackend  -> PipeWireAudioState
```

The production change is concentrated in `shared/system/*`. The only Bar
production changes consume the existing `defaultStateAvailable` service
property so the volume UI does not present or mutate stale default-sink state.
No TopBar visual parity work was resumed, and M8-C was not started.

## Root causes closed

1. NetworkManager wireless `GetAll()` replies were protected by daemon
   generation but not the device-refresh generation, allowing an old refresh
   to interfere with a newer refresh's state or accounting.
2. The fallback Wi-Fi adapter selected before `PrimaryConnection` resolution
   was not consistently reselected when the primary ActiveConnection changed.
3. BlueZ discovery completion identified operations by daemon generation and
   operation strings, but not by a per-attempt identity; failed starts could
   also strand persistent demand.
4. BlueZ invalidation refreshes lacked an object/interface revision, so a late
   `GetAll()` could overwrite a changed, removed, or recreated interface.
5. PipeWire `SPA_PARAM_Props` events were treated as complete replacements,
   and the default-output command path could optimistically mutate local
   metadata state before the metadata write was accepted and observed.

## NetworkManager closure

`NetworkWirelessRefreshState` now owns the active wireless refresh identity
and pending device paths. Every wireless `GetAll()` callback captures daemon
generation, refresh generation, and device path. A stale callback only deletes
its watcher; it cannot update `NetworkManagerState`, decrement current
accounting, complete reconciliation, or refresh access points.

`reconcileSelectedWifiDevice()` is the single selected-adapter transition
helper. It is called after primary reconciliation and live primary property
changes. A switch invalidates the AP refresh generation, invalidates the scan
operation and cooldown, clears old AP state, updates the adapter and active AP
authority, rebuilds watchers, publishes the new snapshot, and refreshes the new
adapter. Active-device removal also invalidates old AP replies immediately
before the normal device refresh.

The existing protocol behavior remains authoritative: `PrimaryConnection`,
`ActiveAccessPoint`, `LastScan`, `AccessPointAdded`, and `AccessPointRemoved`
are used without polling or command-line fallback.

## BlueZ closure

Discovery operations now use a typed `BluetoothOperationResult` carrying the
operation kind and monotonically increasing request id. Only the current
request id can settle the current StartDiscovery or StopDiscovery operation.
Timeout invalidates the old id before state changes, so a late reply is inert.

Persistent demand after a failed or timed-out StartDiscovery uses one bounded
single-shot retry timer with delays of 500 ms, 1 s, 2 s, and 5 s maximum.
Retries are cancelled/reset when demand disappears, the adapter or daemon is
unavailable, or the service stops. Successful acquisition resets the backoff.
The existing shared-discovery lease model remains intact: releasing Eclipse's
lease does not require `Adapter1.Discovering` to become false for other
clients.

`BluezObjectStore` now keeps monotonic revisions per object path and interface.
Revisions change on managed-object replacement, interface add/remove,
`PropertiesChanged` merges, and authoritative replacement. Invalidated
refreshes capture the current revision after merging changed properties and
apply a `GetAll()` result only when daemon generation, object path, interface,
and revision still match. Invalidated properties continue to retain their
last-known values until the authoritative refresh arrives.

## PipeWire and audio UI closure

`PipeWireNodePropsPatch` uses optional volume, mute, and channel-volume fields.
`PipeWireAudioState::applyNodePropsPatch()` changes only properties present in
the event; absent fields preserve their known values. Channel volumes preserve
the distinction between absent and present-empty. `defaultVolume()` returns a
pair only when mute and volume are independently authoritative, with volume
coming from a valid scalar or channel-volume value.

`setDefaultOutput()` now validates the node and metadata object, writes the
metadata value through a small testable seam, and waits for the metadata event
to update local authority. A failed or accepted write does not optimistically
change the local default.

The existing cubic 0–150% UI mapping and linear PipeWire state remain
unchanged. `VolumeIndicator` and `VolumePopup` now consume
`defaultStateAvailable`: unavailable state uses the existing unavailable/muted
semantics, suppresses wheel changes, and disables mute and slider input. No
layout, popup, glyph, or visual redesign was made.

## Regression tests

`SystemServicesTest` now reports 36 QTest functions: 34 logical feature tests
after excluding setup/cleanup. The added production-used coverage includes:

- stale NetworkManager wireless refresh accounting;
- fallback-to-primary Wi-Fi selection and live primary-device reselection;
- BlueZ interface revisions for changed, removed/recreated, and independent
  interfaces;
- discovery failure retry, timeout/late-reply rejection, retry cancellation,
  daemon-loss recovery, and stop-failure/new-demand recovery;
- PipeWire partial mute, volume, and channel patches;
- incomplete default state and non-optimistic metadata writes.

The preserved Bar regression boundary reports:

- `BarCoreTest`: 23 QTest functions, 21 logical feature tests;
- `BarQmlSmokeTest`: 18 QTest functions, 16 logical feature tests, including
  `volumeUiDisablesWhenDefaultStateUnavailable`;
- `bar-qml-legacy-guard`: passed.

## Build and validation matrix

Using the repository's existing build directories and presets:

| Preset | Target builds | `system-services-test` | BarCore / BarQml |
|---|---:|---:|---:|
| Debug | passed | 36/36 | 23/23, 18/18 |
| Release | passed | 36/36 | 23/23, 18/18 |
| Clang | passed | 36/36 | 23/23, 18/18 |
| ASan | passed | 36/36 | functional tests reached, process exit failed on 183-byte NVIDIA/GL indirect leak |
| UBSan | passed | 36/36 | 23/23, 18/18 |
| no-Typhon | passed | 36/36 | 23/23, 18/18 |
| no-layer-shell | passed | 36/36 | 23/23, 18/18 |

The Clang build emitted only existing PipeWire macro/C99-extension warnings;
there were no new compile errors. The ASan system suite and UBSan system
suite were both clean. ASan Bar processes exit nonzero after their functional
checks because the host NVIDIA/GL stack reports an indirect 183-byte leak;
this is separate from the changed native-service code.

The current Release CTest run contains 63 registered tests:

- 60 passed;
- 2 failed: the existing dirty Settings navigation expectations, and
  `shell-unified-runtime-integration-test` failing to create its local socket
  with `QLocalServer::listen: Name error`;
- 1 not run: `paper-surface-policy-test`, whose executable is absent from the
  current unrelated Paper build graph.

The scoped Shell runtime check passed, and the Bar legacy guard passed. The
full-suite failures were not changed or hidden by this milestone.

## Live qualification

Only read-only host checks were performed; no scan, pairing, connection
transition, audio default change, volume change, or daemon restart was made.

- NetworkManager and bluetooth system services were active.
- PipeWire and WirePlumber user services were active.
- NetworkManager `PrimaryConnection` was
  `/org/freedesktop/NetworkManager/ActiveConnection/2`, state `2` (activated),
  using `/org/freedesktop/NetworkManager/Devices/2` (`eno1`, Ethernet).
- BlueZ `/org/bluez/hci0` reported `Powered=true` and `Discovering=false`.

Live Wi-Fi adapter switching, Bluetooth discovery ownership, and PipeWire
default A→B→A confirmation were not forced because doing so could disturb the
active desktop session. The deterministic seams cover those race and
authority rules without requiring live daemons.

## Manual audits and limitations

The final production search found no legacy `wpctl`, `pactl`, `pw-cli`,
`pw-dump`, `pw-mon`, `nmcli`, `iw`, `iwctl`, `bluetoothctl`, Quickshell,
`astrea-statusd`, or legacy JSON bridge under `shared/system`. Discovery
completion is typed and request-identity checked; invalidation refreshes use
interface revisions; and destructive `volumeKnown = property != nullptr` /
`muteKnown = property != nullptr` replacement semantics are absent.

The deterministic tests do not replace a full live D-Bus/PipeWire integration
harness. They do not force real AP transitions, Bluetooth owner races against
another live client, or live metadata default switching. No pairing, Wi-Fi
SecretAgent flow, per-application audio, profiles, System Tray, Control
Center, VPN, or MPRIS work was added.

The codebase-memory index was refreshed after the edits. Its best-effort
coverage report still marks several Qt/D-Bus files as parse-partial; the
flagged source ranges were checked directly and all final claims are qualified
accordingly.

## Protocol references

Implementation assumptions were checked against the official
[NetworkManager D-Bus API](https://networkmanager.dev/docs/api/latest/),
[D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager),
[BlueZ Adapter API](https://bluez.readthedocs.io/en/latest/adapter-api/),
[PipeWire metadata API](https://docs.pipewire.org/page_module_metadata.html),
and [WirePlumber default-node configuration](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/default_nodes.html).
