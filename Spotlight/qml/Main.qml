import QtQuick
import QtQuick.Window
import "components" as Components

Window {
    id: window

    title: "Astrea Spotlight"
    visible: SpotlightController.surfaceVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint

    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY
    opacity: 1.0

    property bool hadActiveFocus: false
    property bool focusPending: false

    onVisibleChanged: {
        if (visible) {
            hadActiveFocus = false
        } else {
            focusPending = false
        }
    }

    onActiveChanged: {
        if (active) {
            hadActiveFocus = true
            if (focusPending && visible && SpotlightController.open) {
                focusPending = false
                panel.focusSearch()
            }
        } else if (hadActiveFocus && visible && SpotlightController.open) {
            SpotlightController.close()
        }
    }

    Connections {
        target: SpotlightController
        function onFocusRequested() {
            if (window.active) {
                panel.focusSearch()
            } else {
                window.focusPending = true
            }
        }
        function onLaunchFailed(error) {
            console.warn("Spotlight launch failed:", error)
        }
    }

    Connections {
        target: window
        function onFrameSwapped() {
            if (window.focusPending && window.visible && window.active
                    && SpotlightController.open) {
                window.focusPending = false
                panel.focusSearch()
            }
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
