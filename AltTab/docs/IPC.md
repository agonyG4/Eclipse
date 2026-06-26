# IPC Protocol

## Socket name

```
astrea-alt-tab-v1
```

## Commands

Each command is sent as a newline-terminated UTF-8 string:

| Command | Description |
|---------|-------------|
| `next` | Select next window (forward) |
| `previous` | Select previous window (backward) |
| `commit` | Commit current selection |
| `cancel` | Cancel and close |
| `show` | Show switcher (open but don't select) |
| `hide` | Hide switcher |
| `reload-windows` | Refresh window list from source |
| `status` | Request JSON status |

Maximum command size: 4096 bytes.

## Status response

```json
{
  "running": true,
  "visible": false,
  "state": "hidden",
  "windows": 4,
  "selectedIndex": 0,
  "selectedAddress": "0x1234",
  "windowSource": "hyprland",
  "windowSourceConnected": true,
  "layerShell": true,
  "layerShellConfigured": true,
  "layerShellError": "",
  "iconTheme": "WhiteSur-dark",
  "iconFallbackTheme": "hicolor"
}
```

## CLI mapping

```
--daemon              Start resident mode (hidden)
--next                Send next IPC command
--previous            Send previous IPC command
--commit              Send commit IPC command
--cancel              Send cancel IPC command
--show                Send show IPC command
--hide                Send hide IPC command
--reload-windows      Send reload-windows IPC command
--status              Request and print JSON status
```
