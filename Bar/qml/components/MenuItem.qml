import QtQuick

Rectangle {
    id: root
    objectName: "menuItem"

    ShellBarTheme { id: theme }

    property string icon: ""
    property string text: ""
    property bool selected: false
    property bool iconIsImage: icon.indexOf("image://") === 0
        || icon.indexOf("file://") === 0
        || icon.indexOf("qrc:/") === 0
        || icon.indexOf("/") === 0
    signal clicked()
    signal hovered()

    width: parent ? parent.width : 0
    height: 36
    radius: theme.shellRadiusMedium
    color: root.selected && root.enabled ? theme.shellHover
        : mouse.containsMouse && root.enabled ? theme.shellSeparator : "transparent"
    opacity: root.enabled ? 1 : theme.opacityMuted

    Behavior on color { ColorAnimation { duration: theme.animationFast } }
    Behavior on opacity { NumberAnimation { duration: theme.animationFast } }

    Row {
        anchors.fill: parent
        objectName: "menuItemRow"
        anchors.leftMargin: 12
        spacing: theme.spacingLarge

        Item {
            objectName: "menuItemIconSlot"
            width: root.icon === "" ? 0 : theme.fontSizeIcon
            height: parent.height

            Image {
                anchors.centerIn: parent
                width: Math.min(theme.fontSizeIcon, parent.width)
                height: width
                sourceSize: Qt.size(width, height)
                visible: root.iconIsImage
                source: root.iconIsImage ? root.icon : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Text {
                anchors.centerIn: parent
                visible: !root.iconIsImage
                text: root.icon
                color: mouse.containsMouse && root.enabled
                    ? theme.shellIconActive : theme.shellIconMain
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
                Behavior on color { ColorAnimation { duration: theme.animationFast } }
            }
        }

        Text {
            objectName: "menuItemText"
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - 12
            text: root.text
            color: mouse.containsMouse && root.enabled
                ? theme.shellTextActive : theme.shellTextSecondary
            elide: Text.ElideRight
            font { family: theme.fontFamily; pixelSize: theme.fontSizeBody; weight: Font.Medium }
            Behavior on color { ColorAnimation { duration: theme.animationFast } }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: root.hovered()
        onClicked: root.clicked()
    }
}
