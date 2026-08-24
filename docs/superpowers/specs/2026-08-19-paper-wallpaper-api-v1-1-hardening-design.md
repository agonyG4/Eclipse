# Astrea Paper Wallpaper API v1.1 — Hardening Design

**Date:** 2026-08-19  
**Status:** Design derived from the authoritative v1.1 request and confirmed against the current v1 source  
**Scope:** Eclipse Paper service/control/Settings/native packaging and the Typhon `astreactl` Paper client

## 1. Design summary

Paper v1 remains the single wallpaper state owner. v1.1 makes mutation completion explicit across the service, control socket, Settings, and `astreactl`; adds bounded control-client lifecycle; packages the canonical AstreaOS Sequoia artwork as a native Eclipse resource; and adds event-driven reconciliation for configured external files.

The central contract becomes:

```text
submit operation
    -> validate
    -> persist
    -> publish authoritative snapshot
    -> emit exactly one terminal operation result
    -> send final IPC response
```

`ok: true` for a mutating request means the transition completed successfully. Queue admission is not a successful mutation.

## 2. Evidence and current v1 inventory

| Evidence | Classification | Finding |
| --- | --- | --- |
| `Paper/core/WallpaperService.*` | CONFIRMED | State, persistence, and bounded latest-wins validation exist, but public mutations have no operation identity or terminal result. |
| `Paper/platform/ipc/WallpaperControlServer.*` | CONFIRMED | `set` replies immediately with `accepted: true`; the server removes its endpoint unconditionally during destruction and has no connection cap or idle timeout. |
| `Settings/services/wallpaper/SettingsWallpaperController.*` | CONFIRMED | Uses `waitForConnected`, `waitForBytesWritten`, and a nested `QEventLoop` from GUI-invoked methods; it cannot observe later Paper completion. |
| `src/astreactl/wallpaper.rs` | CONFIRMED | Reads one immediate response, discards `accepted`, and collapses Paper failures to a generic internal control error. |
| `src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg` in the AstreaOS checkout | CONFIRMED | Canonical existing product artwork is available and will be reused, not regenerated. |
| `Paper/qml/WallpaperSurface.qml` | CONFIRMED | Two-slot asynchronous renderer exists and remains the no-blank-frame presentation boundary. |
| Native Eclipse lockscreen | UNPROVEN/OUT OF SCOPE | No native lockscreen wallpaper consumer exists; v1.1 will not invent one. |
| Regulus | UNPROVEN/UNAVAILABLE | No usable production Regulus implementation was found; the existing persistence interface remains the migration seam. |

The graph index was refreshed for Eclipse before this design. Some C++ files have parser-partial ranges; those ranges were read directly before relying on the source. Typhon Paper paths have no recorded coverage gaps. These are best-effort evidence signals, not completeness proofs.

## 3. Operation identity and terminal results

Add a public `WallpaperOperationId` sequence owned by `WallpaperService`, independent from the validation worker token. Each accepted external mutation receives one monotonically increasing ID. The service emits:

```cpp
void wallpaperOperationFinished(WallpaperOperationId id,
                                 const WallpaperOperationResult &result);
```

`WallpaperOperationResult` contains the operation ID, terminal status, typed error code/message, and the final authoritative snapshot. Terminal statuses are `Succeeded`, `Rejected`, `Superseded`, `PersistenceFailed`, and `CancelledByReset`; shutdown invalidation is represented for internal operations but does not leave an external request unresolved.

The service keeps at most one active validation and one replaceable pending validation. When a newer mutation arrives, any active or pending mutation it supersedes receives exactly one `Superseded` result. A stale worker completion is ignored after its operation has already terminated. Initial startup validation and source reconciliation are internal validations and do not fabricate external operation IDs.

The service preserves the prior authoritative snapshot on rejected validation or persistence failure. A successful set persists first, then publishes configured/effective state and emits `Succeeded`. Reset invalidates active/pending set work, clears persistence, publishes the factory state, and completes its own operation.

## 4. Final control response model

`WallpaperControlServer` remains a small line-oriented JSON adapter with the existing textual commands. `get` and `default` remain immediate. `set` waits for the matching `wallpaperOperationFinished` result before writing exactly one terminal response. Reset uses the same result path so persistence failure is observable.

Responses use:

```json
{
  "ok": true,
  "completed": true,
  "requestId": 43,
  "snapshot": {}
}
```

Failures use `ok: false`, `completed: true`, `requestId`, `errorCode`, `message`, and the previous/final authoritative snapshot. Superseded requests are explicit failures with `errorCode: "superseded"`.

Each waiting socket has a safe `QPointer`-tracked operation association, one bounded response timeout, and clean disconnect handling. Client disconnect does not cancel the global Paper mutation. A request is released when it terminates or its observer disconnects.

## 5. Endpoint ownership and bounded clients

`WallpaperControlServer` tracks `m_endpointOwned`. It becomes true only after this instance successfully listens, including stale-socket recovery. Destruction removes the filesystem endpoint only when the flag is true. A failed second server therefore cannot remove the first server’s live socket.

