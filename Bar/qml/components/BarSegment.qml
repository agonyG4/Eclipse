import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    default property alias contentData: content.data
    property bool interactive: false
    property bool active: false
    property int segmentHeight: theme.pillHeight
    property int fixedWidth: 0
    property int horizontalPadding: 10
    property alias spacing: content.spacing
    signal clicked()

    implicitWidth: content.implicitWidth + horizontalPadding * 2
    implicitHeight: segmentHeight
    width: fixedWidth > 0 ? fixedWidth : implicitWidth
    height: segmentHeight
    opacity: 0

    function reveal() { opacity = 1 }

    Component.onCompleted: reveal()

    HoverHandler { id: hoverHandler }

    Rectangle {
        objectName: "barSegmentSurface"
        anchors.fill: parent
        radius: theme.shellRadiusLarge - 2
        color: theme.shellBackground
        border.color: hoverHandler.hovered ? theme.shellBorderHover : theme.shellBorder
        border.width: 1
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
