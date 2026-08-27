import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    ShellBarTheme { id: theme }

    property var clockService: null
    property var audioService: null
    property var networkService: null
    property var bluetoothService: null
    property var statusNotifierService: null
    property var contextMenuController: null
    property string outputKey: ""
    property var trayTooltipSurface: null
    property var popupController: null
    property var barGeometry: null
    property var workspaceModel: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
    property int launcherWidth: 48
    property int statusLeft: barGeometry
        ? barGeometry.statusLeft(outputWidth, statusPill.width) : 0
    property real clockIndicatorLocalX: statusPill.mapFromItem(clock, clock.width / 2, 0).x
    property int clockAnchorX: barGeometry
        ? barGeometry.statusAnchorX(outputWidth, statusPill.width,
            Math.round(clockIndicatorLocalX))
        : 0
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: statusPill.width
    height: barGeometry ? barGeometry.pillHeight : 36

    BarSegment {
        id: statusPill
        objectName: "statusPill"
        interactive: false
        horizontalPadding: barGeometry ? barGeometry.sidePadding : 10
        spacing: 0
        clip: true
        fixedWidth: barGeometry
            ? barGeometry.statusWidth(outputWidth, launcherWidth, Math.round(implicitWidth))
            : 0

        Tray {
            id: tray
            objectName: "tray"
            trayService: window.statusNotifierService
            popupController: window.popupController
            contextMenuController: window.contextMenuController
            outputKey: window.outputKey
            tooltipSurface: window.trayTooltipSurface
            barGeometry: window.barGeometry
            outputWidth: window.outputWidth
            statusLeft: window.statusLeft
            outputOriginX: window.outputOriginX
            outputOriginY: window.outputOriginY
            statusTop: window.barGeometry ? window.barGeometry.topMargin : 5
        }

        Item {
            objectName: "trayNetworkSpacer"
            width: tray.itemCount > 0 ? theme.spacingTiny : 0
            height: 1
        }

        NetworkIndicator {
            networkService: window.networkService
            popupController: window.popupController
            popupKind: 3
            anchorItem: statusPill
            anchorOffset: window.statusLeft
        }

        BluetoothIndicator {
            bluetoothService: window.bluetoothService
            popupController: window.popupController
            popupKind: 4
            anchorItem: statusPill
            anchorOffset: window.statusLeft
        }

        VolumeIndicator {
            audioService: window.audioService
            popupController: window.popupController
            popupKind: 5
            anchorItem: statusPill
            anchorOffset: window.statusLeft
        }

        Clock {
            id: clock
            clockService: window.clockService
        }
    }
}
