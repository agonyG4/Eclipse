import QtQuick

Rectangle {
    id: root

    default property alias contentData: content.data
    property color backgroundColor: Theme.sidebarBackground
    property color washColor: Theme.sidebarWash
    property color borderColor: Theme.sidebarBorder

    color: backgroundColor
    border.width: 1
    border.color: borderColor

    Rectangle {
        anchors.fill: parent
        color: root.washColor
    }

    Item {
        id: content
        anchors.fill: parent
    }
}
