import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var statusNotifierService: null
    property int outputWidth: 1
    property int outputHeight: 1
    property string itemKey: ""
    property real anchorX: 0
    property bool tooltipVisible: false
    readonly property string tooltipTitle: statusNotifierService
        ? statusNotifierService.tooltipTitleForItem(itemKey) : ""

    visible: tooltipVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowTransparentForInput
    width: outputWidth
    height: 28

    ShellBarTheme { id: theme }

    function showTooltip(key, x) {
        itemKey = key
        anchorX = x
        tooltipVisible = true
    }

    function hideTooltip() {
        tooltipVisible = false
        itemKey = ""
    }

    Connections {
        target: window.statusNotifierService
        function onItemRemoved(key) {
            if (key === window.itemKey)
                window.hideTooltip()
        }
    }

    Rectangle {
        id: tooltipCard
        objectName: "tooltipCard"
        visible: window.tooltipVisible && window.tooltipTitle !== ""
        x: Math.max(0, Math.min(window.width - width, window.anchorX - width / 2))
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(260, tooltipText.implicitWidth + 18)
        height: 28
        radius: theme.shellRadiusSmall
        color: theme.shellBackground
        border.color: theme.shellBorder
        border.width: 1

        Text {
            id: tooltipText
            objectName: "tooltipText"
            anchors.centerIn: parent
            width: parent.width - 12
            text: window.tooltipTitle
            color: theme.shellTextActive
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.family: theme.fontFamily
            font.pixelSize: theme.fontSizeCaption
            maximumLineCount: 1
        }
    }
}
