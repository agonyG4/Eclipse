# Eclipse
AstreaOS Shell

Native components:

- `Spotlight/` provides the native Qt Quick launcher/search surface.
- `AltTab/` provides native window switching and identity resolution.
- `Dock/` provides the resident Qt Quick pinned-application Dock foundation.
- `shared/` provides the supervised application launcher, desktop-entry
  catalog, icon provider, and Layer Shell helper.

Dock Stage 1 is always-visible when enabled, bottom-centered, and Layer Shell
work-area reserving. It displays configured pins and launches through
`astrea-launch`. Typhon does not currently expose a public window-management
protocol, so running, active, and minimized Dock state is intentionally not
reported or simulated.
