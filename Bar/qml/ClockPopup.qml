import QtQuick
import "components"

PopupCard {
    id: root

    ShellBarTheme { id: theme }

    property var clockService: null
    implicitWidth: 220

    Text {
        text: root.clockService ? root.clockService.timeText : ""
        color: root.clockService ? theme.shellTextActive : "#ffffffff"
        font.pixelSize: 28
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        width: parent.width
    }
    Text {
        text: root.clockService ? root.clockService.dateText : ""
        color: root.clockService ? theme.shellTextSecondary : "#99ffffff"
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        width: parent.width
    }
}
