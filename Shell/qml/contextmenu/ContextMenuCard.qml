import QtQuick
import Astrea.Shared as Shared

Shared.ShellCard {
    id: root
    objectName: "contextMenuCard"

    ShellMenuTheme { id: theme }

    default property alias contentData: content.data
    property int cardPadding: theme.contextMenuCardPadding
    property int contentSpacing: theme.contextMenuRowSpacing
    implicitWidth: content.implicitWidth + cardPadding * 2
    implicitHeight: content.implicitHeight + cardPadding * 2
    material: theme.material
    cardRadius: theme.contextMenuRadius

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: root.cardPadding
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }

    MouseArea {
        anchors.fill: parent
        z: -1
    }
}
