import QtQuick
import Astrea.Shared as Shared

Shared.ShellCard {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    implicitWidth: 200
    property int cardPadding: 18
    property int contentSpacing: 14
    property color backgroundColor: theme.background
    property color borderColor: theme.border
    // Attached popups hang from the bar edge, so their top corners are square.
    property bool attachedTop: false
    implicitHeight: content.implicitHeight + cardPadding * 2
    width: implicitWidth
    height: implicitHeight
    material: theme.material
    cardRadius: theme.shellRadiusLarge
    color: backgroundColor
    border.color: borderColor
    topLeftRadius: attachedTop ? 0 : radius
    topRightRadius: attachedTop ? 0 : radius

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
