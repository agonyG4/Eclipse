# Native StatusNotifier bridge

`astrea-shared-statusnotifier` is the shell's native StatusNotifierItem and
DBusMenu boundary. `ShellRuntime` owns one `StatusNotifierService` for the
process lifetime; each bar output receives that service and owns only popup,
tooltip, and geometry presentation state. The service maintains one watcher
bridge, item model, proxy, icon store, and DBusMenu client per registered item.

## Registration and ownership

The watcher accepts all three StatusNotifier registration forms:

| Registration | Normalized service | Normalized object path |
| --- | --- | --- |
| `service` | supplied well-known or unique name | `/StatusNotifierItem` |
| `/object/path` | the caller's unique name | supplied path |
| `service/object/path` | supplied service | supplied path |

Service names use the D-Bus grammar: at least two dot-separated components,
ASCII letters/digits/underscore/hyphen, a 255-byte maximum, and no leading
digit in well-known components. Unique names begin with `:` and follow the
unique-name component rules. Object paths accept `/` and have no artificial
255-byte cap; StatusNotifier item registration still rejects `/` because an
item must identify a concrete object.

The local watcher exports both freedesktop and KDE aliases. Its public
`StatusNotifierHostRegistered()` signal has no arguments. D-Bus caller identity
is read from the real exported watcher object, not from an adaptor, so
path-only registrations and host ownership are attributed to the caller's
unique name. Well-known host registration is accepted only after resolving
the caller's owner; owner loss removes its items, hosts, pending host
verifications, menus, proxies, and icon state.

## DBusMenu lifecycle

`DBusMenuClient::state` distinguishes `Unavailable`, `Unloaded`, `Loading`,
`Ready`, `Empty`, `Error`, and `Stopped`. Presentation always performs the
root `AboutToShow` handshake and an initial root `GetLayout`, even when the
remote service reports no update. A valid empty root is a successful `Empty`
state, not a failed or unavailable menu. Submenus repeat the same handshake
for their node and are bounded by the shared depth/node/child/label limits.

The model handles the Qt D-Bus wire forms for layout replies, typed
`ItemsPropertiesUpdated` updates/removals, `LayoutUpdated`, and `Event`. Menu
activation is sent with the required event variant. Signal connections and
disconnects use the same exact typed signatures.

## Icons and QML presentation

Icon precedence is bounded pixmap data, then a valid bounded PNG image, then a
named icon resolved through the existing Astrea/XDG theme path. Icon revisions
invalidate `image://astrea-tray/<key>?revision=<N>` sources; replacement and
removal clear stale images. QML binds the authoritative menu model from the
service, resets the presentation state on context changes and reopen, keeps
the native popup available while the model is loading, and uses the ordinary
context menu only when no native menu is exposed.

The real-session-bus fixture and integration test exercise service-only and
path-only registration, item actions, tooltip and icon decoding, root and
nested menu actions, live property updates/removals, empty-menu handling, and
owner cleanup without a skip path.
