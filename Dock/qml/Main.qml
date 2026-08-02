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
    }
}
