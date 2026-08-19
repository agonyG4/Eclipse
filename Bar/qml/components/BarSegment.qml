import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    property bool interactive: false
    property bool active: false
    property int horizontalPadding: 12
    signal clicked()

    implicitWidth: content.implicitWidth + horizontalPadding * 2
    implicitHeight: theme.pillHeight
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        objectName: "barSegmentSurface"
        anchors.fill: parent
        radius: theme.shellRadiusLarge - 2
        color: root.active ? theme.shellActive
            : mouse.pressed ? theme.shellPressed
            : mouse.containsMouse ? theme.shellHover
            : theme.shellBackground
        border.color: mouse.containsMouse ? theme.shellBorderHover : theme.shellBorder
        border.width: 1
        Behavior on color { ColorAnimation { duration: theme.animationQuick } }
        Behavior on border.color { ColorAnimation { duration: theme.animationNormal } }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: root.interactive
        onClicked: root.clicked()
    }
}
