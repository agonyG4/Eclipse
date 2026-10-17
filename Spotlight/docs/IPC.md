# IPC

## Resident process and server

`astrea-shell` is the resident process and hosts `ShellIpcServer`. The
`astrea-spotlight` compatibility executable uses `ShellIpcClient` to send
resident commands through the Shell IPC protocol. `SpotlightIpcServer` is a
legacy/test-only implementation; it is not the current resident server.

## Commands

The compatibility CLI forwards these resident commands to Shell:

- `show`
- `hide`
- `toggle`
- `query <text>`
- `activate`
- `reload-index`
- `status`

The command-line forms are `--show`, `--hide`, `--toggle`, `--query <text>`,
`--activate`, `--reload-index`, and `--status`.

## Responses

`--status` prints the status JSON returned by the resident Shell. If Shell is
not running or does not reply, the compatibility client reports that state.

## CLI behavior

- `--daemon` does not start resident mode. It is intentionally rejected with
  an error directing users to `astrea-shell`.
- Resident commands are delivered with `ShellIpcClient` to `ShellIpcServer`.
- `--resolve-icon` and `--icon-theme` continue to run local icon diagnostics.

The compatibility executable remains independent of
`astrea-spotlight-runtime` and `astrea-spotlight-wayland`.
