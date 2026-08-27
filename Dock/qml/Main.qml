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
        dockSurfaceGeometry: DockSurfaceGeometry
        outputKey: window.screen ? window.screen.name : ""
        outputWidth: window.screen ? window.screen.geometry.width : width
        outputHeight: window.screen ? window.screen.geometry.height : height
        onReorderRequested: function(desktopFileName, targetPinIndex) {
            DockController.movePinned(desktopFileName, targetPinIndex)
        }
    }
}
