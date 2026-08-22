import QtQuick
import "components"

Item {
    id: root

    ShellBarTheme { id: theme }

    property var trayService: null
    property var popupController: null
    property var tooltipSurface: null
    property int outputWidth: 1
    property int statusLeft: 0
    readonly property int itemCount: trayRepeater.count
    implicitWidth: trayRow.implicitWidth
    implicitHeight: 36
    width: implicitWidth
    height: 36

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

                width: ready ? 28 : 0
                height: 36
                visible: ready

                Rectangle {
                    id: hitSurface
                    objectName: "trayItemSurface"
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    radius: theme.shellRadiusMedium
                    color: mouse.pressed ? theme.shellPressed
                         : hover.hovered ? theme.shellSeparator : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: theme.animationFast }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        sourceSize: Qt.size(16, 16)
                        source: itemDelegate.iconSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: status !== Image.Error && status !== Image.Null
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !itemDelegate.iconSource
                        text: "•"
                        color: theme.shellIconMain
                        font.pixelSize: theme.fontSizeIcon
                    }
                }

                HoverHandler { id: hover }

                Timer {
                    id: tooltipTimer
                    interval: 420
                    repeat: false
                    onTriggered: {
                        if (root.tooltipSurface) {
                            const point = itemDelegate.mapToItem(null,
                                itemDelegate.width / 2, itemDelegate.height / 2)
                            root.tooltipSurface.showTooltip(itemDelegate.key, point.x)
                        }
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    enabled: itemDelegate.ready
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    hoverEnabled: true
                    onEntered: tooltipTimer.restart()
                    onExited: {
                        tooltipTimer.stop()
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
                        const point = itemDelegate.mapToItem(null,
                            itemDelegate.width / 2, itemDelegate.height / 2)
                        if (mouse.button === Qt.MiddleButton) {
                            if (root.trayService)
                                root.trayService.secondaryActivate(itemDelegate.key,
                                    Math.round(point.x), Math.round(point.y))
                        } else if (mouse.button === Qt.RightButton
                                   || (mouse.button === Qt.LeftButton && itemDelegate.onlyMenu)) {
                            if (itemDelegate.hasMenu && root.popupController)
                                root.popupController.toggleTrayMenu(Math.round(point.x),
                                                                    itemDelegate.key)
                        } else if (itemDelegate.hasMenu && root.popupController) {
                            root.popupController.toggleTrayMenu(Math.round(point.x), itemDelegate.key)
                        } else if (root.trayService) {
                            root.trayService.activate(itemDelegate.key,
                                Math.round(point.x), Math.round(point.y))
                        }
                    }
                }
            }
        }
    }
}
