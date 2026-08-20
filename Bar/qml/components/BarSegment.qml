import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    property bool interactive: false
    property bool active: false
    property int horizontalPadding: 12
    property alias spacing: content.spacing
    signal clicked()

    implicitWidth: content.implicitWidth + horizontalPadding * 2
    implicitHeight: theme.pillHeight
    width: implicitWidth
    height: implicitHeight
    opacity: 0

    function reveal() { opacity = 1 }

    Component.onCompleted: reveal()

    HoverHandler { id: hoverHandler }

    Rectangle {
        objectName: "barSegmentSurface"
        anchors.fill: parent
        radius: theme.shellRadiusLarge - 2
        color: root.active ? theme.shellActive
            : mouse.pressed ? theme.shellPressed
            : hoverHandler.hovered ? theme.shellHover
            : theme.shellBackground
        border.color: hoverHandler.hovered ? theme.shellBorderHover : theme.shellBorder
        border.width: 1
        Behavior on color { ColorAnimation { duration: theme.animationQuick } }
        Behavior on border.color { ColorAnimation { duration: theme.animationNormal } }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: theme.spacing
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: false
        onClicked: root.clicked()
    }

    Behavior on opacity {
        NumberAnimation { duration: theme.animationSlow; easing.type: Easing.OutCubic }
    }
}
