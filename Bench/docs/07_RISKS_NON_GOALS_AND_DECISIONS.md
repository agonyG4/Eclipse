# Risks, Non-Goals, and Decisions

## Locked decisions

### Preserve the three-region TopBar behavior

The native port preserves a dedicated reserve surface plus separate launcher/status visuals. Do not replace it with one full-width visual panel simply because that is easier.

### Simplify status positioning

Use top+right Layer Shell anchoring with a 6 px right margin instead of dynamically mutating a left margin. The reference math resolves to the same final position after its width cap.

### Simplify popup ownership

Use one fullscreen Overlay popup surface per output rather than the reference's separate click-shield and small popup Layer Shell surfaces.

### No fake workspace protocol

Define the UI model now; wait for the real Typhon workspace contract before production binding.

### No legacy system bridges in the new QML

Do not reproduce Quickshell Process/FileView patterns in Eclipse.

## Primary risks

### 1. Reserve surface input capture

A transparent full-width QQuickWindow can still intercept input. This is a release blocker. Verify `Qt::WindowTransparentForInput` on the actual LayerShellQt/Wayland stack or implement an explicit empty input region.

### 2. QScreen removal lifetime

Qt screen removal can expose stale pointer bugs if window/bundle ownership is informal. Make output teardown explicit and guarded.

### 3. Layer Shell configured too late

The existing Eclipse shell intentionally configures Layer Shell before mapping. The Bar must follow the same discipline. Do not briefly show an ordinary Qt window.

### 4. Fullscreen popup overlay intercepts all input by design

It must exist only while a popup is active. A hidden but still mapped transparent overlay would block the desktop.

### 5. Theme duplication

Do not create another unrelated hard-coded shell palette. Establish reusable shell tokens that future Control Center/OSD/Island work can share.

### 6. Scope creep into system services

Attempting NetworkManager, BlueZ, PipeWire, StatusNotifier, notifications, workspaces, and Control Center in the same M8-A closure would weaken the core surface migration. Keep interfaces ready, but finish output/surface correctness first.

## Explicit M8-A non-goals

Do not implement or claim completion of:

- native NetworkManager integration;
- native BlueZ integration;
- native PipeWire/WirePlumber integration;
- StatusNotifierItem host;
- DBus menu host;
- full Control Center;
- notification daemon/history migration;
- volume OSD migration;
- Typhon workspace protocol;
- Force Quit compositor protocol;
- session-lock implementation;
- brightness/night-shift backend;
- MPRIS/media service;
- power-management backend.

## Acceptable temporary product behavior

During M8-A:

- system indicators whose service does not exist should be hidden;
- workspace strip may be empty in production;
- unsupported Astrea menu actions may be disabled/hidden;
- Search and native Clock should work;
- Settings should work when the existing catalog/launcher can resolve the application entry.

This is preferable to fake data or legacy command fallbacks because the architecture remains correct for M8-B+.
