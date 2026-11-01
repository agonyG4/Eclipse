import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property string label: ""
    signal triggered()
    width: parent ? parent.width : 0
    height: Math.max(32, actionLabel.implicitHeight + 14)

    Rectangle {
        anchors.fill: parent
        radius: theme.shellRadiusMedium
        color: mouse.pressed ? theme.shellPressed
            : mouse.containsMouse && root.enabled ? theme.shellHover
            : "transparent"
    }

    Text {
        id: actionLabel
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.enabled ? theme.shellTextMain : theme.shellTextSecondary
        opacity: root.enabled ? 1 : 0.45
        font.family: theme.typography.role("label").family
        font.pixelSize: theme.typography.role("label").pixelSize
        font.weight: theme.typography.role("label").weight
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: root.enabled
        onClicked: root.triggered()
    }
}
