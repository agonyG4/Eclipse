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

            delegate: Item {
                id: itemDelegate
                objectName: "trayItem"
                required property string key
                required property string title
                required property string iconSource
                required property bool hasMenu
                required property bool onlyMenu
                required property bool ready
                readonly property bool hasUsableMenu: {
                    if (!root.trayService)
                        return false
                    root.trayService.presentationRevision
                    return root.trayService.hasUsableMenuForItem(itemDelegate.key)
                }

                width: ready ? 28 : 0
                height: 28
                visible: ready

                Rectangle {
                    id: hitSurface
                    objectName: "trayItemSurface"
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    radius: theme.shellRadiusMedium
                    color: mouse.pressed ? Qt.rgba(1, 1, 1, 0.2)
                         : hover.hovered ? theme.shellSeparator : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: theme.animationFast }
                    }

                    Image {
                        objectName: "trayItemIcon"
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        sourceSize: Qt.size(16, 16)
                        source: itemDelegate.iconSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: status !== Image.Error && status !== Image.Null
                    }

                }

                HoverHandler { id: hover }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: itemDelegate.ready
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: itemDelegate.ready ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onEntered: {
                        if (root.tooltipSurface && root.trayService
                                && root.trayService.tooltipTitleForItem(itemDelegate.key) !== "") {
                            const anchor = root.anchorFor(itemDelegate,
                                itemDelegate.width / 2, itemDelegate.height / 2)
                            root.tooltipSurface.showTooltip(itemDelegate.key, anchor.localX)
                        }
                    }
                    onExited: {
                        if (root.tooltipSurface)
                            root.tooltipSurface.hideTooltip()
                    }
                    onWheel: event => {
                        if (root.trayService) {
                            root.trayService.scroll(itemDelegate.key,
                                event.angleDelta.y, event.angleDelta.y !== 0 ? "vertical" : "horizontal")
                            event.accepted = true
                        }
                    }
                    onClicked: mouse => {
                        const anchor = root.anchorFor(itemDelegate,
                            itemDelegate.width / 2, itemDelegate.height / 2)
                        if (mouse.button === Qt.MiddleButton) {
                            if (root.trayService)
                                root.trayService.secondaryActivate(itemDelegate.key,
                                    anchor.globalX, anchor.globalY)
                        } else if (mouse.button === Qt.RightButton) {
                            if (!root.presentContextMenu(itemDelegate) && root.trayService)
                                root.trayService.contextMenu(itemDelegate.key,
                                    anchor.globalX, anchor.globalY)
                        } else if (mouse.button === Qt.LeftButton && itemDelegate.onlyMenu) {
                            if (!root.presentContextMenu(itemDelegate) && root.trayService) {
                                root.trayService.contextMenu(itemDelegate.key,
                                    anchor.globalX, anchor.globalY)
                            }
                        } else if (root.trayService) {
                            root.trayService.activate(itemDelegate.key,
                                anchor.globalX, anchor.globalY)
                        }
                    }
                }
            }
        }
    }
}
