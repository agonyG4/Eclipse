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
        anchors.fill: parent
        radius: height / 2
        color: mouse.pressed ? theme.shellPressed
            : root.active ? theme.shellActive
            : mouse.containsMouse ? theme.shellHover
            : theme.shellSurface
        border.color: mouse.containsMouse ? theme.shellBorder : "transparent"
        border.width: 1
        Behavior on color { ColorAnimation { duration: theme.animationQuick } }
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
