# Native download progress without browser extensions

Investigation: 2026-10-19. Installed browsers: Brave 155.1.97.59 and Zen 1.23.3b
(Gecko 158.0, build 20261016101044). This document records experiments, not a
production native receiver implementation.

## Findings

| Browser/backend | Native channel | Result |
| --- | --- | --- |
| Brave, Unity/KDE environment compatibility | `com.canonical.Unity.LauncherEntry.Update` on session D-Bus | Confirmed with a real download and extensions disabled |
| Brave, native Wayland, Unity/KDE compatibility | Same session D-Bus channel | Confirmed with `--ozone-platform=wayland`, `DISPLAY` unset and extensions disabled |
| Brave, Astrea environment | Same candidate channel | No updates, even with libunity and a receiver present |
| Brave, KDE compatibility, no Unity bus-name owner | Same candidate channel | No updates |
| Zen under XWayland | `_NET_WM_XAPP_PROGRESS` X11 CARDINAL property | Confirmed with a real download and no extensions loaded |
| Firefox native Wayland implementation examined | GTK taskbar adapter calls a default no-op `nsWindow::SetProgress` | No equivalent native Wayland output found in this path; this is source evidence, not a runtime proof excluding every possible channel |

The native data is aggregate application/window progress. It does not identify
individual download files, filenames, per-file byte counts or distinct terminal
reasons. It should be represented as application progress, rather than fabricated
per-file download snapshots in the existing API.

## Brave experiment

The installed Brave binary contains the libunity integration symbols. The system's
dynamic-loader cache did not contain libunity. For the experiment only, official
Debian libunity9, libunity-protocol-private0 and libdee packages were extracted to
the existing Aether build tree. They were not installed into system library paths.

The actual libunity implementation was used, not a shim. Its inspector checks
ownership of `com.canonical.Unity` on the session bus. A small Python/Gio receiver
owned that name on a dedicated private bus and subscribed to
`com.canonical.Unity.LauncherEntry.Update`.

A clean Brave profile, with `--disable-extensions`, downloaded a 4 MiB fixture from
a local HTTP server. The browser-only test environment used `XDG_CURRENT_DESKTOP=KDE`,
`KDE_SESSION_VERSION=6`, and a private `LD_LIBRARY_PATH` containing the extracted
runtime libraries (including their libunity subdirectory). No global desktop or
browser-profile settings were changed.

Observed URI: `application://brave-browser.desktop`.
Observed progress fractions:

```text
0.109375 0.203125 0.296875 0.40625 0.5
0.59375 0.6875 0.796875 0.890625 0.984375
```

Final updates included `progress: 1`, `progress-visible: false`, followed by
`count: 0`, `count-visible: false`, `progress: 0`. Property dictionaries are partial
updates and must be merged. Loss of the publishing bus owner must clear its state.

Control runs completed the same local file download but emitted no launcher updates:

- `XDG_CURRENT_DESKTOP=Astrea`, libraries present, Unity name owned;
- KDE compatibility, libraries present, no Unity name owned.

The current session reports Astrea. Typhon's launcher defaults to OblivionOne when
the desktop variables are unset. Neither passes the upstream Chromium environment
guard. A receiver alone therefore does not enable this in the current session.

## Zen experiment and source trace

The installed Zen taskbar module obtains `nsIGtkTaskbarProgress` and attaches the
primary browser window. The examined Firefox implementation maps summary progress
to a window percentage. X11 publishes `_NET_WM_XAPP_PROGRESS`; zero deletes the
property. The current base window method is empty and the examined Wayland class
does not override it. The installed Zen binary contains the XApp property string;
its literal Unity LauncherEntry interface/library names were absent (supporting
evidence, not a general proof about all possible runtime behavior).

A fresh Zen profile was launched with `MOZ_ENABLE_WAYLAND=0` against the current
Typhon XWayland display. No extension was installed. Marionette initiated a normal
link click to the local 4 MiB ZIP fixture. `xprop` on that test browser window
observed percentages:

```text
3 10 18 26 30 34 37 41 42 46 50 53 57 61 64 68
```

The target file finished downloading. These values prove the live XWayland channel;
the experiment does not claim that the property reached 100 or independently
distinguished completion from failure/cancellation.

## Follow-up: no XWayland

A second Brave experiment explicitly selected the headed native Wayland backend
with `--ozone-platform=wayland`, removed `DISPLAY` from the child environment and
enabled client Wayland protocol logging. The log contained `wl_display` traffic.
The fresh profile again used `--disable-extensions`, the private session bus and
the same isolated libunity/KDE compatibility settings described above.