The server accepts a fixed maximum of 16 connected clients. Excess pending connections are closed immediately. Each accepted client gets a single-shot idle timer, restarted only when a bounded complete command is received; an incomplete client is disconnected after the timeout. The existing 4096-byte command bound, `0700` parent directory, and `0600` socket remain unchanged.

## 6. Async Settings client

Settings gets a reusable small Qt Core/Network Paper client inside the existing Settings service target rather than linking renderer code. It uses `QLocalSocket` signals, a bounded response buffer, a single-shot timeout, and request IDs. No GUI-invoked method calls `waitForConnected`, `waitForBytesWritten`, or starts a nested event loop.

`SettingsWallpaperController` exposes `busy`, `pendingAction`, snapshot properties, and error state. It sends a request and returns immediately. Final responses update the authoritative projection. A controller request generation prevents an older reply from overwriting a newer request. Service-unavailable errors are delivered asynchronously. QML disables conflicting actions while busy, but correctness does not depend on that UI guard.

## 7. `astreactl` semantics

Typhon remains a transport-only client. Its Paper adapter waits for the final response, requires `completed: true`, returns success only for `ok: true`, and preserves Paper `errorCode` and `message` in a typed client error. It does not validate images, persist wallpaper state, or track operation state.

The existing human/JSON output conventions remain. A final successful set returns the final ready snapshot; invalid and superseded transactions return non-zero with the Paper error details.

## 8. Canonical default packaging

The inspected AstreaOS checkout contains the canonical product asset:

```text
/home/agony/.local/share/Astrea/src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg
```

The file is an existing 8.7 MB JPEG. It will be copied unchanged into `Eclipse/Paper/assets/default.jpg` and embedded in the `Astrea.Paper` Qt resource module. The normal resolver order becomes:

1. explicit constructor source for tests/hosts;
2. `ASTREA_WALLPAPER_DEFAULT` development override;
3. packaged `qrc:/qt/qml/Astrea/Paper/assets/default.jpg`;
4. installed/legacy compatibility locations where available;
5. embedded emergency SVG.

The stable logical ID remains `astrea://wallpaper/default`; emergency remains `astrea://wallpaper/emergency`. A production-style resolver test unsets development overrides and proves the packaged default resolves without `ASTREA_ROOT`.

## 9. Reactive source reconciliation

Add a Paper-owned `WallpaperSourceWatcher` based on `QFileSystemWatcher` and a single-shot debounce timer. For a configured external file it watches both the canonical file and its parent directory. Parent notifications handle delete/recreate and atomic rename replacement, while file notifications handle ordinary writes. Events are coalesced before requesting a reconciliation validation.

Reconciliation uses the same one-active/one-pending worker bound but is not an external mutation operation. If the configured file disappears or becomes invalid, configured intent remains persisted, effective becomes the factory default, state becomes `Fallback`, and the precise source error is published. If the file is recreated, atomically replaced, or becomes valid again, Paper automatically revalidates and restores effective configured state. A valid same-path replacement advances the wallpaper generation and emits the effective change so the renderer reloads the image even though the path string is unchanged.

The watcher is detached when configured state is cleared/reset and reattached after successful set/startup migration. There is no periodic polling or idle timer.

## 10. Renderer and scope preservation

`WallpaperSurface.qml` keeps its existing two-slot load/swap behavior. The surface receives the service generation as a reload identity so valid atomic replacement can refresh an unchanged path without exposing a blank frame. Global scope remains the only implemented policy; `Output` remains an explicit unsupported v1 scope.

No native lockscreen surface, dynamic/video/slideshow provider, or per-output policy is added.

## 11. Test-first qualification design

The regression suite will first prove the current failures: immediate successful `set`, stale Settings state, blocking client path, endpoint deletion by a failed second server, and missing client bounds. New tests then prove operation completion, exact supersede/cancel semantics, persistence failure preservation, final IPC responses, async Settings state, typed `astreactl` errors, packaged default resolution, source deletion/restoration, atomic replacement, invalid replacement, debounce bounds, and renderer generation reload.

Native qualification will be attempted using the normal Eclipse + Typhon launch path if available. Offscreen tests will be reported separately and will not be presented as live Wayland or factory-visual proof.

## 12. Rejected alternatives

- Returning `accepted: true` immediately for mutations: rejected because it makes `ok` ambiguous and lets clients report false success.
- Moving operation state into Typhon: rejected because Paper remains the authority and Typhon must stay transport-only.
- Blocking Settings until the socket replies: rejected because it freezes the GUI and cannot observe later completion.
- Polling the filesystem: rejected because it adds idle wakeups and does not solve atomic replacement cleanly.
- Watching only the file: rejected because deletion/rename can remove the file watch.
- Making the normal default the emergency SVG: rejected because emergency is a safety net, not product artwork.
- Copying or regenerating a new image: rejected because the canonical AstreaOS Sequoia asset already exists.
- Adding a general IPC framework: rejected because the existing Paper socket is sufficient.
- Implementing a native lockscreen or per-output selection: rejected as unsupported scope in the current Eclipse architecture.
