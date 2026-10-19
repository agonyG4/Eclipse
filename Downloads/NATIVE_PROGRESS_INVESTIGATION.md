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
| Zen, native Wayland, isolated AutoConfig customization | Internal public Downloads view to the existing API's Unix socket | Confirmed per-file name, bytes and completion; no extension or automation port in final runs |
| Firefox native Wayland implementation examined | GTK taskbar adapter calls a default no-op `nsWindow::SetProgress` | No equivalent native Wayland output found in this path; this is source evidence, not a runtime proof excluding every possible channel |

The built-in XApp/LauncherEntry data is aggregate application/window progress. It does not identify
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

## Zen native Wayland: tested browser customization

A follow-up tested the installed Zen version on native Wayland with
`MOZ_ENABLE_WAYLAND=1`, `GDK_BACKEND=wayland` and `DISPLAY` removed from the child
environment. Client protocol logging confirmed `wl_display` traffic. The stock
browser's GTK taskbar logger reported changing percentages, including 3, 10, 18,
26, 34, 42, 50, 57, 65, 73, 81, 89 and 96. A private-bus LauncherEntry receiver
owned the Unity compatibility name but observed no progress signals. This runtime
check supports the examined taskbar-path limitation; it does not exclude every
other possible browser integration.

An alternative was then tested without rebuilding Zen: a copy of the **installed
runtime**, placed under the existing Aether build tree, loaded a local AutoConfig
script. The script subscribes to `Downloads.getList(Downloads.PUBLIC).addView()`
and maps download changes into the existing JSON Lines API. It sends records
through `nsISocketTransportService.createUnixDomainTransport()`. No extension,
native-messaging host, libunity or desktop-environment spoofing is used by this
publisher. This is browser-side customization, not a capability enabled by merely
adding a dock listener.

The initial proof used Marionette to click a link, but the final four test cases
launched the browser directly with a local download URL. They did **not** pass
`--marionette`, `--remote-allow-system-access` or a remote-debugging flag. AutoConfig
loads the publisher at normal startup; automation is not its delivery mechanism.

Each final case used a fresh profile and a valid ZIP containing a 4 MiB member
(4,194,424 bytes including archive headers). The downloaded archive was checked
with `ZipFile.testzip()`. The API ran on a separate temporary Unix socket.

| Case | Result |
| --- | --- |
| Unmodified installed Zen, public download | ZIP completed; no records appeared on the test API |
| AutoConfig, public download with Content-Length | 15 distinct sampled records, intermediate fractions, correct filename, final `completed`, `progress: 1`, 4,194,424 received/total bytes |
| AutoConfig, public download without Content-Length | 14 distinct sampled records, `totalBytes: null` and unknown progress while transferring; final `completed`, `progress: 1`, 4,194,424 received bytes |
| AutoConfig, private-window download | ZIP completed; zero records appeared on the test API |

The first no-automation run stopped at a diagnostic assertion because `dump()`
logging was disabled in the fresh normal profile. The probe then enabled
`browser.dom.window.dump.enabled` to observe startup markers. All four final cases
passed; the publisher itself did not need a transport change for this diagnostic.
The completion mapping uses the browser's final `target.size`, because its
throttled `currentBytes` value was still below the actual file size in an earlier
completion notification.

This proves a route to per-file progress on native Wayland **without an extension**,
provided that the browser installation is customized. It does not establish
extension-free capture from an entirely unmodified Zen installation. AutoConfig
requires installation-level files and, for this privileged script, sets
`general.config.sandbox_enabled=false`; this is the AutoConfig script sandbox,
not the web-content process sandbox. The installed `/opt/zen-browser-bin` runtime
and the user's normal browser profile were not modified.

The publisher is a research prototype. It uses internal browser APIs and does not
yet implement acknowledged delivery, reconnect/replay, stable profile identity,
removal synchronization or bounded outbound queues. Pause/resume, failure,
cancellation and multiple concurrent downloads were not validated in these runs.
Packaging a maintained browser integration or carrying a browser patch remains
separate implementation work.

Research artifacts in `/mnt/Aether/Desktop/GitHub/Eclipse-build/Downloads/native`:

- `live-zen-wayland.py`: stock native-Wayland taskbar/D-Bus probe;
- `zen-autoconfig-runtime/`: isolated runtime copy and its two AutoConfig files;
- `zen-autoconfig-publisher.cfg`: the experimental publisher;
- `probe-zen-autoconfig.py`: four-case probe without an automation port;
- `zen-autoconfig-results.json`: successful final probe results.

Profiles, downloaded ZIPs, API processes and test browser processes were cleaned
up. No source repository was copied and no compilation was performed.

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
5. Keeping Zen on native Wayland requires a browser-side publisher for this path,
   such as the tested AutoConfig customization, a maintained browser patch or the
   existing extension bridge. Adding only a dock listener cannot manufacture data
   absent from the window's native output.

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
- [Mozilla AutoConfig setup](https://support.mozilla.org/en-US/kb/customizing-firefox-using-autoconfig)
- [Firefox AutoConfig loader and sandbox preference](https://github.com/mozilla-firefox/firefox/blob/main/extensions/pref/autoconfig/src/nsReadConfig.cpp)
- [Firefox internal download-list views](https://github.com/mozilla-firefox/firefox/blob/main/toolkit/components/downloads/DownloadList.sys.mjs)
- [Firefox Unix socket transport interface](https://github.com/mozilla-firefox/firefox/blob/main/netwerk/base/nsISocketTransportService.idl)
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
