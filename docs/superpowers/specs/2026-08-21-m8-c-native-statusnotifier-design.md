# M8-C Native StatusNotifier and DBusMenu Design

## Approval and scope

The attached M8-C brief is the approved design for this implementation. The work is limited to the native shell tray: StatusNotifierWatcher/StatusNotifierItem discovery, DBusMenu rendering, icon decoding and delivery, bar integration, and the required tests and documentation. It does not add a notification center, Control Center, workspace UI, XEmbed, Quickshell, or subprocess-based helper.

## Architecture

`astrea-shared-statusnotifier` owns one `StatusNotifierService` for the shell process. It contains the protocol value types, watcher bridge, item registry/model, typed item proxies, DBusMenu client/model, bounded icon store, and `QQuickImageProvider`. `ShellRuntime` owns the service for its whole lifetime. `AstreaShellApplication` registers the provider with the same QML engine, and `BarSurfaceManager` passes the same service to every output bundle.

The watcher bridge first observes the freedesktop and KDE watcher names. An existing watcher is never replaced. If no compatible watcher is present, the bridge races ownership of the freedesktop and KDE aliases around one registry object; losing either race releases the partial ownership and falls back to the external owner. If both names resolve to one owner they are one logical watcher. If they resolve to different owners, the deterministic freedesktop-first selection remains active and health reports the conflict.

Every registration is normalized to `{service, objectPath, uniqueOwner}`. Service-only registrations use `/StatusNotifierItem`; path-only registrations use the sender's unique owner; combined registrations split the service and path. The registry key is stable for the current owner/generation. A re-registration creates a new generation, and all asynchronous callbacks capture that generation, object path, and interface identity so stale replies cannot mutate a replacement item.

The item registry is a deterministic `QAbstractListModel`. It keeps Passive items in the model, but the tray presentation filters to ready items only. The model exposes typed roles (`key`, `id`, `title`, `category`, `status`, `iconSource`, `tooltipTitle`, `tooltipDescription`, `hasMenu`, `onlyMenu`, and `ready`). Per-output state is restricted to popup/tooltip/context identity; item discovery, icon revisions, and menu clients are global.

## D-Bus and asynchronous boundaries

All remote calls use `QDBusPendingCallWatcher` or signal subscriptions; the GUI thread never waits for a remote D-Bus reply. The watcher exposes both protocol dialects and the compatible properties/methods required by the brief. Item proxies try freedesktop then KDE interfaces, refresh `Properties.GetAll`, and subscribe to property and item signals. Owner disappearance removes the item, disposes its menu client and icon cache, and asks each output to close any matching popup context.

DBusMenu is represented as a bounded typed tree. `GetLayout` is parsed recursively with explicit depth, node, child, label, and icon-data limits. `LayoutUpdated` and `ItemsPropertiesUpdated` advance a generation and invalidate only the affected model. `AboutToShow` is awaited before opening a submenu; clicks are sent as typed events and no toggle state is changed optimistically. QML receives models and scalar roles, never raw D-Bus proxies.

## Icon and tooltip pipeline

Named icons resolve through the existing Astrea/XDG icon theme setup, with an item-local `IconThemePath` searched without mutating the process-wide theme. Pixmaps are decoded from network-byte-order ARGB32 bytes after validating dimensions, overflow, payload size, and per-item cache bounds. Selection is exact, then smallest larger, then largest smaller; attention and overlay data are composed as a bounded image. The provider serves `image://astrea-tray/<item-key>?revision=<N>` directly from memory and uses no temporary files.

The bar's tray delegate emits typed tooltip requests. A small, input-transparent, non-exclusive `TrayTooltipSurface` is mapped only while a tooltip is visible, below the bar and clamped to the output. Popup menus use the existing overlay surface with `PopupKind::TrayMenu`, a stable item context key, bounded nested cards, keyboard-safe labels, and a clear no-actions state.

## Failure and compatibility policy

Unavailable watcher/service state is explicit and observable in `healthJson()`. It does not prevent the bar from loading. Re-enabling the bar does not restart or destroy the global service. Malformed registrations, unsupported item data, oversized pixmaps/menu nodes, owner loss, and failed calls become bounded item/menu errors rather than crashes. The implementation retains compatibility aliases and does not depend on a particular desktop daemon.

## Verification strategy

Pure normalization, ARGB32 decoding, menu parsing, bounds, generation, and action-routing tests run without a session bus. D-Bus integration tests use an optional private session-bus fixture when available. QML tests exercise production resources and popup lifecycle. Build and test qualification records Debug, Release, Clang, ASan, UBSan, no-Typhon, and no-layer-shell variants when the local environment provides those toolchains, with unrun variants called out explicitly.
