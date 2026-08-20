import QtQuick

Item {
    id: control

    ShellBarTheme { id: theme }

    property bool active: false
    property bool interactive: true
    property string mouseAreaObjectName: ""
    property int fixedWidth: 0
    property int horizontalPadding: theme.spacingContainer
    property int backgroundMargin: theme.spacingTiny
    property real backgroundRadius: theme.shellRadiusMedium - 2
    property color activeColor: theme.shellActive
    property color pressedColor: theme.shellPressed
    property color hoverColor: theme.shellHover
    property color idleColor: "transparent"
    property alias spacing: contentRow.spacing
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool pressed: mouseArea.pressed

    default property alias contentData: contentRow.data

    signal clicked(real anchorX)
    signal wheel(var event)

    width: fixedWidth > 0 ? fixedWidth : contentRow.implicitWidth + horizontalPadding
    height: 34

    Rectangle {
        anchors.fill: parent
        anchors.margins: control.backgroundMargin
        radius: control.backgroundRadius
        color: control.active ? control.activeColor
             : control.pressed ? control.pressedColor
             : control.hovered ? control.hoverColor
             : control.idleColor

        Behavior on color { ColorAnimation { duration: theme.animationMicro } }
    }

    HoverHandler { id: hoverHandler }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: theme.spacingMicro
    }

    MouseArea {
        id: mouseArea
        objectName: control.mouseAreaObjectName
        anchors.fill: parent
        enabled: control.interactive
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: control.interactive
        cursorShape: control.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: {
            const point = control.mapToItem(null, control.width / 2, control.height / 2)
            control.clicked(point.x)
        }

        onWheel: event => control.wheel(event)
    }
}
