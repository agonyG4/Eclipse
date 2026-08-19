import QtQuick

Rectangle {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    implicitWidth: 200
    implicitHeight: content.implicitHeight + 24
    width: implicitWidth
    height: implicitHeight
    radius: theme.shellRadiusLarge
    color: theme.shellBackground
    border.color: theme.shellBorder
    border.width: 1

    Column {
        id: content
        z: 1
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4
    }

    // Consume clicks inside the card so the overlay's outside-click handler
    // never closes a popup after an action has already handled the event.
    MouseArea {
        anchors.fill: parent
        z: 0
    }
}
