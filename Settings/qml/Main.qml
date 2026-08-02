import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    id: window

    title: qsTr("Astrea Settings")
    visible: true
    width: 1050
    height: 650
    minimumWidth: 800
    minimumHeight: 500
    color: "#10243F"
    flags: Qt.Window | Qt.FramelessWindowHint
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeNormal

    background: Rectangle {
        color: "#10243F"
    }

    AppShell {
        anchors.fill: parent
        controller: SettingsController
        sidebarCollapsed: false
        windowMaximized: window.visibility === Window.Maximized
        onMoveWindowRequested: window.startSystemMove()
        onMinimizeRequested: window.showMinimized()
        onMaximizeRestoreRequested: {
            if (window.visibility === Window.Maximized)
                window.showNormal()
            else
                window.showMaximized()
        }
        onCloseRequested: window.close()
    }
}
