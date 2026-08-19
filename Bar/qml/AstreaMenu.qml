import QtQuick
import "components"

PopupCard {
    id: root

    property var barController: null
    property var popupController: null

    MenuAction {
        label: "Search"
        enabled: root.barController && root.barController.searchAvailable
        onTriggered: {
            root.popupController.close()
            root.barController.showSearch()
        }
    }

    MenuAction {
        label: "Settings"
        enabled: root.barController && root.barController.settingsAvailable
        onTriggered: {
            root.popupController.close()
            root.barController.launchSettings()
        }
    }

    // These capability-gated actions stay absent until Eclipse has a safe
    // native implementation.  No legacy command fallback is permitted.
    MenuAction {
        label: "About Astrea"
        visible: root.barController && root.barController.aboutAvailable
        enabled: visible
    }
    MenuAction {
        label: "Force Quit"
        visible: root.barController && root.barController.forceQuitAvailable
        enabled: visible
    }
    MenuAction {
        label: "Lockscreen"
        visible: root.barController && root.barController.lockscreenAvailable
        enabled: visible
    }
    MenuAction {
        label: "Power"
        visible: root.barController && root.barController.powerAvailable
        enabled: visible
    }
}
