import QtQuick
import "../../Bar/qml/components"

Rectangle {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    property int cardPadding: 10
    property int contentSpacing: 3
    implicitWidth: 280
    implicitHeight: content.implicitHeight + cardPadding * 2
    radius: theme.shellRadiusLarge
    color: theme.background
    border.color: theme.border
    border.width: 1

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
