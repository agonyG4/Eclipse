import QtQuick
import "components"

Item {
    id: root

    ShellBarTheme { id: theme }

    property var trayService: null
    property var popupController: null
    property var contextMenuController: null
    property Item surfaceContentItem: null
    property string outputKey: ""
    property var tooltipSurface: null
    property var barGeometry: null
    property var statusLayout: null
    property int outputWidth: 1
    property int statusLeft: 0
    property int outputOriginX: 0
    property int outputOriginY: 0
    property int statusTop: 5
    readonly property int itemCount: trayRepeater.count
    readonly property int firstDelegateHeight: trayRepeater.count > 0
        && trayRepeater.itemAt(0) ? trayRepeater.itemAt(0).height : 0
    implicitWidth: trayRow.implicitWidth
    implicitHeight: 36
    width: implicitWidth
    height: 36

    function anchorFor(delegate, x, y) {
        const surfacePoint = root.surfaceContentItem
            ? delegate.mapToItem(root.surfaceContentItem, x, y)
            : delegate.mapToItem(root, x, y)
        if (root.barGeometry)
            return root.barGeometry.trayAnchor(root.outputOriginX, root.outputOriginY,
                                               root.statusLeft, root.statusTop,
                                               Math.round(surfacePoint.x), Math.round(surfacePoint.y))
        return {localX: root.statusLeft + surfacePoint.x,
                localY: root.statusTop + surfacePoint.y,
                globalX: root.outputOriginX + root.statusLeft + surfacePoint.x,
                globalY: root.outputOriginY + root.statusTop + surfacePoint.y}
    }

    function presentContextMenu(delegate) {
        if (!root.contextMenuController || !delegate.hasMenu || !delegate.hasUsableMenu)
            return false
        const topLeft = root.surfaceContentItem
            ? delegate.mapToItem(root.surfaceContentItem, 0, 0)
            : delegate.mapToItem(root, 0, 0)
        return root.contextMenuController.presentTray(delegate.key,
            Math.round(root.statusLeft + topLeft.x),
            Math.round(root.statusTop + topLeft.y),
            Math.round(delegate.width), Math.round(delegate.height),
            root.barGeometry ? root.barGeometry.popupTop : 54,
            root.outputKey)
    }

    Row {
        id: trayRow
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            id: trayRepeater
            model: root.trayService ? root.trayService.itemModel : null

            delegate: IndicatorButton {
                id: itemDelegate
                objectName: "trayItem"
                fixedWidth: ready ? 28 : 0
                height: 28
                horizontalPadding: 0
                backgroundMargin: 0
                backgroundRadius: theme.shellRadiusMedium
                backgroundObjectName: "trayItemSurface"
                hoverColor: theme.shellSeparator
                pressedColor: Qt.rgba(1, 1, 1, 0.2)
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                active: false
                interactive: ready
                reorderable: ready && !!root.statusLayout
                statusLayout: root.statusLayout
                reorderItem: itemDelegate
                property string resolvedOrderKey: ""
                required property var model
                readonly property string trayId: {
                    const row = typeof model !== "undefined" ? model : null
                    return row && row.id !== undefined ? String(row.id) : ""
                }
                required property string key
                required property string title
                required property string iconSource
                required property bool hasMenu
                required property bool onlyMenu
                required property bool ready
                // Optional: only the real model carries this role.
                readonly property int unreadCount: model.unreadCount !== undefined
                    ? model.unreadCount : 0
                readonly property bool hasUsableMenu: {
                    if (!root.trayService)
                        return false
                    root.trayService.presentationRevision
                    return root.trayService.hasUsableMenuForItem(itemDelegate.key)
                }

                visible: ready

                BarIcon {
                    objectName: "trayItemIcon"
                    source: itemDelegate.iconSource
                    visible: status !== Image.Error && status !== Image.Null

                    Rectangle {
                        objectName: "trayUnreadBadge"
                        visible: itemDelegate.unreadCount > 0
                        // Hangs off the icon's corner, inside the 28 px cell.
                        x: parent.width + 5 - width
                        y: -4
                        width: Math.max(height, trayUnreadLabel.implicitWidth + 6)
                        height: 12
                        radius: height / 2
                        color: "#d83b48"

                        Text {
                            id: trayUnreadLabel
                            objectName: "trayUnreadLabel"
                            anchors.centerIn: parent
                            text: itemDelegate.unreadCount > 99 ? "99+"
                                                                : String(itemDelegate.unreadCount)
                            color: "white"
                            font.family: theme.typography.role("caption").family
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }

                function updateTooltip() {
                    if (itemDelegate.hovered && !root.statusLayout?.dragActive
                            && root.tooltipSurface && root.trayService
                            && root.trayService.tooltipTitleForItem(itemDelegate.key) !== "") {
                        const anchor = root.anchorFor(itemDelegate,
                            itemDelegate.width / 2, itemDelegate.height / 2)
                        root.tooltipSurface.showTooltip(itemDelegate.key, anchor.localX)
                    } else if (root.tooltipSurface) {
                        root.tooltipSurface.hideTooltip()
                    }
                }

                Component.onCompleted: {
                    if (root.statusLayout) {
                        let naturalKey = ""
                        if (root.statusLayout.orderStore && root.statusLayout.orderStore.trayKey) {
                            naturalKey = root.statusLayout.orderStore.trayKey(itemDelegate.trayId,
                                itemDelegate.key)
                        } else {
                            const separator = itemDelegate.key.indexOf("|")
                            const objectPath = separator < 0 ? itemDelegate.key
                                : itemDelegate.key.slice(separator + 1)
                            naturalKey = itemDelegate.trayId
                                ? "tray:" + encodeURIComponent(itemDelegate.trayId) + ":" + objectPath
                                : "tray-fallback:" + itemDelegate.key
                        }
                        itemDelegate.resolvedOrderKey = root.statusLayout.registerItem(
                            itemDelegate, naturalKey, itemDelegate.key)
                        itemDelegate.orderKey = itemDelegate.resolvedOrderKey
                    }
                }
                Component.onDestruction: {
                    if (root.statusLayout)
                        root.statusLayout.unregisterItem(itemDelegate, itemDelegate.resolvedOrderKey)
                }
                onReadyChanged: root.statusLayout?.rebuild()
                onHoveredChanged: updateTooltip()
                onReorderStarted: {
                    if (root.tooltipSurface)
                        root.tooltipSurface.hideTooltip()
                }
                Connections {
                    target: root.statusLayout
                    function onDragActiveChanged() { itemDelegate.updateTooltip() }
                }
                onWheel: event => {
                    if (root.trayService) {
                        root.trayService.scroll(itemDelegate.key,
                            event.angleDelta.y, event.angleDelta.y !== 0 ? "vertical" : "horizontal")
                        event.accepted = true
                    }
                }
                onButtonClicked: (button, anchorX) => {
                    const anchor = root.anchorFor(itemDelegate,
                        itemDelegate.width / 2, itemDelegate.height / 2)
                    if (button === Qt.MiddleButton) {
                        if (root.trayService)
                            root.trayService.secondaryActivate(itemDelegate.key,
                                anchor.globalX, anchor.globalY)
                    } else if (button === Qt.RightButton) {
                        if (!root.presentContextMenu(itemDelegate) && root.trayService)
                            root.trayService.contextMenu(itemDelegate.key,
                                anchor.globalX, anchor.globalY)
                    } else if (button === Qt.LeftButton && itemDelegate.onlyMenu) {
                        if (!root.presentContextMenu(itemDelegate) && root.trayService) {
                            root.trayService.contextMenu(itemDelegate.key,
                                anchor.globalX, anchor.globalY)
                        }
                    } else if (button === Qt.LeftButton && root.trayService) {
                        root.trayService.activate(itemDelegate.key,
                            anchor.globalX, anchor.globalY)
                    }
                }
            }
        }
    }
}
