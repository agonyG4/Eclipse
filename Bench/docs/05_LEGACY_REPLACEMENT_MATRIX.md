# Legacy Replacement Matrix

The purpose of this matrix is to prevent accidental architecture regression during the port.

| Legacy mechanism | What it currently provides | Native Eclipse direction |
|---|---|---|
| `Quickshell.screens` + `Variants` | one Bar per monitor | `QGuiApplication::screens`, `screenAdded`, `screenRemoved`, `BarSurfaceManager` |
| `PanelWindow` + `WlrLayershell.*` | Layer Shell windows | existing `AstreaLayerShellHelper` + `QQuickWindow` |
| `Quickshell.Hyprland` | workspace model and dispatch | compositor-neutral `WorkspaceModel`, future Typhon workspace protocol |
| `Quickshell.Services.SystemTray` | StatusNotifier items and menus | future native StatusNotifierHost + DBus menu model |
| `Quickshell.Io.Process` | all shell-command integrations | C++ controller/service APIs only |
| `StatusFile.qml` | JSON cache watching | typed native QObject/model state |
| `astrea_statusd.py` | audio/network/Bluetooth cache | future native SystemRuntime services |
| `wpctl` from QML | volume get/set | future AudioService, ideally PipeWire/WirePlumber-native |
| `nmcli` from QML/daemon | Wi-Fi state/actions | future NetworkService via NetworkManager API/DBus |
| `bluetoothctl` + Python helper | power, scan, pair, trust, autoconnect | future BluetoothService via BlueZ API/DBus |
| `ddcutil` from Control Center | display brightness | future BrightnessService; backend selected by hardware capability |
| `player` helper/scripts | media state/actions | future MPRIS/MediaService |
| `hyprctl kill` | force-quit mode | future Typhon/shell-authorized compositor action; capability-gated meanwhile |
| Quickshell lockscreen launch | session lock UI | future compositor session-lock path; do not emulate with a random window |
| `shutdown now` | poweroff | future PowerService/logind path with proper authorization |
| `ASTREA_ROOT` image file URLs | TopBar assets | compiled Qt resources/image provider |
| Python region helper | locale/region formatting | native Qt locale/i18n service |
| QML-owned scan-owner dictionary | Bluetooth scan demand aggregation | preserve concept inside future BluetoothService |

## Hard prohibition for M8-A QML

New Bar QML must not contain any of these integration patterns:

```text
import Quickshell
import Quickshell.Io
import Quickshell.Hyprland
import Quickshell.Services.SystemTray
Process {
FileView {
IpcHandler {
wpctl
nmcli
bluetoothctl
hyprctl
ddcutil
quickshell
```

The only exception is text inside migration documentation/reference files, never production Bar QML.
