import QtQuick
import QtQuick.Window
import "components" as Components

Window {
    id: window

    title: "Astrea Spotlight"
    visible: SpotlightController.surfaceVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint

    width: screen ? screen.geometry.width : 1920
    height: screen ? screen.geometry.height : 1080
    x: screen ? screen.geometry.x : 0
    y: screen ? screen.geometry.y : 0
    opacity: 1.0

    property bool hadActiveFocus: false

    onVisibleChanged: {
        if (visible) {
            hadActiveFocus = false
            requestActivate()
            Qt.callLater(panel.focusSearch)
        }
    }

    onActiveChanged: {
        if (active) {
            hadActiveFocus = true
        } else if (hadActiveFocus && visible && SpotlightController.open) {
            SpotlightController.close()
        }
    }

    Connections {
        target: SpotlightController
        function onFocusRequested() {
            window.requestActivate()
            panel.focusSearch()
        }
        function onLaunchFailed(error) {
            console.warn("Spotlight launch failed:", error)
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: SpotlightController.open
        onClicked: function(mouse) {
            var inside = mouse.x >= panel.x && mouse.x <= panel.x + panel.width
                && mouse.y >= panel.y && mouse.y <= panel.y + panel.height
            if (!inside) SpotlightController.close()
        }
    }

    Components.SpotlightPanel {
        id: panel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: parent.height * 0.25
    }
}
