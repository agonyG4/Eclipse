import QtQuick
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared
import "components"

Window {
    id: window

    title: "Astrea Alt+Tab"
    property bool surfaceReady: false
    visible: surfaceReady && AltTabController.surfaceVisible
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
            item: window.loadedPanel && window.loadedPanel.backdropItem
                  ? window.loadedPanel.backdropItem : null
            enabled: window.loadedPanel && window.loadedPanel.visible
                     && window.loadedPanel.opacity > 0.001
            radius: window.loadedPanel && window.loadedPanel.backdropItem
                    && Number.isFinite(window.loadedPanel.backdropItem.radius)
                    && window.loadedPanel.backdropItem.radius >= 0
                    ? window.loadedPanel.backdropItem.radius : 0
        }
    }

    property bool hadActiveFocus: false
    property bool focusPending: false
    property var loadedPanel: altTabPanel.item

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
            if (focusPending && visible && AltTabController.open) {
                focusPending = false
                if (loadedPanel) loadedPanel.forceActiveFocus()
            }
        } else if (hadActiveFocus && visible && AltTabController.open) {
            AltTabController.cancel()
        }
    }

    Connections {
        target: AltTabController
        function onFocusRequested() {
            if (window.active) {
                return
            }
            window.focusPending = true
        }
    }

    Connections {
        target: window
        function onFrameSwapped() {
            if (window.focusPending && window.visible && window.active
                    && AltTabController.open) {
                window.focusPending = false
                if (window.loadedPanel) window.loadedPanel.forceActiveFocus()
            }
        }
    }

    MouseArea {
        objectName: "altTabOutsideClickArea"
        anchors.fill: parent
        enabled: AltTabController.open
        onClicked: function(mouse) {
            var panel = altTabPanel
            var inside = mouse.x >= panel.x && mouse.x <= panel.x + panel.width
                && mouse.y >= panel.y && mouse.y <= panel.y + panel.height
            if (!inside) AltTabController.cancel()
        }
    }

    Shared.ThemeComponentLoader {
        id: altTabPanel
        objectName: "altTabPanelLoader"
        width: item ? item.width : 220
        height: item ? item.height : 116
        anchors.centerIn: parent
        source: typeof ThemeController !== "undefined" && ThemeController
                ? (ThemeController.themeComponentsRevision >= 0
                   ? ThemeController.themedComponent("altTabPanel") : "") : ""
        initialProperties: ({ controller: AltTabController, windowModel: AltTabWindowModel,
                              availableWidth: window.width })
        validateItem: function(candidate) {
            return candidate.themeApiVersion === 1
                    && candidate.controller === AltTabController
                    && candidate.windowModel === AltTabWindowModel
                    && candidate.backdropItem instanceof Item
        }
        fallbackComponent: AltTabPanel {}

        Connections {
            target: altTabPanel
            function onItemChanged() {
                if (window.active && window.visible && AltTabController.open && window.loadedPanel)
                    window.loadedPanel.forceActiveFocus()
            }
        }
    }
}
