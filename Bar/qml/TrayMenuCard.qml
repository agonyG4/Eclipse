import QtQuick
import "components"

PopupCard {
    id: root
    objectName: "trayCascadeMenu"

    ShellBarTheme { id: theme }

    property var menuModel: null
    property var trayService: null
    property var popupController: null
    property string contextKey: ""
    property Item presentationParent: parent
    property int depth: 1
    property int maxDepth: 8
    property var childModel: null
    property int childAnchorY: 0
    property string emptyText: "No actions exposed"

    function iconFor(iconSource, toggleType, state, hasChildren) {
        if (hasChildren)
            return "›"
        if (toggleType === "checkmark")
            return state === 1 ? "󰄲" : state === 2 ? "󰡖" : ""
        if (toggleType === "radio")
            return state === 1 ? "󰐕" : ""
        return iconSource || ""
    }

    implicitWidth: 220
    cardPadding: 12
    contentSpacing: 4

    Text {
        visible: root.menuModel && root.menuModel.rowCount() === 0
        text: root.emptyText
        width: parent.width
        height: 36
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: theme.shellTextDim
        font.family: theme.fontFamilyText
        font.pixelSize: theme.fontSizeBody
    }

    Repeater {
        model: root.menuModel
        delegate: Item {
            id: row
            property var menuOwner: root.menuModel
            required property int nodeId
            required property string label
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
                icon: root.iconFor(row.iconSource, row.toggleType, row.state, row.hasChildren)
                onClicked: {
                    if (row.hasChildren) {
                        root.childModel = row.menuOwner.childModel(row.nodeId)
                        root.childAnchorY = row.y
                        if (root.trayService)
                            root.trayService.prepareMenuForPresentation(root.contextKey,
                                                                        row.nodeId)
                    } else if (row.menuOwner) {
                        row.menuOwner.activate(row.nodeId)
                        if (root.popupController)
                            root.popupController.close()
                    }
                }
            }
        }
    }

    Loader {
        id: childLoader
        parent: root.presentationParent
        active: root.childModel !== null && root.depth < root.maxDepth
        source: active ? Qt.resolvedUrl("TrayMenuCard.qml") : ""
        property var childMenuModel: root.childModel
        onLoaded: {
            item.menuModel = childMenuModel
            item.trayService = root.trayService
            item.popupController = root.popupController
            item.contextKey = root.contextKey
            item.presentationParent = root.presentationParent
            item.depth = root.depth + 1
            item.x = root.x + root.width - 4 <= root.presentationParent.width
                ? root.x + root.width - 4 : root.x - width + 4
            item.y = Math.max(0, Math.min(root.presentationParent.height - height,
                                           root.y + root.childAnchorY))
        }
        onChildMenuModelChanged: if (item) item.menuModel = childMenuModel
    }
}
