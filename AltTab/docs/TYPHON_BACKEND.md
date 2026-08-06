# Typhon AltTab Backend

The `typhon` backend maps shared Astrea Toplevel snapshots into the existing
AltTab `WindowInfo` model. It advertises `WindowList`, `EventStream`, and
`ActiveWindow`. Window activation is explicitly unsupported in M6 and returns
a deterministic failure without sending a compositor request.

## Mapping

The decimal Typhon `WindowId` string is the `WindowInfo::windowId` value and is
the stable identity key. PID is metadata only. App ID maps to `appId` and
`className`; title maps to both title fields. Minimized windows map to
`isMinimized` while remaining `isHidden == false`, because a minimized managed
toplevel is still eligible for task switching. Workspace and output fields
remain empty: Typhon v1 does not publish workspace metadata.

An empty Typhon workspace is unknown metadata, not a hidden or invalid window.
AltTab accepts it as eligible and passes workspace `-1` to identity resolution.
Explicit numeric workspace values must be positive; non-positive or malformed
explicit values remain excluded. No workspace value is synthesized for Typhon.

Focus serial order is converted to dense `focusHistoryId` values: the most
recent focused window is zero, later focused windows increase, and never-focused
windows receive a large stable tail. The backend connection generation is
carried into identity resolution so asynchronous results from an old
generation cannot update a remapped window.

Identity resolution is performed only after atomic snapshot publication. Typhon
inputs use the window ID as address, PID as metadata, app ID as class, the
title, the connection generation, and an app-ID-plus-title fingerprint. The
existing asynchronous `AppIdentityResolver` remains the only deep resolver.

## Snapshot delivery

Opening AltTab is request-driven. When a backend reaches `Ready` while AltTab
is opening, the controller issues one snapshot request for that opening
generation; the matching `snapshotReady` response is the only opening boundary.
An unsolicited `snapshotChanged` updates an already-open model but does not
open the UI.

Typhon snapshot requests made before the first committed revision wait in a
bounded queue of 16 tokens. The first committed snapshot is cached, published
through `snapshotChanged`, and then returned to every queued token. Each
accepted token receives exactly one result. A rejected request at capacity
receives one empty result and `BackendError::ConnectionFailed`; pending tokens
are retained. Stop and terminal connection failures resolve all remaining
tokens with empty results and the same error. Requests from an older Typhon
connection generation are failed rather than receiving a newer snapshot.

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
