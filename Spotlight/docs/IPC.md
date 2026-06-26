# IPC

## Commands

- `show`
- `hide`
- `toggle`
- `query <text>`
- `activate`
- `reload-index`
- `status`

## Responses

### Status JSON

```json
{
  "running": true,
  "visible": true,
  "open": true,
  "results": 3,
  "componentEnabled": true,
  "gameMode": false,
  "layerShell": true,
  "layerShellCompiled": true,
  "layerShellConfigured": true,
  "layerShellError": "",
  "iconTheme": "WhiteSur-dark",
  "iconFallbackTheme": "hicolor"
}
```

## CLI Mapping

- `--daemon` starts resident mode.
- `--show`, `--hide`, `--toggle`, `--query`, `--activate`, `--reload-index`, `--status` are forwarded as IPC commands.
- `--resolve-icon` prints icon diagnostics.
- `--icon-theme` prints theme diagnostics.
