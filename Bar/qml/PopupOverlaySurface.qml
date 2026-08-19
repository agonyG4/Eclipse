import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var barController: null
    property var clockService: null
    property var popupController: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int sidePadding: barGeometry ? barGeometry.popupSidePadding : 8
    property int topOffset: barGeometry ? barGeometry.popupTop : 54
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: outputWidth
    height: outputHeight

    MouseArea {
        anchors.fill: parent
        enabled: popupController && popupController.surfaceRequired
        z: 0
        onClicked: popupController.close()
    }

    AstreaMenu {
        id: astreaMenu
        objectName: "astreaMenu"
        visible: popupController && popupController.surfaceRequired
                  && popupController.kind === 1
        width: barGeometry ? barGeometry.popupWidth(outputWidth, implicitWidth, sidePadding)
                           : implicitWidth
        x: popupController && barGeometry
            ? barGeometry.popupX(outputWidth, width, popupController.anchorX, sidePadding)
            : sidePadding
        y: topOffset
        z: 2
        barController: window.barController
        popupController: window.popupController
        opacity: 0
        scale: 0.97
        ParallelAnimation {
            id: astreaExit
            NumberAnimation { target: astreaMenu; property: "opacity"; to: 0; duration: 180 }
            NumberAnimation { target: astreaMenu; property: "scale"; to: 0.97; duration: 220; easing.type: Easing.OutCubic }
            onFinished: if (popupController && popupController.closing
                            && popupController.kind === 1)
                popupController.completeClose()
        }
    }

    ClockPopup {
        id: clockPopup
        objectName: "clockPopup"
        visible: popupController && popupController.surfaceRequired
                  && popupController.kind === 2
        width: barGeometry ? barGeometry.popupWidth(outputWidth, implicitWidth, sidePadding)
                           : implicitWidth
        x: popupController && barGeometry
            ? barGeometry.popupX(outputWidth, width, popupController.anchorX, sidePadding)
            : sidePadding
        y: topOffset
        z: 2
        clockService: window.clockService
        opacity: 0
        scale: 0.97
        ParallelAnimation {
            id: clockExit
            NumberAnimation { target: clockPopup; property: "opacity"; to: 0; duration: 180 }
            NumberAnimation { target: clockPopup; property: "scale"; to: 0.97; duration: 220; easing.type: Easing.OutCubic }
            onFinished: if (popupController && popupController.closing
                            && popupController.kind === 2)
                popupController.completeClose()
        }
    }

    function syncPopupPresentation() {
        if (!popupController)
            return
        if (popupController.closing) {
            if (popupController.kind === 1)
                astreaExit.restart()
            else if (popupController.kind === 2)
                clockExit.restart()
            return
        }
        astreaExit.stop()
        clockExit.stop()
        if (popupController.popupOpen) {
            if (popupController.kind === 1) {
                astreaMenu.opacity = 1
                astreaMenu.scale = 1
            } else if (popupController.kind === 2) {
                clockPopup.opacity = 1
                clockPopup.scale = 1
            }
        }
    }

    Connections {
        target: popupController
        function onChanged() { window.syncPopupPresentation() }
    }

    Component.onCompleted: syncPopupPresentation()
}
