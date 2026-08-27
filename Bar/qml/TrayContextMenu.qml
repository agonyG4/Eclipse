import QtQuick
import "components"

PopupCard {
    id: root
    objectName: "trayContextMenu"

    ShellBarTheme { id: theme }

    property var trayService: null
    property var popupController: null
    property var contextMenuController: null
    property var contextMenuGeneration: 0
    property string contextKey: ""
    readonly property var serviceRevision: trayService ? trayService.presentationRevision : 0
    readonly property var menuModel: {
        root.serviceRevision
        return root.trayService ? root.trayService.menuModelForItem(root.contextKey) : null
    }
    readonly property int remoteMenuState: {
        root.serviceRevision
        return root.trayService
            ? root.trayService.menuStateForItem(root.contextKey) : 0
    }
    property var cascadeModel: null
    property int cascadeAnchorY: 0
    property var pendingCascadeOwner: null
    property int pendingCascadeNodeId: -1
    property int pendingCascadeAnchorY: 0
    property string presentedContextKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int activeIndex: -1
    property string emptyText: "No actions exposed"
    readonly property bool hasRemoteMenu: {
        root.serviceRevision
        return root.trayService ? root.trayService.hasMenuForItem(root.contextKey) : false
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

    function selectRow(index) {
        if (index >= 0 && isNavigable(index))
            activeIndex = index
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
        if (row.hasChildren)
            openChild(row.menuOwner, row.nodeId, row.y)
        else {
            activateNode(row.menuOwner, row.nodeId)
            if (root.contextMenuController || root.popupController)
                (root.contextMenuController || root.popupController).close()
        }
    }

    function resetMenuState() {
        cascadeModel = null
        cascadeAnchorY = 0
        pendingCascadeOwner = null
        pendingCascadeNodeId = -1
        pendingCascadeAnchorY = 0
        activeIndex = -1
    }

    function requestRootPresentation() {
        if (!trayService || contextKey === "" || presentedContextKey === contextKey)
            return
        presentedContextKey = contextKey
        trayService.prepareMenuForPresentation(contextKey)
    }

    function beginPresentation() {
        resetMenuState()
        requestRootPresentation()
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

    function resolvePendingCascade() {
        if (!pendingCascadeOwner || pendingCascadeNodeId < 0 || !trayService)
            return
        if (root.remoteMenuState === 2)
            return
        const child = pendingCascadeOwner.childModel(pendingCascadeNodeId)
        if (!child)
            return
        cascadeModel = child
        cascadeAnchorY = pendingCascadeAnchorY
        pendingCascadeOwner = null
        pendingCascadeNodeId = -1
    }

    function openChild(ownerModel, nodeId, y) {
        if (!ownerModel || !trayService)
            return
        cascadeModel = null
        pendingCascadeOwner = ownerModel
        pendingCascadeNodeId = nodeId
        pendingCascadeAnchorY = y
        trayService.aboutToShowMenu(contextKey, nodeId)
        resolvePendingCascade()
    }

    function closeCascades() {
        cascadeModel = null
        pendingCascadeOwner = null
        pendingCascadeNodeId = -1
        pendingCascadeAnchorY = 0
    }

    function closeChild() {
        closeCascades()
    }

    function iconFor(iconSource, iconName, toggleType, state, hasChildren) {
        if (hasChildren)
            return "󰅂"
        if (toggleType === "checkmark")
            return state === 1 ? "󰄲" : state === 2 ? "󰡖" : ""
        if (toggleType === "radio")
            return state === 1 ? "󰐕" : ""
        return iconSource || ""
    }

    function activateNode(ownerModel, nodeId) {
        if (root.contextMenuController) {
            root.contextMenuController.activate(root.contextMenuGeneration,
                                                 "tray.node." + nodeId)
        } else if (ownerModel) {
            ownerModel.activate(nodeId)
        }
    }

    Component.onCompleted: {
        resetMenuState()
        if (visible)
            requestRootPresentation()
        Qt.callLater(initializeSelection)
    }
    onContextKeyChanged: {
        presentedContextKey = ""
        if (visible)
            beginPresentation()
        else
            resetMenuState()
    }
    onVisibleChanged: {
        if (visible) {
            presentedContextKey = ""
            beginPresentation()
            forceActiveFocus()
            Qt.callLater(initializeSelection)
        } else {
            presentedContextKey = ""
            closeCascades()
        }
    }

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
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Space || event.key === Qt.Key_Right) {
            activateSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && root.contextMenuController) {
            root.contextMenuController.close()
            event.accepted = true
        }
    }

    Connections {
        target: trayService
        function onMenuClientChanged(key) {
            if (key === root.contextKey) {
                root.presentedContextKey = ""
                if (root.visible)
                    root.beginPresentation()
                else
                    root.resetMenuState()
            }
        }
        function onMenuContentChanged(key) {
            if (key === root.contextKey)
                root.resolvePendingCascade()
        }
    }

    Item {
        id: header
        objectName: "trayMenuHeader"
        width: parent.width
        height: 28

        Image {
            id: headerIcon
            objectName: "trayMenuHeaderIcon"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            sourceSize: Qt.size(18, 18)
            source: {
                root.serviceRevision
                return root.trayService
                    ? root.trayService.iconSourceForItem(root.contextKey) : ""
            }
            fillMode: Image.PreserveAspectFit
            visible: source !== ""
        }

        Text {
            objectName: "trayMenuHeaderTitle"
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: {
                root.serviceRevision
                return root.trayService
                    ? root.trayService.displayTitleForItem(root.contextKey) : "System tray"
            }
            color: theme.shellTextActive
            elide: Text.ElideRight
            font.family: theme.fontFamily
            font.pixelSize: theme.fontSizeBody
            font.weight: Font.DemiBold
        }
    }

    MenuSeparator {
        objectName: "trayMenuHeaderSeparator"
        visible: root.hasRemoteMenu
    }

    Text {
        objectName: "trayMenuNoActions"
        visible: !root.hasRemoteMenu || !root.menuModel || !root.trayService
                 || root.remoteMenuState === 4
                 || root.remoteMenuState === 5
        text: root.emptyText
        width: parent.width
        height: 32
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: theme.shellTextSecondary
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSizeSmall
    }

    Component {
        id: menuRow

        Item {
            id: row
            property var menuOwner: null
            required property int index
            required property int nodeId
            required property string label
            required property string iconName
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
                icon: root.iconFor(row.iconSource, row.iconName, row.toggleType,
                                   row.state, row.hasChildren)
                selected: root.activeIndex === row.index
                onHovered: root.selectRow(row.index)
                onClicked: {
                    root.selectRow(row.index)
                    if (row.hasChildren) {
                        root.openChild(row.menuOwner, row.nodeId, row.y)
                    } else if (row.menuOwner) {
                        root.activateNode(row.menuOwner, row.nodeId)
                        if (root.contextMenuController || root.popupController)
                            (root.contextMenuController || root.popupController).close()
                    }
                }
            }
        }
    }

    Repeater {
        id: menuRepeater
        model: root.menuModel
        delegate: menuRow
        onItemAdded: function(index, item) { item.menuOwner = root.menuModel }
    }

    Loader {
        id: cascadeLoader
        parent: root.parent
        active: root.cascadeModel !== null
        source: root.cascadeModel !== null ? Qt.resolvedUrl("TrayMenuCard.qml") : ""
        property var cascadeMenuModel: root.cascadeModel
        width: Math.min(220, Math.max(1, root.outputWidth))
        height: item ? Math.min(item.implicitHeight, Math.max(1, root.outputHeight)) : 0
        onLoaded: {
            item.menuModel = cascadeMenuModel
            item.trayService = root.trayService
            item.popupController = root.popupController
            item.contextMenuController = root.contextMenuController
            item.contextMenuGeneration = root.contextMenuGeneration
            item.contextKey = root.contextKey
            item.presentationParent = root.parent
            item.outputWidth = root.outputWidth
            item.outputHeight = root.outputHeight
            item.parentMenuCard = root
            item.width = Math.min(220, Math.max(1, root.outputWidth))
            item.depth = 1
            item.x = root.cascadeXFor(root.x, root.width, width, root.parent.width)
            item.y = root.cascadeYFor(root.y, root.cascadeAnchorY, height, root.parent.height)
            Qt.callLater(item.forceActiveFocus)
        }
        onCascadeMenuModelChanged: if (item) item.menuModel = cascadeMenuModel
    }
}
