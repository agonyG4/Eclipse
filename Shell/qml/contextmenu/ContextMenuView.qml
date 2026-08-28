import QtQuick

Item {
    id: root

    ShellMenuTheme { id: theme }

    property var menuModel: null
    property var contextMenuController: null
    property int presentationGeneration: 0
    property int outputWidth: 1
    property int outputHeight: 1
    property bool rootView: true
    property var parentMenuView: null
    property int activeIndex: -1
    property int submenuIndex: -1
    property int rowHeight: theme.contextMenuNormalRowHeight
    property rect activeRowRectangle: Qt.rect(root.x, root.y, root.width, root.rowHeight)
    readonly property var modelRevision: menuModel ? menuModel.presentationRevision : 0
    readonly property int exactContentHeight: menuModel && modelRevision >= 0
        ? menuModel.presentationContentHeight(theme.contextMenuNormalRowHeight,
                                              theme.contextMenuSeparatorHeight) : 0
    readonly property int desiredHeight: exactContentHeight
        + theme.contextMenuCardPadding * 2
    readonly property int naturalWidth: menuModel && modelRevision >= 0
        ? menuModel.presentationNaturalWidth(
              theme.fontFamily,
              theme.fontSizeBody,
              theme.fontSizeSmall,
              theme.contextMenuRowHorizontalMargin,
              theme.contextMenuIconSlotWidth,
              theme.contextMenuRowSpacing,
              theme.contextMenuCardPadding,
              theme.contextMenuBorderWidth) : 0
    readonly property int edgeMargin: theme.contextMenuEdgeMargin
    readonly property int resolvedWidth: Math.min(
        theme.contextMenuMaximumWidth,
        Math.max(naturalWidth, theme.contextMenuMinimumWidth),
        Math.max(1, outputWidth - edgeMargin * 2))
    readonly property int resolvedHeight: Math.min(
        desiredHeight,
        Math.max(1, outputHeight - edgeMargin * 2))
    readonly property bool scrollable: desiredHeight > resolvedHeight
    readonly property real cardWidth: card.width
    readonly property real cardHeight: card.height
    readonly property real listContentWidth: list.contentWidth >= 0
        ? list.contentWidth : list.width
    readonly property real listContentHeight: list.contentHeight
    readonly property int modelRowCount: list.count
    property var submenuModel: submenuIndex >= 0 && menuModel
        ? menuModel.childModelAt(submenuIndex) : null
    implicitWidth: resolvedWidth
    implicitHeight: desiredHeight
    width: resolvedWidth
    height: resolvedHeight

    function initializeSelection() {
        if (!menuModel)
            return
        const first = menuModel.firstNavigable()
        selectRow(first)
    }

    function selectRow(row) {
        activeIndex = row
        list.currentIndex = row
        if (row >= 0) {
            list.positionViewAtIndex(row, ListView.Contain)
            updateActiveRowRectangle()
            activeRowUpdateTimer.restart()
        }
    }

    Timer {
        id: activeRowUpdateTimer
        interval: 0
        repeat: false
        onTriggered: root.updateActiveRowRectangle()
    }

    function updateActiveRowRectangle() {
        if (!list.currentItem) {
            activeRowRectangle = Qt.rect(root.x, root.y, root.width, root.rowHeight)
            return
        }
        const listOrigin = list.mapToItem(root.parent, 0, 0)
        const point = list.currentItem.mapToItem(list, 0, 0)
        activeRowRectangle = Qt.rect(listOrigin.x + point.x, listOrigin.y + point.y,
                                     list.currentItem.width, list.currentItem.height)
    }

    function moveSelection(delta) {
        if (!menuModel)
            return
        const next = menuModel.nextNavigable(activeIndex, delta)
        if (next >= 0)
            selectRow(next)
    }

    function activateSelection() {
        if (!menuModel || activeIndex < 0 || !list.currentItem)
            return
        if (list.currentItem.hasChildren) {
            openSubmenu(activeIndex)
            return
        }
        const token = list.currentItem.token
        if (contextMenuController)
            contextMenuController.activate(presentationGeneration, token)
    }

    function openSubmenu(row) {
        if (!menuModel)
            return
        const child = menuModel.childModelAt(row)
        if (!child)
            return
        activeIndex = row
        submenuIndex = row
    }

    function closeSubmenu() {
        submenuIndex = -1
    }

    function restoreFocus() {
        forceActiveFocus()
        if (activeIndex >= 0)
            selectRow(activeIndex)
    }

    Component.onCompleted: initializeSelection()
    onMenuModelChanged: initializeSelection()

    focus: visible

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Up) {
            moveSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            moveSelection(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            if (menuModel) {
                selectRow(menuModel.firstNavigable())
            }
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            if (menuModel) {
                selectRow(menuModel.nextNavigable(-1, -1))
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Space) {
            activateSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            if (activeIndex >= 0)
                openSubmenu(activeIndex)
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (!rootView) {
                if (parentMenuView) {
                    const parent = parentMenuView
                    parent.closeSubmenu()
                    parent.restoreFocus()
                } else {
                    closeSubmenu()
                }
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && contextMenuController) {
            contextMenuController.close()
            event.accepted = true
        }
    }

    ContextMenuCard {
        id: card
        anchors.fill: parent
        implicitWidth: root.resolvedWidth
        implicitHeight: root.desiredHeight
        width: root.width
        height: root.height

        ListView {
            id: list
            objectName: "contextMenuList"
            anchors.fill: parent
            anchors.margins: card.cardPadding
            model: root.menuModel
            interactive: root.scrollable
            clip: true
            delegate: Item {
                id: row
                required property int index
                required property int nodeKind
                required property string token
                required property string label
                required property string icon
                required property bool nodeEnabled
                required property bool nodeVisible
                required property string shortcut
                required property int checkState
                required property int checkType
                required property bool destructive
                required property bool hasChildren

                width: list.width
                height: !nodeVisible ? 0 : nodeKind === 1
                    ? theme.contextMenuSeparatorHeight : root.rowHeight
                visible: nodeVisible

                ContextMenuSeparator {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: row.nodeKind === 1
                }

                ContextMenuItem {
                    id: menuItem
                    anchors.fill: parent
                    visible: row.nodeKind !== 1
                    token: row.token
                    label: row.label
                    icon: row.icon === "" ? "" : row.icon.indexOf("://") >= 0
                        ? row.icon : "image://astrea-icon/" + row.icon
                    shortcut: row.shortcut
                    nodeEnabled: row.nodeEnabled
                    selected: root.activeIndex === row.index || menuItem.mouseHovered
                    hasChildren: row.hasChildren
                    checkType: row.checkType
                    checkState: row.checkState
                    destructive: row.destructive
                    onHovered: root.selectRow(row.index)
                    onTriggered: {
                        root.selectRow(row.index)
                        root.activateSelection()
                    }

                }
            }
        }
    }

    Loader {
        id: submenuLoader
        parent: root.parent
        active: root.submenuModel !== null
        source: active ? Qt.resolvedUrl("ContextMenuView.qml") : ""
        property var childMenuModel: root.submenuModel
        z: 2
        width: item ? item.resolvedWidth : 0
        height: item ? item.resolvedHeight : 0
        x: root.contextMenuController && root.submenuModel
           ? root.contextMenuController.submenuPosition(root.outputWidth, root.outputHeight,
                                                        width, height,
                                                        root.activeRowRectangle,
                                                        LayoutMirroring.enabled,
                                                        root.edgeMargin).x : 0
        y: root.contextMenuController && root.submenuModel
           ? root.contextMenuController.submenuPosition(root.outputWidth, root.outputHeight,
                                                        width, height,
                                                        root.activeRowRectangle,
                                                        LayoutMirroring.enabled,
                                                        root.edgeMargin).y : 0
        onLoaded: {
            item.menuModel = childMenuModel
            item.contextMenuController = root.contextMenuController
            item.presentationGeneration = root.presentationGeneration
            item.outputWidth = root.outputWidth
            item.outputHeight = root.outputHeight
            item.rootView = false
            item.parentMenuView = root
            item.forceActiveFocus()
        }
        onChildMenuModelChanged: if (item) item.menuModel = childMenuModel
    }
}
