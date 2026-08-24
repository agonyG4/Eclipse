# M8-A Agent Task — Native Eclipse TopBar Foundation and Visual Shell Port

You are working on the supplied Eclipse source snapshot in `target/Eclipse/`.

Your task is to implement **M8-A — Native TopBar Foundation and Visual Shell Port** inside the unified `astrea-shell` runtime.

Do not ask the user architecture questions. The migration kit is the specification. If a small implementation detail is not explicitly fixed, choose the option that best preserves Eclipse's existing ownership model, deterministic tests, and Layer Shell correctness.

## Mandatory reading before editing

Read, in order:

1. `README.md`
2. `docs/00_SOURCE_BASELINE.md`
3. `docs/01_CURRENT_TOPBAR_AUDIT.md`
4. `docs/02_TARGET_ARCHITECTURE.md`
5. `docs/03_PORT_MAP.md`
6. `docs/04_BEHAVIORAL_CONTRACT.md`
7. `docs/05_LEGACY_REPLACEMENT_MATRIX.md`
8. `docs/06_TEST_AND_VALIDATION_PLAN.md`
9. `docs/07_RISKS_NON_GOALS_AND_DECISIONS.md`
10. `docs/08_FOLLOW_UP_ROADMAP.md`

Use `reference/astreaos/` as the visual/behavioral reference and `target/Eclipse/` as the architecture/build authority.

## Core objective

After this task, `astrea-shell` must own a native per-output TopBar foundation that:

- reserves 45 logical pixels at the top of every output;
- renders the current Astrea left and right floating-pill layout using Qt Quick;
- creates and destroys surfaces correctly as `QScreen`s appear/disappear;
- uses the existing LayerShellQt integration through `AstreaLayerShellHelper`;
- includes a native clock;
- includes the Astrea menu visual and functional Search integration;
- launches Settings through the existing Eclipse application catalog/launcher path when resolvable;
- has a generic per-output popup overlay infrastructure;
- exposes a compositor-neutral workspace model contract without inventing a Typhon protocol;
- contains no Quickshell dependency and no shell-command/file-cache integration in production Bar QML.

This task is a foundation milestone, not a claim of full current-TopBar feature parity. System tray, network, Bluetooth, audio, Control Center, notification history, volume OSD, and real Typhon workspace data are explicitly follow-up work.

## Required architecture

Implement the equivalent of:

```text
ShellRuntime
├── BarController
├── BarClockService
├── WorkspaceModel
└── existing shell services/controllers

AstreaShellApplication
└── BarSurfaceManager
    └── BarSurfaceBundle per QScreen
        ├── ReserveSurface
        ├── LauncherSurface
        ├── StatusSurface
        └── PopupOverlaySurface
```

You may adjust exact class/file names to match project conventions, but do not collapse surface ownership into QML-only global magic and do not place QQuickWindow ownership into low-level system-service code.

## Surface requirements

### Reserve

- Layer: Top.
- Anchors: top + left + right.
- Exclusive zone: 45.
- Height: 45.
- Transparent.
- Keyboard interactivity: none.
- Must be input-transparent/click-through.

### Launcher

- Layer: Top.
- Anchors: top + left.
- Exclusive zone: -1.
- Left margin: 8.
- Top margin: 5.
- Height: 36.
- Keyboard interactivity: none.

### Status

- Layer: Top.
- Anchors: top + right.
- Exclusive zone: -1.
- Right margin: 6.
- Top margin: 5.
- Height: 36.
- Keyboard interactivity: none.
- Content width must be capped so it cannot overlap the launcher region plus the 28 px minimum gap.

### Popup overlay

- Layer: Overlay.
- Anchors: all four edges.
- Exclusive zone: -1.
- Hidden/unmapped while no popup is active.
- Outside click closes the active popup.
- Popup card click must not close through to the outside handler.
- Card default top offset: 54.
- Card x-position: centered around caller anchor, clamped to output side padding.
- Only one active TopBar popup per output.

Configure Layer Shell **before first map** for every surface. Assign the `QScreen` before the LayerShellQt window wrapper is created.

## Multi-output requirements

Do not use `QGuiApplication::primaryScreen()` as Bar ownership.

Create one surface bundle per actual `QScreen` and subscribe to screen add/remove lifecycle.

Output removal must destroy the complete bundle without stale `QScreen` pointers, stale popup state, duplicate signal connections, or leaked windows.

## Visual port requirements

Use the supplied AstreaOS Bar as the reference for:

- 45 px reserved height;
- 36 px floating pills;
- spacing and rounding;
- Astrea logo;
- hover/pressed/active states;
- workspace dot/active-pill animation contract;
- popup opacity/scale feel;
- popup placement.

Do not use the task as an excuse to redesign the Bar.

At the same time, do not blindly copy Quickshell-specific implementation code. Re-express it in Qt Quick/native Eclipse patterns.

Package the Astrea logo as a Qt resource. Do not require `ASTREA_ROOT` file URLs for the Bar.

## Workspace contract

Create a compositor-neutral model with roles equivalent to:

