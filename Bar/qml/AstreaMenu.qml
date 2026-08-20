import QtQuick
import "components"

PopupCard {
    id: root

    property var barController: null
    property var popupController: null
    implicitWidth: 200
    cardPadding: 12
    contentSpacing: 4

    MenuItem {
        icon: "󰍉"
        text: "Search"
        enabled: root.barController && root.barController.searchAvailable
        onClicked: {
            root.popupController.close()
            root.barController.showSearch()
        }
    }

    MenuSeparator {}

    MenuItem {
        icon: "󰋖"
        text: "About this PC"
        enabled: root.barController && root.barController.aboutAvailable
    }

    MenuItem {
        icon: "󰍜"
        text: "Settings"
        enabled: root.barController && root.barController.settingsAvailable
        onClicked: {
            root.popupController.close()
            root.barController.launchSettings()
        }
    }

    MenuSeparator {}

    MenuItem { icon: "󰅙"; text: "Force Quit"; enabled: root.barController && root.barController.forceQuitAvailable }
    MenuItem { icon: "󰷛"; text: "Lockscreen"; enabled: root.barController && root.barController.lockscreenAvailable }
    MenuItem { icon: "󰐥"; text: "Power"; enabled: root.barController && root.barController.powerAvailable }
}
