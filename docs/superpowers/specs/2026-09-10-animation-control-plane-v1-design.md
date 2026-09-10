# Eclipse Animation Control Plane v1 Integration

## Purpose

Eclipse provides the user-facing Settings and Dock integration for Typhon’s
Astrea Animation Control Plane. Typhon remains the only configuration and
persistence owner. Eclipse sends bounded native control requests, adopts the
authoritative response, and localizes stable IDs without inventing capability
state.

## Settings transport and controller

Settings uses a native Qt `QLocalSocket` client for the existing `astrea.control`
version 1 newline-delimited JSON protocol. The client enforces the 64 KiB
request and 1 MiB response limits, one complete response line, request-ID
matching, protocol/version checks, and strict success/error shapes. Endpoint
discovery follows Typhon’s secure runtime layout: current `WAYLAND_DISPLAY`
when valid, otherwise exactly one same-user secure instance; ambiguous or
symlinked/world-writable candidates are rejected.

`SettingsAnimationController`, owned by `SettingsController` and exposed as
`SettingsController.animations`, projects authoritative snapshots into QML.
It serializes one mutation at a time, coalesces speed edits, restores the last
confirmed snapshot after a rejected request, and adopts the returned snapshot
after success. It exposes availability, busy state, generation/source,
enabled, preset, speed, override state, slot capability data, and bounded
errors. Planned effects are visible as planned but cannot be manually
selected.

The dedicated `Animations` page lives under Customization beside Appearance,
Wallpaper, and Dock. It uses the existing Form cards, rows, toggle, selector,
and slider components. The slider sends model-driven values in the 0.5x–2.0x
range and flushes on release. The existing Compositor page no longer owns a
local preview-only animation toggle; it links to the real page instead. There
is no decorative preview until Typhon provides a real preview command.

All user-facing strings use the existing English i18n catalog. Stable Typhon
IDs and availability remain transport data; localization is Eclipse’s job.

## Dock anchor publication

The Dock publishes one resting, unscaled icon rectangle per
`desktopFileName`. QML supplies the stable delegate identity and geometry
event; it does not enumerate or expose Typhon `WindowId`s. Dock C++ caches the
structural anchor, projects it to every current `windowIds` entry for that app,
and sends the same compositor-global logical rectangle through the existing
authenticated `TyphonToplevelConnection`.

Coordinate conversion remains centralized in `DockSurfaceGeometry`: Dock or
panel geometry is converted to output-local coordinates and output origin is
then applied. Negative output origins are valid. The calculation uses resting
icon size and structural layout only; hover magnification, lift animation,
pointer movement, and drag scale cannot move the destination. Duplicate
identical publications are coalesced.

Structural changes republish anchors: icon size, Dock edge/placement/margin,
pinned or runtime order, delegate insertion/removal, output assignment or
geometry, and app-to-window association changes. A cached anchor is sent to a
new window immediately. Windows no longer associated with an app lose local
publication bookkeeping. Disconnect clears local publication bookkeeping;
reconnect repopulates it from the cached structural anchors after a fresh
Typhon snapshot.

The shared Typhon protocol copy is kept semantically identical to Typhon’s
source XML and exposes v3 anchor requests only through the already authenticated
private toplevel channel. Eclipse does not consume anchors for Lamp rendering
in this version.

## Verification boundary

Focused tests cover control framing/discovery, controller snapshot and failure
semantics, Settings navigation/page ownership, protocol v2 compatibility and
v3 anchors, resting geometry for all supported Dock edges and output origins,
and exact app-to-window projection. No Lamp deformation, fake preview, or
per-window Dock icon model is introduced.
