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
    property var pendingChildOwner: null
    property int pendingChildNodeId: -1
    property int pendingChildAnchorY: 0
    property string emptyText: "No actions exposed"

    function iconFor(iconSource, toggleType, state, hasChildren) {
        if (hasChildren)
            return "󰅂"
        if (toggleType === "checkmark")
            return state === 1 ? "󰄲" : state === 2 ? "󰡖" : ""
        if (toggleType === "radio")
            return state === 1 ? "󰐕" : ""
        return iconSource || ""
    }

    implicitWidth: 220
    cardPadding: 12
    contentSpacing: 4

    function cascadeXFor(parentX, parentWidth, childWidth, outputWidth) {
        const rightX = parentX + parentWidth - 4
        const leftX = parentX - childWidth + 4
        const maxX = Math.max(0, outputWidth - childWidth)
        if (rightX + childWidth <= outputWidth)
            return rightX
        if (leftX >= 0 && leftX + childWidth <= outputWidth)
            return leftX
        return Math.max(0, Math.min(maxX, rightX))
    }

    function cascadeYFor(parentY, anchorY, childHeight, outputHeight) {
        const maxY = Math.max(0, outputHeight - childHeight)
        return Math.max(0, Math.min(maxY, parentY + anchorY))
    }

    function resolvePendingChild() {
        if (!pendingChildOwner || pendingChildNodeId < 0 || !trayService)
            return
        if (trayService.menuStateForItem(contextKey) === 2)
            return
        const child = pendingChildOwner.childModel(pendingChildNodeId)
        if (!child)
            return
        childModel = child
        childAnchorY = pendingChildAnchorY
        pendingChildOwner = null
        pendingChildNodeId = -1
    }

    Connections {
        target: root.trayService
        function onMenuContentChanged(key) {
            if (key === root.contextKey)
                root.resolvePendingChild()
        }
    }

    Text {
        visible: root.menuModel && root.menuModel.rowCount() === 0
        text: root.emptyText
        width: parent.width
        height: 32
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: theme.shellTextSecondary
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSizeSmall
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

            MenuSeparator {
                visible: row.separator
            }

            MenuItem {
                visible: !row.separator
                enabled: row.itemEnabled
                text: row.label
                icon: root.iconFor(row.iconSource, row.toggleType, row.state, row.hasChildren)
                onClicked: {
                    if (row.hasChildren) {
                        root.childModel = null
                        root.pendingChildOwner = row.menuOwner
                        root.pendingChildNodeId = row.nodeId
                        root.pendingChildAnchorY = row.y
                        if (root.trayService) {
                            root.trayService.aboutToShowMenu(root.contextKey, row.nodeId)
                            root.resolvePendingChild()
                        }
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
            item.x = root.cascadeXFor(root.x, root.width, width, root.presentationParent.width)
            item.y = root.cascadeYFor(root.y, root.childAnchorY, height,
                                      root.presentationParent.height)
        }
        onChildMenuModelChanged: if (item) item.menuModel = childMenuModel
    }
}
