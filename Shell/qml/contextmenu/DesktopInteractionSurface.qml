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

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: function(mouse) {
            if (mouse.button !== Qt.RightButton || !root.contextMenuController)
                return
            root.contextMenuController.presentDesktop(Math.round(mouse.x), Math.round(mouse.y),
                                                      root.outputKey)
            mouse.accepted = true
        }
    }
}
