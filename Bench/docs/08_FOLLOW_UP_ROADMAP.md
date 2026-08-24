# Follow-Up Roadmap

The following milestones are intentionally outside the immediate M8-A implementation, but the M8-A architecture must make them additive rather than requiring another shell rewrite.

## M8-B — Shared Native System Runtime

Add typed shared services under the unified shell runtime.

Priority order:

1. Audio service;
2. Network service;
3. Bluetooth service;
4. Media/MPRIS service;
5. power and brightness services.

Target rule:

```text
TopBar ─────────────┐
Control Center ─────┼── SystemRuntime services
Island/OSD ─────────┤
Settings ───────────┘
```

There must not be separate state authorities for the same subsystem.

## M8-C — Native System Tray

Implement a real StatusNotifierItem host and menu model.

Behavioral target from the reference:

- icon;
- tooltip;
- activate;
- secondary activate;
- context menu;
- separators;
- check states;
- nested menus.

## M8-D — TopBar Popups and Control Center

Port the existing popup visuals/modules onto the shared native services.

Preserve the current modular Control Center idea, but replace process/file-based actions with typed service calls.

## M8-E — Typhon Workspace Integration

When Typhon exposes the finalized workspace protocol/model:

- bind it to the M8-A `WorkspaceModel` boundary;
- support Super+1..N compositor actions in Typhon rather than shell-side Hyprland dispatch;
- track active/occupied/urgent state;
- add output identity when multi-output workspace behavior is defined;
- preserve the current animated active-width workspace indicator.

Do not redesign the TopBar QML for this milestone; only connect the provider.

## M8-F — Notifications and OSD

Move notification ownership into Eclipse/Astrea native runtime and connect:

- notification history from the clock;
- volume OSD;
- future brightness/media OSDs;
- secure popup/output placement.

## M8-G — Island/Media Integration

Reuse the same MediaService/system state to migrate the remaining shell island/media experience without duplicating process monitors.

## M8-H — Shell polish and cross-component theme consolidation

After the major components are native:

- unify Borealis shell tokens;
- remove duplicated visual constants from Dock/Spotlight/Bar;
- establish common popup/animation primitives where they are genuinely shared;
- keep component-specific geometry separate where needed.
