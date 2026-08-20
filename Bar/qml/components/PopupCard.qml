import QtQuick

Rectangle {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    implicitWidth: 200
    property int cardPadding: 18
    property int contentSpacing: 14
    property color backgroundColor: theme.popupBackground
    property color borderColor: theme.popupBorder
    implicitHeight: content.implicitHeight + cardPadding * 2
    width: implicitWidth
    height: implicitHeight
    radius: theme.shellRadiusLarge
    color: backgroundColor
    border.color: borderColor
    border.width: 1

    Column {
        id: content
        z: 1
        anchors.fill: parent
        anchors.margins: root.cardPadding
        spacing: root.contentSpacing
    }

    // Consume clicks inside the card so the overlay's outside-click handler
    // never closes a popup after an action has already handled the event.
    MouseArea {
        anchors.fill: parent
        z: 0
    }
}
