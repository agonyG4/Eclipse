# Behavioral Contract

M8-A is accepted by behavior, not by source similarity.

## Top reserved area

- Every active output reserves exactly 45 logical pixels at the top through Layer Shell exclusive zone.
- The reserve surface is visually transparent.
- The reserve surface does not consume pointer/touch input.
- Removing the Bar releases the exclusive zone cleanly.

## Launcher pill

- Top margin: 5 px.
- Left margin: 8 px.
- Visible pill height: 36 px.
- Astrea button uses the supplied Astrea logo asset.
- The pill preserves the current Borealis-like rounded shell appearance.
- Hover/pressed/active feedback remains subtle and animated.
- Workspace visual contract supports active-width expansion and dot states.
- Until a real provider exists, no fake production workspaces are displayed.

## Status pill

- Top margin: 5 px.
- Right margin: 6 px.
- Height: 36 px.
- Width is content-driven but must never cross into the launcher region plus a 28 px minimum gap.
- M8-A must at minimum render the native clock correctly.
- Unimplemented system indicators are hidden rather than displaying made-up state.

## Clock

- Time updates without polling external commands/files.
- Updating at minute boundaries is sufficient for a minute-resolution display.
- Date/day/month formatting must be deterministic and testable.
- Do not depend on the legacy Python region helper in the Eclipse runtime.
- The visible format should match the current reference as closely as the native i18n/locale model allows.

## Astrea menu

- Opens from the Astrea button in a TopBar popup card.
- Search invokes the existing native Spotlight controller.
- Settings uses the existing native application-launch path if available.
- Actions without a real native backend in the current Eclipse/Typhon snapshot must be disabled or hidden with an explicit capability flag; they must not fall back to `hyprctl`, `quickshell`, or an arbitrary shell string.
- Clicking outside closes the menu.
- Popup opening/closing preserves the reference opacity/scale feel.

## Popup overlay

- Exactly one popup is active per output.
- Opening a second popup replaces/closes the first deterministically.
- Popup card horizontal position is centered on the originating indicator and clamped to output side padding.
- Default top offset is 54 px.
- Outside click closes.
- Clicks inside the card do not trigger outside close.
- The full-output overlay is unmapped when no popup is active.
- Popup state is destroyed on output removal.

## Multi-output

- A Bar bundle exists for every Qt `QScreen` exposed by the Wayland session.
- Each bundle is explicitly assigned to its own screen before Layer Shell configuration.
- Adding an output creates exactly one new bundle.
- Removing an output destroys exactly one bundle and leaves remaining outputs stable.
- No Bar surface silently falls back to `primaryScreen()` ownership.
- Primary-screen changes do not duplicate or orphan Bar surfaces.

## Existing shell behavior

The task must not regress:

- Dock mapping/configuration;
- Alt+Tab;
- Spotlight;
- Typhon shared connection/authentication;
- shell IPC;
- application catalog/indexing;
- CI/sanitizer builds.

## Performance and lifecycle

- No periodic child processes are introduced by M8-A.
- No filesystem status-polling loop is introduced.
- Hidden popup surfaces consume no animation timer work beyond what is needed for normal Qt object state.
- Surface creation/destruction must be deterministic and leak-free under ASan/LSan where available.
