import QtQuick
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared
import "components" as Components

Window {
    id: window

    title: "Astrea Spotlight"
    property bool surfaceReady: false
    visible: surfaceReady && SpotlightController.surfaceVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint

    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY
    opacity: 1.0

    BackdropEffectRegions {
        id: backdropRegions
        anchors.fill: parent
        enabled: typeof ThemeController !== "undefined" && ThemeController
                 && ThemeController.shellStyle === 2

        BackdropRegion {
            item: window.loadedPanel
            enabled: window.loadedPanel && window.loadedPanel.visible && window.loadedPanel.opacity > 0.001
            radius: window.loadedPanel && Number.isFinite(window.loadedPanel.radius)
                    && window.loadedPanel.radius >= 0 ? window.loadedPanel.radius : 0
        }
    }

    property bool hadActiveFocus: false
    property bool focusPending: false
    property var loadedPanel: panel.item

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
                if (loadedPanel) loadedPanel.focusSearch()
            }
        } else if (hadActiveFocus && visible && SpotlightController.open) {
            SpotlightController.close()
        }
    }

    Connections {
        target: SpotlightController
        function onFocusRequested() {
            if (window.active) {
                if (window.loadedPanel) window.loadedPanel.focusSearch()
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
                if (window.loadedPanel) window.loadedPanel.focusSearch()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: SpotlightController.open
        onClicked: function(mouse) {
            var inside = mouse.x >= panel.x && mouse.x <= panel.x + panel.width
                && window.loadedPanel && mouse.y >= panel.y && mouse.y <= panel.y + panel.height
            if (!inside) SpotlightController.close()
        }
    }

    Shared.ThemeComponentLoader {
        id: panel
        objectName: "spotlightPanelLoader"
        width: item ? item.width : 600
        height: item ? item.height : 58
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: parent.height * 0.25
        source: typeof ThemeController !== "undefined" && ThemeController
                ? (ThemeController.themeComponentsRevision >= 0
                   ? ThemeController.themedComponent("spotlightPanel") : "") : ""
        initialProperties: ({ controller: SpotlightController })
        validateItem: function(candidate) {
            return candidate.themeApiVersion === 1
                    && candidate.controller === SpotlightController
                    && typeof candidate.focusSearch === "function"
                    && Number.isFinite(candidate.radius) && candidate.radius >= 0
        }
        fallbackComponent: Components.SpotlightPanel {}

        Connections {
            target: panel
            function onItemChanged() {
                if (window.active && window.visible && SpotlightController.open && window.loadedPanel)
                    window.loadedPanel.focusSearch()
            }
        }
    }
}
