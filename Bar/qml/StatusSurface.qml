import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var clockService: null
    property var popupController: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int launcherWidth: 48
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
        width: barGeometry
            ? barGeometry.statusWidth(outputWidth, launcherWidth, Math.round(implicitWidth))
            : 0

        Clock {
            id: clock
            clockService: window.clockService
            interactive: true
            onClicked: popupController && popupController.toggleClock(window.clockAnchorX)
        }
    }
}
