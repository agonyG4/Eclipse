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
    property var cascadeModel: null
    property int cascadeAnchorY: 0
    property int outputWidth: 1
    property string emptyText: "No actions exposed"
    implicitWidth: 220
    cardPadding: 12
    contentSpacing: 4

    function resetMenu() {
        cascadeModel = null
        cascadeAnchorY = 0
        menuModel = trayService ? trayService.menuModelForItem(contextKey) : null
        if (trayService)
            trayService.openMenu(contextKey)
    }

    function openChild(ownerModel, nodeId, y) {
        if (!ownerModel)
            return
        const child = ownerModel.childModel(nodeId)
        if (!child)
            return
        cascadeModel = child
        cascadeAnchorY = y
        if (trayService)
            trayService.aboutToShowMenu(contextKey, nodeId)
    }

    function closeCascades() {
        cascadeModel = null
    }

    function iconFor(iconSource, iconName, toggleType, state, hasChildren) {
        if (hasChildren)
            return "›"
        if (toggleType === "checkmark")
            return state === 1 ? "󰄲" : state === 2 ? "󰡖" : ""
        if (toggleType === "radio")
            return state === 1 ? "󰐕" : ""
        return iconSource || iconName || ""
    }

    Component.onCompleted: resetMenu()

    Connections {
        target: trayService
        function onItemChanged(key) {
            if (key === root.contextKey)
                root.resetMenu()
        }
    }

    Item {
        id: header
        objectName: "trayMenuHeader"
        width: parent.width
        height: 28

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "•"
            color: theme.shellIconMain
            font.pixelSize: theme.fontSizeIcon
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.trayService
                ? root.trayService.tooltipTitleForItem(root.contextKey) : "System tray"
            color: theme.shellTextSecondary
            elide: Text.ElideRight
            font.family: theme.fontFamilyText
            font.pixelSize: theme.fontSizeCaption
        }
    }

    MenuSeparator { objectName: "trayMenuHeaderSeparator" }

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

    Component {
        id: menuRow

        Item {
            id: row
            property var menuOwner: null
            required property int nodeId
            required property string label
            required property string iconName
            required property string iconSource
            required property string toggleType
            required property int state
            required property bool separator
            required property bool hasChildren
            property bool itemEnabled: model.enabled
            property bool itemVisible: model.visible

            width: parent ? parent.width : root.width
            height: !itemVisible ? 0 : separator ? 1 : 36
            visible: itemVisible

            Rectangle {
                anchors.fill: parent
                visible: row.separator
                color: theme.shellSeparator
            }

            MenuItem {
                visible: !row.separator
                enabled: row.itemEnabled
                text: row.label
                icon: root.iconFor(row.iconSource, row.iconName, row.toggleType,
                                   row.state, row.hasChildren)
                onClicked: {
                    if (row.hasChildren) {
                        root.openChild(row.menuOwner, row.nodeId, row.y)
                    } else if (row.menuOwner) {
                        row.menuOwner.activate(row.nodeId)
                        if (root.popupController)
                            root.popupController.close()
                    }
                }
            }
        }
    }

    Repeater {
        model: root.menuModel
        delegate: menuRow
        onItemAdded: function(index, item) { item.menuOwner = root.menuModel }
    }

    Loader {
        id: cascadeLoader
        parent: root
        active: root.cascadeModel !== null
        sourceComponent: cascadeCard
        property var cascadeMenuModel: root.cascadeModel
        x: root.parent && root.x + root.width + width <= root.parent.width
            ? root.width - 4 : -width + 4
        y: Math.max(0, Math.min(root.parent ? root.parent.height - height : root.cascadeAnchorY,
                                root.cascadeAnchorY))
    }

    Component {
        id: cascadeCard

        PopupCard {
            objectName: "trayCascadeMenu"
            width: 220
            cardPadding: 12
            contentSpacing: 4
            property var cascadeMenuModel: null

            Repeater {
                model: parent.cascadeMenuModel
                delegate: menuRow
                onItemAdded: function(index, item) {
                    item.menuOwner = parent.cascadeMenuModel
                }
            }
        }
    }
}
