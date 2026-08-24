# Astrea Eclipse TopBar Port Kit

Date: 2026-08-18

This kit is a closed migration package for porting the current AstreaOS TopBar from Quickshell into the native Eclipse `astrea-shell` runtime.

The package deliberately separates three things:

1. **Reference behavior** — what the current AstreaOS TopBar looks like and how it behaves.
2. **Legacy implementation details** — Quickshell-specific bridges, shell commands, JSON polling, and Hyprland bindings that must not be copied into the native Eclipse implementation.
3. **Target architecture** — the native Qt/C++/QML structure the Eclipse implementation should converge on.

The executable task for the coding agent is `AGENT_TASK.md`. Read the documents in this order before editing code:

1. `docs/00_SOURCE_BASELINE.md`
2. `docs/01_CURRENT_TOPBAR_AUDIT.md`
3. `docs/02_TARGET_ARCHITECTURE.md`
4. `docs/03_PORT_MAP.md`
5. `docs/04_BEHAVIORAL_CONTRACT.md`
6. `docs/05_LEGACY_REPLACEMENT_MATRIX.md`
7. `docs/06_TEST_AND_VALIDATION_PLAN.md`
8. `docs/07_RISKS_NON_GOALS_AND_DECISIONS.md`
9. `docs/08_FOLLOW_UP_ROADMAP.md`
10. `AGENT_TASK.md`

## Package layout

```text
Astrea_Eclipse_TopBar_Port_Kit_2026-08-18/
├── AGENT_TASK.md
├── README.md
├── docs/
├── meta/
├── target/
│   └── Eclipse/                 # Current implementation target snapshot
└── reference/
    ├── astreaos/                # Curated TopBar/runtime reference source
    └── typhon/                  # Relevant compositor audit context
```

## Scope of the immediate task

The immediate implementation milestone is **M8-A — Native TopBar Foundation and Visual Shell Port**.

It must establish the correct per-output Layer Shell lifecycle, native shell ownership, visual TopBar surfaces, popup infrastructure, clock, Astrea menu integration, and stable model contracts for future workspaces/system state. It must not drag Quickshell, Hyprland-only bindings, command-driven status plumbing, or JSON cache polling into Eclipse.

This is intentionally the foundation for later native system services. Full NetworkManager, BlueZ, PipeWire/WirePlumber, StatusNotifierItem, notifications, and Typhon workspace integration are follow-up milestones described in `docs/08_FOLLOW_UP_ROADMAP.md`.
