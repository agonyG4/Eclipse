# Zen and Brave download bridge

Implement the previously proposed extension -> Native Messaging -> local API
integration, now explicitly requested by the user. No graphical dock changes.

- Ship a Firefox-compatible Zen background extension and a Brave Manifest V3
  service worker sharing the same adapter. Permissions: downloads, storage and
  nativeMessaging only; no content scripts or website access.
- Observe creation, state/name changes and erasure; poll bytes every second while
  downloads are active because `downloads.onChanged` excludes received-byte updates.
- Map browser states to the existing API. Unknown totals become null, paused
  downloads remain paused, USER_CANCELED becomes cancelled, and completion is explicit.
  Skip private downloads and records whose target filename is not yet available.
- Give each browser profile a persistent random source ID. Keep at most 256 tracked
  IDs, preferentially retaining active downloads. Start with current downloads,
  not the complete download history. Replay tracked IDs after reconnection/reload.
- A Python standard-library native host translates length-prefixed JSON to the
  existing local JSON Lines API. Bound frames, reject unsupported operations, and
  keep stdout exclusively for native protocol frames.
- Start the sibling `astrea-downloads serve` automatically when the API is absent.
  Use a detached process so the API survives the browser host disconnecting.
- Prepare extension bundles and the host in the stable Aether Eclipse build tree.
  A user-level installer registers host manifests for native Zen and Brave; it
  does not silently enable unsigned extensions or change browser security settings.
- Brave's unpacked extension has a stable public key/ID; Zen uses a fixed Gecko ID.
  Local Zen loading is temporary until the add-on is signed for permanent install.
- Validate mapping/polling/replay with Node tests and framing/forwarding/automatic
  startup against the real API with subprocess integration tests. Keep all test
  profiles, caches, build artifacts and generated bundles on Aether.

Activation in the user's browsers still requires loading the prepared extensions
through their extension managers. This is an explicit remaining setup step, not
an automatic side effect of compiling the bridge.
