import QtQuick
import "components"

PopupCard {
    id: root
    objectName: "trayContextMenu"

    ShellBarTheme { id: theme }

    property var trayService: null
    property var popupController: null
    property string contextKey: ""
    property var menuModel: trayService ? trayService.menuModelForItem(contextKey) : null
    property var modelStack: []
    property string emptyText: "No actions exposed"
    implicitWidth: 240
    cardPadding: 8
    contentSpacing: 2

    function resetMenu() {
        modelStack = []
        menuModel = trayService ? trayService.menuModelForItem(contextKey) : null
        if (trayService)
            trayService.openMenu(contextKey)
    }

    Component.onCompleted: resetMenu()

    Connections {
        target: trayService
        function onItemChanged(key) {
            if (key === root.contextKey)
                root.resetMenu()
        }
    }

    Text {
        visible: !root.menuModel || root.menuModel.rowCount() === 0
        text: root.emptyText
        width: parent.width
        height: 36
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: theme.shellTextDim
        font.family: theme.fontFamilyText
        font.pixelSize: theme.fontSizeBody
    }

    MenuItem {
        visible: root.modelStack.length > 0
        text: "Back"
        icon: "‹"
        onClicked: {
            const stack = root.modelStack.slice()
            root.menuModel = stack.pop()
            root.modelStack = stack
        }
    }

    Repeater {
        model: root.menuModel
        delegate: MenuItem {
            required property int nodeId
            required property string label
            required property string iconName
            required property bool separator
            required property bool hasChildren
            property bool itemEnabled: model.enabled

            visible: !separator
            enabled: itemEnabled
            text: label
            icon: iconName
            onClicked: {
                if (hasChildren && trayService) {
                    trayService.openMenu(root.contextKey)
                    const child = root.menuModel.childModel(nodeId)
                    if (child) {
                        root.modelStack = root.modelStack.concat([root.menuModel])
                        root.menuModel = child
                    }
                } else if (root.menuModel) {
                    root.menuModel.activate(nodeId)
                    if (popupController)
                        popupController.close()
                }
            }
        }
    }
}
