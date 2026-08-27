import QtQuick
import "components"

PopupCard {
    id: root
    objectName: "trayCascadeMenu"

    ShellBarTheme { id: theme }

    property var menuModel: null
    property var trayService: null
    property var popupController: null
    property var contextMenuController: null
    property var contextMenuGeneration: 0
    property string contextKey: ""
    property Item presentationParent: parent
    property var parentMenuCard: null
    property int depth: 1
    property int maxDepth: 8
    property var childModel: null
    property int childAnchorY: 0
    property var pendingChildOwner: null
    property int pendingChildNodeId: -1
    property int pendingChildAnchorY: 0
    property int outputWidth: 1
    property int outputHeight: 1
    property int activeIndex: -1
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
    height: Math.min(implicitHeight, Math.max(1, outputHeight))
    focus: visible
    cardPadding: 12
    contentSpacing: 4
    clip: true

    function rowAt(index) {
        return menuRepeater.itemAt(index)
    }

    function isNavigable(index) {
        const row = rowAt(index)
        return row && row.itemVisible && row.itemEnabled && !row.separator
    }

    function initializeSelection() {
        activeIndex = -1
        for (let index = 0; index < menuRepeater.count; ++index) {
            if (isNavigable(index)) {
                activeIndex = index
                break
            }
        }
    }

    function moveSelection(delta) {
        const count = menuRepeater.count
        if (count === 0)
            return
        let index = activeIndex < 0 ? (delta > 0 ? -1 : 0) : activeIndex
        for (let step = 0; step < count; ++step) {
            index = (index + (delta > 0 ? 1 : -1) + count) % count
            if (isNavigable(index)) {
                activeIndex = index
                break
            }
        }
    }

    function activateSelection() {
        const row = rowAt(activeIndex)
        if (!row || !isNavigable(activeIndex))
            return
        if (row.hasChildren) {
            childModel = null
            pendingChildOwner = row.menuOwner
            pendingChildNodeId = row.nodeId
            pendingChildAnchorY = row.y
            if (trayService) {
                trayService.aboutToShowMenu(contextKey, row.nodeId)
                resolvePendingChild()
            }
        } else if (row.menuOwner) {
            if (contextMenuController)
                contextMenuController.activate(contextMenuGeneration,
                                                "tray.node." + row.nodeId)
            else
                row.menuOwner.activate(row.nodeId)
            if (contextMenuController || popupController)
                (contextMenuController || popupController).close()
        }
    }

    function closeChild() {
        childModel = null
        pendingChildOwner = null
        pendingChildNodeId = -1
    }

    function restoreParentFocus() {
        if (parentMenuCard) {
            const parent = parentMenuCard
            parent.closeChild()
            parent.forceActiveFocus()
        }
    }

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

    Component.onCompleted: Qt.callLater(initializeSelection)
    onMenuModelChanged: Qt.callLater(initializeSelection)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Up) {
            moveSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            moveSelection(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            initializeSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            for (let index = menuRepeater.count - 1; index >= 0; --index) {
                if (isNavigable(index)) {
                    activeIndex = index
                    break
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            const row = rowAt(activeIndex)
            if (row && isNavigable(activeIndex) && row.hasChildren)
                activateSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Space) {
            activateSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            restoreParentFocus()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && contextMenuController) {
            contextMenuController.close()
            event.accepted = true
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
        id: menuRepeater
        model: root.menuModel
        delegate: Item {
            id: row
            property var menuOwner: root.menuModel
            required property int index
            required property int nodeId
            required property string label
            required property string iconSource
            required property string toggleType
            required property int state
            required property bool separator
            required property bool hasChildren
            required property bool nodeEnabled
            required property bool nodeVisible
            property bool itemEnabled: nodeEnabled
            property bool itemVisible: nodeVisible

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
                    selected: root.activeIndex === row.index
                    onHovered: root.activeIndex = row.index
                    onClicked: {
                        root.activeIndex = row.index
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
                        if (root.contextMenuController)
                            root.contextMenuController.activate(root.contextMenuGeneration,
                                                                 "tray.node." + row.nodeId)
                        else
                            row.menuOwner.activate(row.nodeId)
                        if (root.contextMenuController || root.popupController)
                            (root.contextMenuController || root.popupController).close()
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
        width: item ? Math.min(item.implicitWidth, Math.max(1, root.outputWidth)) : 0
        height: item ? Math.min(item.implicitHeight, Math.max(1, root.outputHeight)) : 0
        onLoaded: {
            item.menuModel = childMenuModel
            item.trayService = root.trayService
            item.popupController = root.popupController
            item.contextMenuController = root.contextMenuController
            item.contextMenuGeneration = root.contextMenuGeneration
            item.contextKey = root.contextKey
            item.presentationParent = root.presentationParent
            item.parentMenuCard = root
            item.outputWidth = root.outputWidth
            item.outputHeight = root.outputHeight
            item.depth = root.depth + 1
            item.x = root.cascadeXFor(root.x, root.width, width, root.presentationParent.width)
            item.y = root.cascadeYFor(root.y, root.childAnchorY, height,
                                      root.presentationParent.height)
            Qt.callLater(item.forceActiveFocus)
        }
        onChildMenuModelChanged: if (item) item.menuModel = childMenuModel
    }
}
