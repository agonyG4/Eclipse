import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    title: "Astrea Alt+Tab"
    visible: AltTabController.surfaceVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint

    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY
    opacity: 1.0

    property bool hadActiveFocus: false

    onVisibleChanged: {
        if (visible) {
            hadActiveFocus = false
            requestActivate()
        }
    }

    onActiveChanged: {
        if (active) {
            hadActiveFocus = true
        } else if (hadActiveFocus && visible && AltTabController.open) {
            AltTabController.cancel()
        }
    }

    Connections {
        target: AltTabController
        function onFocusRequested() {
            window.requestActivate()
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: AltTabController.open
        onClicked: function(mouse) {
            var panel = altTabPanel
            var inside = mouse.x >= panel.x && mouse.x <= panel.x + panel.width
                && mouse.y >= panel.y && mouse.y <= panel.y + panel.height
            if (!inside) AltTabController.cancel()
        }
    }

    AltTabPanel {
        id: altTabPanel
        anchors.centerIn: parent
    }
}
