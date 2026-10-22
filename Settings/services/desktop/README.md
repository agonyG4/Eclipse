# Native desktop settings

Language, Apps and User Profile use focused Qt services. QML displays native state
and requests actions; filesystem, process and DBus operations run on serialized
worker jobs. System changes happen only after explicit UI actions. Errors remain
visible and no fallback data is presented as an installed application or account.

## Language and region

`RegionSettings` preserves the legacy `AstreaOS/system/settings.json` under the
XDG config directory. Patches use a file lock and atomic replacement, preserve
unknown fields and reject invalid documents without rewriting them. Refresh is
read-only. GeoClue changes use system authentication through `pkexec systemctl`;
unavailable controls are disabled. This preference controls permission to use
system geolocation, rather than guessing a country from the network.

`RegionalPreferences` watches the document and supplies regional formatting to
the Bar clock. Settings loads the selected translation catalogue at startup,
merging it over the English fallback. The migrated Portuguese catalogue covers
keys present in the legacy translation; other locales use English for missing
translations. Existing pages may require reopening to update their text.

## Applications

The installed list uses the existing `DesktopEntryParser` with XDG precedence
and hidden-entry masks. Package sizes and Flatpak permission information are
queried on selection. Native applications are explicitly reported as lacking
per-application sandbox controls. Shortcut creation refuses overwrites. Opening
a location uses the desktop URL handler, never a shell command.

Removal requires the page's confirmation dialog and validates the current
removal identity again. Flatpak removals retain app data; native package removal
uses authenticated Pacman without recursive dependency removal; local launcher
removal deletes only the user-owned desktop entry. Protected Settings launchers
and Steam game launchers cannot be removed through this page.

## Profile and authentication

Account identity comes from the current Unix uid. AccountsService supplies and
updates display name and avatar through authenticated DBus calls. Avatar input
must be a local, readable image with bounded file size and dimensions. The
password action opens `/usr/bin/passwd` through `xdg-terminal-exec`, so Settings
never receives passwords.

SDDM autologin uses `astrea-settings-profile-helper`, installed by CMake into the
configured libexec directory together with its Polkit policy. The helper is not
setuid: it requires an authenticated `pkexec` invocation and resolves its target
account exclusively from `PKEXEC_UID`. It accepts only enable/disable and an
installed session filename. It atomically writes the fixed Astrea SDDM drop-in,
respects configuration precedence and rejects conflicting main-file overrides
or autologin configured for another account. Autologin stays unavailable until
the helper, Polkit and SDDM are installed. Building does not install anything or
change the live machine's account, packages, GeoClue or SDDM configuration.

Tests use temporary XDG directories under the verified Aether build tree. They
cover persistence, corruption, unknown fields, desktop masks, non-overwriting
shortcuts, removal validation, regional consumers, SDDM precedence, name
validation and the three QML pages. Privileged account mutations are not applied
to the developer's real account during verification.
