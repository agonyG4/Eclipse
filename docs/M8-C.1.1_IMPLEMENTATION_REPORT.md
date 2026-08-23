# M8-C.1.1 — StatusNotifier Wire Contract, Native Menu Bootstrap, and Zero-Skip Integration Closure

## Scope

This closure completes the M8-C.1 StatusNotifier/DBusMenu interoperability
work in Eclipse. It preserves the single process-wide
`StatusNotifierService`, per-output presentation state, the historical M8-C.1
report, and the existing native watcher aliases. No Typhon, M8-B, M8-D, XEmbed,
notification-history, or workspace changes are included.

## Gaps closed

The prior closure left the real-session-bus test body guarded because the
fixture handshake had not been proven through the same D-Bus path used by the
production watcher. It also left several protocol edges implicit:

- service-only registration used a duplicated default path, while caller
  identity for path-only registration was not reliably obtained from the
  exported object;
- the host signal was exposed with an extra argument in the introspection
  surface, and asynchronous host verification did not fully invalidate owner
  loss;
- the menu loader treated an empty root as a failure and could skip the first
  `AboutToShow`/`GetLayout` bootstrap;
- Qt's actual D-Bus representations for layout replies, `a{sv}` properties,
  and the DBusMenu event variant were not all covered by the live path;
- menu presentation could lose its binding or show stale icons when an item,
  path, or remote property changed;
- the integration fixture did not exercise both registration forms, nested
  actions, live updates/removals, empty menus, and owner cleanup as one
  mandatory test.

## Implementation

- Centralized the `/StatusNotifierItem` default and tightened D-Bus name/path
  validation. Well-known names enforce the leading-digit rule; unique names,
  hyphens, long object paths, and root `/` are covered separately from the
  StatusNotifier item-path policy.
- Kept caller identity on the real `QDBusContext` watcher object and made both
  freedesktop and KDE adaptors expose zero-argument
  `StatusNotifierHostRegistered()`. Host verification checks the caller's
  unique owner and invalidates pending requests when that owner disappears.
- Added explicit DBusMenu lifecycle states. Presentation always performs the
  root handshake and initial layout request, accepts a valid empty root, and
  repeats the handshake for nested nodes. Typed signal teardown is symmetric.
- Added wire-compatible layout/property/event decoding, bounded PNG
  validation, named-icon fallback, revisioned icon replacement/removal, and
  recursive cascade-card presentation. The authoritative QML menu binding is
  reset only through the service context/menu-client lifecycle.
- Replaced the guarded fixture body with a deterministic isolated-session-bus
  test. The fixture registers service-only and path-only items from its own
  process, serves real item/menu calls, emits live updates and removals, and
  exits to prove owner cleanup.

## Verification

All commands below were run with the repository's RTK wrapper in a Debug,
no-layer-shell build:

- `statusnotifier-test`: 16 passed, 0 skipped.
- `statusnotifier-dbus-integration-test`: 1 mandatory Qt test passed, 0
  skipped; the CTest wrapper also passed with `dbus-run-session`.
- `bar-core-test`: focused tray-anchor coverage passed.
- `bar-qml-smoke-test`: 28 passed, 0 skipped.
- `git diff --check`: passed.
- The mandatory integration source and fixture contain no `QSKIP` path.

The broader repository CTest run was also executed in the same build
configuration: 62 tests passed, two configured tests were not runnable because
their executables were absent from the existing build tree, and two unrelated
pre-existing modified surfaces failed (`settings-navigation-model-test` and
`shell-unified-runtime-integration-test`). Release, sanitizers, and
layer-shell-enabled builds were not recreated for this closure.

## Result

M8-C.1.1 closes the real-bus registration/menu/action/cleanup path with a
zero-skip deterministic fixture while retaining the historical M8-C.1
qualification text unchanged.