The 4 MiB local download completed. LauncherEntry updates included the same
intermediate fractions from 0.109375 to 0.984375 and a final progress value of 1.
Thus the demonstrated Brave publisher does not require XWayland. Its remaining
requirements are libunity, a compatible desktop-detection result and ownership of
the Unity compatibility name on the session bus. This experiment does not mean
the stock Astrea session already satisfies those conditions.

For Firefox, the follow-up fetched and inspected the current upstream
`TaskbarProgress.cpp`, `nsWindow.h`, `nsWindowWayland.h` and
`nsWindowWayland.cpp`. The adapter calls `mPrimaryWindow->SetProgress(progress)`;
the base implementation is empty, and neither examined Wayland file implements
an override. This establishes a gap in that specific native publisher, rather
than a general limitation of Wayland or D-Bus. A Zen-side publisher could emit
LauncherEntry over D-Bus while the browser remains a native Wayland client.
The installed Zen build was not rebuilt or patched in this investigation.

KDE's documented [Plasma Browser Integration](https://community.kde.org/Plasma/Browser_Integration)
uses a browser extension plus a native host, so it does not provide an existing
extension-free solution for this Zen requirement. A search of Zen's repository
tree through the GitHub API was rate limited; no exhaustive claim about all
Zen-specific patches is made here.

The practical starting point without XWayland or a browser extension is therefore
Brave's aggregate application progress. For stock Zen on native Wayland, this
investigation has not found a ready publisher; a browser change remains a possible
route. Merely watching a growing `.part` file cannot supply the expected total
size or reliably distinguish all download states.

## Integration implications

1. Add an application-progress contract to the headless Eclipse API: desktop/app
   identity, optional window identity, progress, visibility and optional count.
2. Implement Unity LauncherEntry reception in an Eclipse session service, preserving
   publisher ownership and partial-update semantics. Acquire the compatibility name
   only when available; do not take it away from another desktop implementation.
3. For Brave, provide libunity through normal packaging and resolve its desktop
   detection guard. An app-specific launch environment is an option; a browser
   change recognizing Astrea is another. Changing the whole session to KDE is not
   required by the demonstrated mechanism and is not proposed here.
4. For Zen via XWayland, extend Typhon's XWM property handling and toplevel metadata
   to carry the XApp percentage to Eclipse. The examined `XwmAtomName`,
   `PropertyKind`, `X11PropertySnapshot`, and `DockAppInfo` definitions do not yet
   represent this progress field.
5. Keeping Zen on native Wayland requires a browser-side publisher/patch for this
   path, or retaining the extension bridge. Adding only a dock listener cannot
   manufacture data absent from the window's native output.

A first extension-free indicator is feasible under these conditions. Exact per-file
ZIP presentation remains a separate integration requiring richer browser data.

## Evidence and limitations

- [Chromium Linux updater](https://github.com/chromium/chromium/blob/main/chrome/browser/download/download_status_updater_linux.cc)
- [Firefox GTK taskbar adapter](https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/TaskbarProgress.cpp)
- [Firefox base GTK window](https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/nsWindow.h)
- [Firefox X11 window implementation](https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/nsWindowX11.cpp)
- [Firefox Wayland window implementation](https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/nsWindowWayland.cpp)
- [Firefox Wayland window declaration](https://github.com/mozilla-firefox/firefox/blob/main/widget/gtk/nsWindowWayland.h)
- [KDE browser integration requirements](https://community.kde.org/Plasma/Browser_Integration)
- [Official Debian libunity source/runtime packages](https://deb.debian.org/debian/pool/main/libu/libunity/)

Research scripts, packages and extracted libraries are under
`/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/native`. Temporary profiles,
downloaded fixtures and private D-Bus/browser processes were cleaned up after runs.
No production native receiver or compositor change was implemented in this study.
The extension-based implementation remains a separate existing integration.

Codebase graph verification used Typhon generation 2026-10-19T18:57:09Z and Eclipse
generation 2026-10-16T19:37:04Z with direct source reads for material definitions.
Coverage checks reported no recorded gap in the cited XWM/header paths. Typhon's
launcher had recorded shell-parser gaps at lines 150/152; those lines were read
directly. Coverage is best-effort and this is a bounded investigation, not a complete
audit of either repository or every browser integration mechanism.