```text
id
active
occupied
urgent
outputId (optional until the compositor exposes it)
```

The production model may be empty in M8-A. This is intentional.

Provide deterministic fake/test data for unit/QML tests so the visual active/dot states are covered.

Do not:

- import `Quickshell.Hyprland`;
- execute `hyprctl`;
- invent an Astrea/Typhon wire protocol in this task;
- expose fake production workspaces.

## Native clock

Replace the legacy region/file/process path with a native Qt implementation.

Requirements:

- no subprocess;
- no watched JSON file;
- update at minute boundaries or equivalent efficient cadence;
- deterministic format tests;
- native locale/i18n integration;
- visually match the current clock presentation as closely as practical without reintroducing the Python helper.

The notification-history popup behind the clock is not part of this task; capability-gate that interaction.

## Astrea menu actions

Port the current menu visual.

Implement:

- **Search** -> existing `SpotlightController::show()` path.
- **Settings** -> resolve and launch the Astrea Settings desktop entry through the existing `DesktopEntryCatalog` + `ApplicationLauncher` path when available.

For About, Force Quit, Lockscreen, and Power:

- expose explicit capability state;
- disable/hide unsupported actions;
- do not use `quickshell`, `hyprctl`, `shutdown`, or arbitrary shell command fallback.

If an existing safe Eclipse-native path already exists for one of those actions, reuse it only after proving it from the supplied target source.

## Legacy integration prohibition

Production Bar QML introduced by this task must not contain:

```text
import Quickshell
import Quickshell.Io
import Quickshell.Hyprland
import Quickshell.Services.SystemTray
Process {
FileView {
IpcHandler {
wpctl
nmcli
bluetoothctl
hyprctl
ddcutil
quickshell
```

Do not make `astrea-shell` depend on `astrea_statusd.py` or `bluetooth_manager.py`.

Add a static test/check that enforces this for the new Bar production QML.

## Theme requirements

Do not create an unrelated third shell palette.

The reference Bar already uses Borealis shell tokens, and Eclipse Settings contains corresponding shell color/radius values. Establish a reusable shell-Bar token layer suitable for future Control Center/OSD migration.

Do not redesign existing Dock/Spotlight visuals in this task unless a tiny build-only change is unavoidable.

## Shell runtime/status integration

Add Bar runtime state to the unified shell rather than creating another daemon.

Extend shell diagnostics/status JSON with stable fields such as:

- Bar enabled;
- number of active Bar output bundles;
- whether a popup is open;
- Layer Shell configuration requested/healthy state if appropriate.

Do not expose pointer addresses or unstable internal IDs.

Do not break the existing shell IPC schema without versioning/compatibility consideration.

## Testing requirements

Follow `docs/06_TEST_AND_VALIDATION_PLAN.md`.

At minimum add deterministic coverage for:

- Layer Shell surface policy;
- geometry/non-overlap;
- workspace model;
- clock formatting/update scheduling;
- popup state machine;
- ShellRuntime Bar ownership;
- QML smoke loading;
- prohibited legacy integrations;
- output bundle lifecycle logic that can be tested without a physical hotplug.

Then run the existing full test suite, not only Bar tests.

Reuse a single build directory per configuration. Do not repeatedly create fresh build trees under `/tmp` or elsewhere.

If live Wayland/multi-output hardware is available, perform the real Layer Shell checks in the validation plan. If it is not available, clearly mark those checks as not executed instead of claiming them.

## Engineering constraints

- Preserve existing Eclipse/Typhon architecture.
- Prefer clear ownership and RAII.
- No background daemon for the Bar.
- No duplicate Wayland connection for Bar state.
- No ordinary Qt window fallback if Layer Shell fails.
- No hidden compatibility shim that keeps Quickshell alive.
- No broad unrelated refactor.
- No speculative system-service implementation beyond what M8-A requires.
- Keep source files reasonably scoped by responsibility.
- Add documentation for the new architecture and the deferred M8-B+ integration points.

## Completion standard

The task is complete only when:

1. the full deterministic test suite passes;
2. the Bar is integrated into the unified `astrea-shell` executable;
3. every active output gets the correct surface bundle;
4. the reserve zone is 45 px and input-transparent;
5. the two floating pills have the reference geometry;
6. the native clock works;
7. Astrea Search opens the existing Spotlight;
8. Settings uses the native launcher path when available;
9. the popup overlay is deterministic and output-local;
10. there are no prohibited legacy dependencies in production Bar QML;
11. existing Dock/Alt+Tab/Spotlight behavior remains intact;
12. documentation accurately states what is implemented and what remains for M8-B+.

## Final report format

When finished, provide a concise engineering report containing:

- root causes/design gaps found in the pre-M8-A shell;
- architecture implemented;
- important files changed;
- behavior preserved from AstreaOS;
- intentional differences from the legacy implementation;
- test/validation commands with exact results;
- live Wayland/hotplug checks performed or not performed;
- remaining M8-B+ work;
- commit hash if you committed the work.

Do not claim full TopBar parity until tray/network/Bluetooth/audio/Control Center/notifications/workspaces have their native follow-up implementations.
