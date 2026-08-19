# M8-B — Native System Services Core

## Outcome

Eclipse now has a native shared system-services layer for audio, network, and
Bluetooth. ShellRuntime owns exactly one instance of each service, starts and
stops them with the runtime, and exposes the same instances to every TopBar
output. The status surface now includes network, Bluetooth, and volume
indicators without introducing a tray, Control Center, or full system
popovers.

## Architecture

The implementation is in shared/system and is linked as
astrea-shared-system. Each service is a Qt-thread authority with:

- a QML-safe state/health contract;
- an injectable backend interface for deterministic tests;
- a stable QAbstractListModel for native objects;
- idempotent, restartable start()/stop() behavior;
- queued immutable backend events so native callbacks never mutate QML
  objects directly.

The production adapters are:

- PipeWireAudioBackend, using the PipeWire thread loop, registry, metadata,
  node listeners, linear wire volume, and the specified cubic UI conversion;
- NetworkManagerBackend, using asynchronous Qt system-bus calls,
  service ownership watching, ObjectManager reconciliation, asynchronous Wi-Fi
  scan requests, access-point reads, and a timer over kernel traffic counters;
- BluezBackend, using asynchronous Qt system-bus calls, ObjectManager
  reconciliation, deterministic adapter selection, adapter power, discovery,
  and paired-device connect/disconnect actions.

When a daemon or bus is missing, initialization remains successful and the
corresponding service reports Unavailable or Degraded. Listener, timer,
scan-owner, pending-call, and PipeWire loop teardown is tied to service
lifetime.

## TopBar contract

BarSurfaceManager and BarSurfaceBundle receive non-owning service pointers
from ShellRuntime. StatusSurface.qml receives them through initial properties
and composes:

- NetworkIndicator for disconnected/Wi-Fi/wired state and connection label;
- BluetoothIndicator for availability, connection count, and scan pulse;
- VolumeIndicator for mute/threshold icons and wheel adjustment;
- the existing clock and geometry authority.

The indicator components do not access D-Bus, PipeWire, filesystem counters,
processes, or legacy status JSON. The existing palette, popup ownership,
status width/anchor calculation, and multi-output bundle lifecycle remain
unchanged.

## Diagnostics

The existing read-only shell status JSON now contains:

    {
      "system": {
        "audio": {"state": "...", "available": true, "ready": true},
        "network": {"state": "...", "available": true, "ready": true},
        "bluetooth": {"state": "...", "available": true, "ready": true}
      }
    }

Only health and coarse live state are exposed; credentials, Wi-Fi secrets,
pairing data, and private device payloads are not included.

## Legacy audit

The existing guard now scans Bar QML/core/platform sources and the complete
shared/system production source tree. The new tree contains no legacy
process/status integration tokens. Native code uses Qt D-Bus and PipeWire
APIs directly.

## M8-B.1 correctness closure

The first implementation pass exposed three authority gaps: BlueZ was
flattening the nested ObjectManager graph and treated D-Bus method submission
as state confirmation; NetworkManager published whichever device callback
arrived last and used a process-global scan clock; PipeWire had no bounded
service-level recovery after a core loss and did not retain per-output
properties across default-sink changes.

Those gaps are now closed without changing the M8-B boundary. BlueZ keeps a
typed path → interface → property graph, merges changed and invalidated
properties from Adapter1, Device1, and Battery1, watches daemon generations,
and settles power/discovery/device operations through completion or bounded
timeout. Bluetooth scan owners express demand while Discovering remains the
authoritative visible state, and Connect rejects devices that are not paired
in the current model.

NetworkManager now listens to manager DeviceAdded/DeviceRemoved signals,
reconciles a PrimaryConnection through its ActiveConnection and physical
device paths, keeps Wi-Fi availability separate from WirelessEnabled and
Scanning, coalesces/cools scan requests, and resets traffic baselines on
interface changes, counter resets, stop, or daemon loss. VPN active
connections are projected onto their selected physical transport rather than
reported as an invented physical type.

AudioService owns a cancellable 250/500/1000/2000/5000 ms reconnect backoff
and PipeWire teardown/re-acquire sequence. PipeWire caches volume, mute, and
channel-volume state per output, averages channel volumes for the compact
status projection, and re-queries the selected default node after metadata
changes. The cubic 0–150% UI mapping remains unchanged.

All service callbacks retain generation checks, diagnostics include the
reconciled Wi-Fi availability/scanning fields, and the production-QRC Bar
smoke test asserts the actual indicator projections.

## Verification

The following focused checks passed in the isolated Unix Makefiles build
directory:

- system-services-test: 8 passed;
- shell-runtime-test: 6 passed;
- bar-core-test: 22 passed;
- bar-qml-smoke-test: 13 passed;
- python3 Bar/tests/check_bar_qml.py Bar/qml shared/system;
- git diff --check;
- astrea-shell non-layer-shell development build.

The M8-B.1 red/green closure suite passed with 14 deterministic
system-service tests and 13 production-QRC Bar tests. The red baseline
included missing Wi-Fi authority fields and optimistic/recovery behavior;
the green run covers audio startup/runtime reconnect, authoritative
Bluetooth discovery and power, paired-only connect, Wi-Fi availability and
scanning, and indicator projections.

The complete Unix Makefiles build passed, followed by 61 configured CTest
entries: 57 passed, 3 integration skips, and the same unrelated
SettingsNavigationModelTest failure caused by the concurrent 13-route
wallpaper catalog. The focused system and Bar tests also passed under
AddressSanitizer plus UndefinedBehaviorSanitizer with leak detection
disabled. LeakSanitizer with Qt offscreen QML reported 183 bytes from the
host NVIDIA GL driver; this is an environment leak outside Eclipse and is
reported rather than suppressed in product code.

The restart regression was also run ten consecutive times against the live
PipeWire daemon; all ten runs passed after listener and metadata-binding
teardown was hardened.

The complete configured CTest run reported 57 passed and 3 environment-
dependent skips. Its only failure was the concurrent Settings navigation test,
which still expects the pre-existing 12-entry catalog while the parallel
wallpaper work adds a 13th route.

The tests cover model sorting/deduplication/no-op resets, audio conversion,
fake-backend lifecycle/restart, Bluetooth scan-owner reference counting,
runtime ownership, injected QML facades, and volume wheel forwarding.

## Live qualification

Read-only host observations on 2026-08-19 found PipeWire 1.6.8, NetworkManager,
and BlueZ active through their native host services, with BlueZ hci0 and
NetworkManager device/active-connection trees visible. ShellRuntime lifecycle
tests also started and stopped the native service authorities against this
host without a crash. No mutating action, connection, scan, power change, or
volume change was performed. Full visual qualification still requires
starting Eclipse on a Wayland session with the production LayerShellQt
preset; the offscreen QML smoke suite provides the deterministic surface
evidence available in this environment. The canonical Ninja preset could not
be configured because Ninja is not installed; the Unix Makefiles build was
used instead.

## Limitations and non-goals

This milestone intentionally does not add a system tray, Control Center,
settings panels, OSDs, MPRIS, Wi-Fi password flows, Bluetooth pairing or
agent flows, or settings migration. Network connection-detail selection is
kept at the compact status-contract level. Existing Paper, Settings, Dock,
Spotlight, and AltTab work in the concurrent worktree is outside this change.
