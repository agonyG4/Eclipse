import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var barController: null
    property var clockService: null
    property var audioService: null
    property var networkService: null
    property var bluetoothService: null
    property var popupController: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int sidePadding: barGeometry ? barGeometry.popupSidePadding : 8
    property int topOffset: barGeometry ? barGeometry.popupTop : 54
    readonly property real hiddenScale: 0.97
    readonly property int fadeDuration: 180
    readonly property int scaleDuration: 220
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: outputWidth
    height: outputHeight

    ShellBarTheme { id: theme }

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
    }

    NetworkPopup {
        id: networkPopup
        objectName: "networkPopup"
        visible: popupController && popupController.surfaceRequired
                  && popupController.kind === 3
        width: barGeometry ? barGeometry.popupWidth(outputWidth, implicitWidth, sidePadding)
                           : implicitWidth
        x: popupController && barGeometry
            ? barGeometry.popupX(outputWidth, width, popupController.anchorX, sidePadding)
            : sidePadding
        y: topOffset
        z: 2
        networkService: window.networkService
        opacity: 0
        scale: 0.97
    }

    BluetoothPopup {
        id: bluetoothPopup
        objectName: "bluetoothPopup"
        visible: popupController && popupController.surfaceRequired
                  && popupController.kind === 4
        width: barGeometry ? barGeometry.popupWidth(outputWidth, implicitWidth, sidePadding)
                           : implicitWidth
        x: popupController && barGeometry
            ? barGeometry.popupX(outputWidth, width, popupController.anchorX, sidePadding)
            : sidePadding
        y: topOffset
        z: 2
        bluetoothService: window.bluetoothService
        opacity: 0
        scale: 0.97
    }

    VolumePopup {
        id: volumePopup
        objectName: "volumePopup"
        visible: popupController && popupController.surfaceRequired
                  && popupController.kind === 5
        width: barGeometry ? barGeometry.popupWidth(outputWidth, implicitWidth, sidePadding)
                           : implicitWidth
        x: popupController && barGeometry
            ? barGeometry.popupX(outputWidth, width, popupController.anchorX, sidePadding)
            : sidePadding
        y: topOffset
        z: 2
        audioService: window.audioService
        opacity: 0
        scale: 0.97
    }

    property Item activePopup: null

    function popupForKind(kind) {
        switch (kind) {
        case 1: return astreaMenu
        case 3: return networkPopup
        case 4: return bluetoothPopup
        case 5: return volumePopup
        default: return null
        }
    }

    ParallelAnimation {
        id: popupEnter
        NumberAnimation {
            objectName: "popupEnterFade"
            target: window.activePopup
            property: "opacity"
            from: 0
            to: 1
            duration: window.fadeDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            objectName: "popupEnterScale"
            target: window.activePopup
            property: "scale"
            from: window.hiddenScale
            to: 1
            duration: window.scaleDuration
            easing.type: Easing.OutBack
        }
    }

    ParallelAnimation {
        id: popupExit
        NumberAnimation {
            objectName: "popupExitFade"
            target: window.activePopup
            property: "opacity"
            to: 0
            duration: window.fadeDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            objectName: "popupExitScale"
            target: window.activePopup
            property: "scale"
            to: window.hiddenScale
            duration: window.scaleDuration
            easing.type: Easing.OutCubic
        }
        onFinished: {
            if (popupController && popupController.closing
                    && window.activePopup === window.popupForKind(popupController.kind))
                popupController.completeClose()
        }
    }

    function syncPopupPresentation() {
        if (!popupController)
            return
        popupEnter.stop()
        popupExit.stop()

        if (popupController.closing) {
            activePopup = popupForKind(popupController.kind)
            if (activePopup)
                popupExit.restart()
            return
        }

        if (popupController.popupOpen) {
            activePopup = popupForKind(popupController.kind)
            if (activePopup) {
                activePopup.opacity = 0
                activePopup.scale = window.hiddenScale
                popupEnter.restart()
            }
        } else {
            activePopup = null
        }
    }

    Connections {
        target: popupController
        function onChanged() { window.syncPopupPresentation() }
    }

    Component.onCompleted: syncPopupPresentation()
}
