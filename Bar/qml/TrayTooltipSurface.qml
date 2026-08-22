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
    readonly property string tooltipDescription: statusNotifierService
        ? statusNotifierService.tooltipDescriptionForItem(itemKey) : ""

    visible: false
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
    }

    Rectangle {
        id: tooltipCard
        x: Math.max(0, Math.min(window.width - width, window.anchorX - width / 2))
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(260, Math.max(40, tooltipText.implicitWidth + 20))
        height: 28
        radius: theme.shellRadiusSmall
        color: theme.shellBackground
        border.color: theme.shellBorder
        border.width: 1

        Text {
            id: tooltipText
            anchors.centerIn: parent
            width: Math.min(240, implicitWidth)
            text: window.tooltipDescription
                ? window.tooltipTitle + " — " + window.tooltipDescription
                : window.tooltipTitle
            color: theme.shellTextMain
            elide: Text.ElideRight
            font.family: theme.fontFamilyText
            font.pixelSize: theme.fontSizeCaption
            maximumLineCount: 1
        }
    }
}
