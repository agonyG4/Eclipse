# M8-B.2 — Native System Backend Protocol Fidelity and Reconciliation Closure

## Scope and baseline

Target milestone: M8-B.2.

Primary production scope: `shared/system/*`.

The current Eclipse source was used as the implementation baseline. The existing M8-A.3 TopBar presentation layer was preserved, not reimplemented. No `Bar/qml/*` production presentation files were changed.

The work remains within the established architecture:

```text
ShellRuntime
├── AudioService → PipeWireAudioBackend
├── NetworkService → NetworkManagerBackend
└── BluetoothService → BluezBackend
```

## NetworkManager

The previous backend flattened base Device and Device.Wireless properties, treated a nonexistent `Scanning` property as authoritative, refreshed the primary connection only as a one-shot snapshot, and rebuilt access-point state too broadly.

Production now uses `NetworkManagerState` and `NetworkScanState` to keep base Device and Wireless properties separate. `ActiveAccessPoint` and `LastScan` are read only from the Wireless state. Wi-Fi discovery selection prefers the Wi-Fi device associated with the current primary connection and otherwise uses deterministic object-path ordering.

`wifiScanning` is explicitly an Eclipse operation-state projection: a locally accepted scan request that has not yet completed, failed, been cancelled, timed out, or been invalidated. `RequestScan()` success transitions to `WaitingForLastScan`; completion requires `LastScan` to advance beyond the captured baseline. Requests during an active operation coalesce, and cooldown always has a timer-driven execution path.

`PrimaryConnection` is the authority for the primary network projection. ActiveConnection `StateChanged` and PropertiesChanged signals are observed, primary request epochs reject late A→B replies, and the associated physical Ethernet/Wi-Fi device is resolved deterministically. Device lifecycle, Wireless updates, ActiveAccessPoint changes, AP add/remove events, AP property changes, and refresh generations are reconciled without retaining stale object paths.

## BlueZ

Production now uses `BluezObjectStore` for the nested object-path → interface → properties structure. `InterfacesAdded` merges interfaces into an existing object, while `InterfacesRemoved` removes only the listed interfaces and removes the object only when empty.

Invalidated properties are never converted into fabricated `false`, zero, or empty values. The previous cached public value remains in place while a generation-checked `GetAll()` refresh replaces the affected interface authoritatively. Object-manager generations clear stale objects and invalidate late replies.

`BluezDiscoveryState` separates scan demand, Eclipse's discovery lease (`None`, `StartPending`, `Held`, `StopPending`), and authoritative Adapter1 `Discovering`. Stop releases Eclipse's lease without requiring global discovery to become false, so another client may continue scanning. Start/stop owner races and daemon recovery reconcile toward one lease without request storms. RSSI is represented as unknown (`-1`) when BlueZ does not provide it rather than as fabricated zero.

## PipeWire and audio service

Production now uses `PipeWireAudioState` as the per-attempt cache for nodes, metadata-derived default identity, volume, channel volumes, mute, and known flags. The arbitrary first-output fallback was removed. Default resolution is metadata-authoritative by current-generation node id when available, otherwise by stable `node.name`; an unresolved default remains unknown.

`AudioService` tracks the backend-provided default node id and volume/mute actions target that id directly, never model row zero. Default changes clear the old default state until the new node's cached or newly enumerated Props are authoritative. PipeWire remains linear-domain; the established AudioService cubic 0–150% conversion is unchanged. Each backend attempt receives a fresh callback generation, and queued callbacks from an older attempt are ignored.

## Production reconciliation components

Added and wired into the production system target:

- `NetworkManagerState` and `NetworkScanState`;
- `BluezObjectStore` and `BluezDiscoveryState`;
- `PipeWireAudioState`.

These are deliberately small state/reconciliation components; D-Bus, PipeWire proxy, thread-loop, watcher, and lifetime mechanics remain in the native backends.

## Tests and regression boundary

`SystemServicesTest` retained the existing fake-backend service-contract coverage and added six production-used protocol/state tests:

- `audioLateAttemptCallbacksAreIgnored`;
- `networkStateSeparatesWirelessAndPrimaryState`;
- `networkScanUsesLastScanAndCoalesces`;
- `bluezObjectStoreMergesInterfaces`;
- `bluezDiscoverySeparatesDemandLeaseAndActualState`;
- `pipewireStateUsesMetadataAndPerNodeCache`.

The deterministic system test executable reports 20 QTest functions passed, including infrastructure `initTestCase` and `cleanupTestCase`; the logical feature count is 18: six protocol/reconciliation tests and twelve existing service/model contract tests.

The existing M8-A.3 regression boundary remains green:

- BarCore: 21 logical feature tests passed (23 QTest-reported functions including setup/cleanup);
- BarQmlSmoke: 15 logical feature tests passed (17 QTest-reported functions including setup/cleanup);
- Bar QML legacy guard: passed.

The same focused system and Bar tests passed in the no-layer-shell configuration.

## Build and sanitizer matrix

Using the repository's existing incremental build directories:

- Debug: full build passed;
- Release: full build passed;
- Clang: `system-services-test` build and run passed;
- ASan: `system-services-test` build and run passed;
- UBSan: `system-services-test` build and run passed;
- no-Typhon: `system-services-test` build and run passed;
- no-layer-shell: system, BarCore, and BarQml smoke builds/runs passed.

Full Release CTest completed 60/62 tests. The two failures are outside this milestone: the existing dirty Settings worktree changes the expected navigation catalogue, and the existing Shell unified-runtime integration test cannot create its local socket (`QLocalServer::listen: Name error`). The equivalent Debug run reproduced those same two isolated failures; an initial parallel Debug/Release run also caused one transient AltTab socket-name collision, and the isolated AltTab test passed.

## Live qualification

Non-destructive host qualification was performed without changing network, Bluetooth, or audio state:

- NetworkManager and `org.freedesktop.NetworkManager` were active;
- the primary connection was `/org/freedesktop/NetworkManager/ActiveConnection/2`, state `ACTIVATED`, backed by `/org/freedesktop/NetworkManager/Devices/2` (`eno1`, Ethernet);
- BlueZ and `bluetoothd` were active; `/org/bluez/hci0` reported `Powered=true`, `Discovering=false`;
- PipeWire and WirePlumber were active on the user bus.

No scan, connection transition, Bluetooth pairing, default-audio switch, volume change, or daemon restart was performed. This avoided disturbing the developer's active desktop session.

## Not executed and limitations

The deterministic tests exercise production reconciliation state but do not require live daemons and do not replace a full D-Bus/PipeWire integration harness. Live AP signal transitions, `LastScan` advancement from a real scan request, Bluetooth owner races against another live client, and PipeWire default A→B publication were not forced during qualification.

The full ASan/UBSan CTest matrix was not run; the targeted production system suite was run under both sanitizers. No pairing, Wi-Fi password/SecretAgent flow, System Tray, Control Center, VPN UI, MPRIS, or per-application audio work was added.

The codebase-memory coverage report is best-effort and flags several Qt/D-Bus source ranges as parse-partial; those ranges were checked with direct source reads and literal searches before completion.

## Protocol references

Implementation assumptions were checked against the official [NetworkManager D-Bus API](https://networkmanager.dev/docs/api/latest/), [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties), [BlueZ Adapter API](https://bluez.readthedocs.io/en/latest/adapter-api/), [PipeWire metadata API](https://docs.pipewire.org/page_module_metadata.html), and [WirePlumber default-node configuration](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/default_nodes.html).
