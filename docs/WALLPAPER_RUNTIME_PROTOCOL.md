# Wallpaper Runtime Protocol Qualification

Paper wallpaper control actions are served by the `WallpaperControlServer` hosted
inside the running `astrea-shell` process. `astrea-settings` is a client of the
Shell-owned socket; rebuilding Settings alone does not update the live Paper
endpoint.

## Safe action qualification

Send a well-formed removal request for an intentionally unknown managed ID:

```bash
printf 'wallpaper remove {"id":"astrea://wallpaper/user/0000000000000000000000000000000000000000000000000000000000000000"}\n' \
  | socat - UNIX-CONNECT:"$XDG_RUNTIME_DIR/astrea-shell/wallpaper.sock"
```

On a Shell with wallpaper removal support, the expected response is a normal
known-action error such as `wallpaper-not-found`. If the response contains
`Unknown Paper wallpaper action`, the active Shell server is older than the
Settings client.

After installing an updated Shell binary, restart the user service so the live
endpoint is replaced:

```bash
systemctl --user restart astrea-shell.service
```

Restarting Settings alone does not update the Paper endpoint. To record which
binary owns the running service, find its PID and inspect:

```bash
pid="$(systemctl --user show --property MainPID --value astrea-shell.service)"
readlink -f "/proc/$pid/exe"
```
