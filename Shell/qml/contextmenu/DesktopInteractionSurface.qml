import QtQuick
import QtQuick.Window

Window {
    id: root

    property var contextMenuController: null
    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    width: outputWidth
    height: outputHeight

    function debugDesktopInput(x, y) {
        if (!root.contextMenuController || !root.contextMenuController.debugEnabled)
            return
        console.log("astrea.context-menu " + JSON.stringify({
            stage: "desktop-input",
            outputKey: root.outputKey,
            outputOriginX: root.outputOriginX,
            outputOriginY: root.outputOriginY,
            windowWidth: root.width,
            windowHeight: root.height,
            mouseX: x,
            mouseY: y,
            forwardedX: x,
            forwardedY: y,
            outputLocal: true
        }))
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: function(mouse) {
            if (mouse.button !== Qt.RightButton || !root.contextMenuController)
                return
            const x = Math.round(mouse.x)
            const y = Math.round(mouse.y)
            root.debugDesktopInput(x, y)
            root.contextMenuController.presentDesktop(x, y, root.outputKey)
            mouse.accepted = true
        }
    }
}
