# Typhon AltTab Backend

The `typhon` backend maps shared Astrea Toplevel snapshots into the existing
AltTab `WindowInfo` model. It advertises `WindowList`, `EventStream`, and
`ActiveWindow`. Window activation is explicitly unsupported in M6 and returns
a deterministic failure without sending a compositor request.

## Mapping

The decimal Typhon `WindowId` string is the `WindowInfo::windowId` value and is
the stable identity key. PID is metadata only. App ID maps to `appId` and
`className`; title maps to both title fields. Minimized windows map to both
`isMinimized` and `isHidden`. Workspace and output fields remain empty.

Focus serial order is converted to dense `focusHistoryId` values: the most
recent focused window is zero, later focused windows increase, and never-focused
windows receive a large stable tail. The backend connection generation is
carried into identity resolution so asynchronous results from an old
generation cannot update a remapped window.

Identity resolution is performed only after atomic snapshot publication. Typhon
inputs use the window ID as address, PID as metadata, app ID as class, the
title, the connection generation, and an app-ID-plus-title fingerprint. The
existing asynchronous `AppIdentityResolver` remains the only deep resolver.

## Selection

Explicit `typhon`, `hyprland`, and `auto` backend requests are supported. Auto
selection reserves Typhon as the first candidate when the finalized protocol
adapter is compiled and available, then falls back to Hyprland when its
environment signature is present. A Typhon backend may report `Unsupported`
asynchronously when the private global is absent.

The existing Hyprland backend and its mutable activation behavior are unchanged.

## Qualification

The backend has deterministic model and fake-adapter coverage. No real Typhon
session qualification has been performed. M7 will add mutable actions only
after the protocol permits them.
