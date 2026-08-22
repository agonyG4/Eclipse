# Native StatusNotifier bridge

`astrea-shared-statusnotifier` is the shell's native StatusNotifierItem and DBusMenu boundary. It owns one watcher bridge, one deterministic item model, one bounded icon store/provider, and one DBusMenu client per item. `ShellRuntime` owns the service for the process lifetime; each bar output receives the same service pointer and owns only its popup/tooltip presentation state.

The bridge supports the freedesktop and KDE watcher/interface aliases, normalizes service-only, path-only, and service/path registrations, and protects asynchronous replies with item generations. It prefers an existing watcher and can expose a compatible local watcher when no external owner is present. Item owner loss removes the model row, menu client, and icon cache.

Icon pixmaps are validated before conversion from network-order ARGB32. Selection is exact, then the smallest larger image, then the largest smaller image. Named icons use the already-applied Astrea/XDG theme and an item-local theme path without changing global theme state. QML consumes in-memory `image://astrea-tray/<key>?revision=<N>` URLs.

DBusMenu is parsed into bounded typed models. Labels remove mnemonic markers while preserving escaped underscores; nested children are exposed as child models, layout/property revisions are generation-based, and remote actions are sent only after the menu has loaded. Limits are intentionally conservative so malformed services cannot create unbounded work or memory use.
