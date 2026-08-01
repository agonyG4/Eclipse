# Native Dock Foundation Design

## Scope

Eclipse will gain an independent `astrea-dock` Qt 6 / Qt Quick resident
component. Stage 1 presents only configured desktop-entry pins, launches them
through the shared supervised `astrea-launch` path, reserves bottom work area
through LayerShellQt, and exposes a small local control/status interface. It
does not infer or display running, active, or minimized application state.

Typhon remains read-only. The future window-management seam is represented by
truthful model roles and a documented integration boundary, not by polling or
process heuristics.

## Architecture

`shared/` owns reusable application infrastructure:

- `launch/` contains the non-blocking supervised launcher used by Spotlight
  and Dock.
- `apps/` contains the immutable desktop-entry catalog and record identity
  rules shared by AltTab and Dock.
- `platform/wayland/` contains a configurable Layer Shell application helper.
- the shared QML icon implementation is authoritative; component QML imports
  it rather than maintaining divergent icon behavior.

Dock owns its product policy:

- `DockConfigWatcher` parses and validates `dock.json` and the `dock` component
  toggle, with independent field fallbacks and atomic-replacement recovery.
- `DockAppModel` stores configured pins in first-occurrence order, keyed by the
  full desktop filename. Catalog snapshots enrich rows without changing the
  stable pin identity.
- `DockController` applies configuration, routes clicks to the injected
  launcher, tracks launch state per desktop filename, and coordinates surface
  visibility.
- `DockLayerShellSurface` configures the Dock-specific scope, bottom anchor,
  margins, exclusive zone, and optional screen without owning policy.
- QML only binds presentation properties and invokes controller methods.

The surface is content-sized: the panel determines width and height, and no
full-output transparent input window is created. When disabled or empty, the
surface is unmapped and has no positive exclusive zone.

## Data Flow

Startup resolves runtime paths, loads validated configuration, initializes the
shared desktop catalog, populates the model, loads QML, then maps the Layer
Shell surface when enabled and non-empty. A click reaches the controller,
which sends the full desktop filename to the shared launcher. The launcher
invokes `astrea-launch` with an argument vector and emits bounded success,
failure, or timeout signals. No shell is used and the GUI thread remains
responsive.

Catalog rebuilds publish immutable snapshots. The Dock model updates only
affected rows and emits exact role lists. Unresolved configured pins remain
visible with fallback data and are enriched automatically after a later
catalog update.

## IPC

The resident server uses the versioned local socket `astrea-dock-v1`. Commands
are newline-delimited, bounded, and parsed from a fixed allow-list:
`status`, `reload`, `show`, `hide`, and `quit`. Status is serialized using
`QJsonObject` and `QJsonDocument`, including Layer Shell mapping state,
configuration revision, pin resolution, launch count, and the explicit
unavailability of Typhon public window integration.

## Testing

Tests are deterministic QtTest executables. Model tests cover stable row
identity and exact role changes. Controller tests use an injected fake
launcher. Config tests use temporary files and exercise clamps, independent
fallbacks, debounce, atomic replacement, and watcher recovery. IPC tests use a
temporary local socket and bounded client requests. Existing Spotlight and
AltTab tests remain part of the root CTest build.

## Alternatives Considered

1. Copy Spotlight and AltTab services into Dock. This is simple initially but
   would preserve divergent launcher, catalog, icon, and Layer Shell behavior.
2. Build one broad shell framework containing every component policy. This
   reduces files but couples unrelated lifecycle and protocol decisions.
3. Extract narrow shared services and retain component-specific policy. This
   is the selected approach because it removes duplication while keeping the
   Dock, Spotlight, and AltTab ownership boundaries explicit.
