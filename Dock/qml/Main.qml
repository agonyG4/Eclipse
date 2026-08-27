import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    title: qsTr("Astrea Dock")
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: panel.width
    height: panel.height

    DockPanel {
        id: panel
        anchors.centerIn: parent
        contextMenuController: ContextMenuController
        outputKey: window.screen ? window.screen.name : ""
        outputOriginX: window.screen ? window.screen.geometry.x : 0
        outputOriginY: window.screen ? window.screen.geometry.y : 0
        onReorderRequested: function(desktopFileName, targetPinIndex) {
            DockController.movePinned(desktopFileName, targetPinIndex)
        }
    }
}
