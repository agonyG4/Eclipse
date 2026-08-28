import QtQuick

Rectangle {
    id: root

    ShellMenuTheme { id: theme }

    property string token: ""
    property string label: ""
    property string icon: ""
    property string shortcut: ""
    property bool nodeEnabled: true
    property bool selected: false
    property bool hasChildren: false
    property int checkType: 0
    property int checkState: 0
    property bool destructive: false
    readonly property bool mouseHovered: mouse.containsMouse
    signal hovered()
    signal triggered()

    Accessible.role: Accessible.MenuItem
    Accessible.name: root.label
    Accessible.description: root.shortcut

    height: theme.contextMenuNormalRowHeight
    radius: theme.shellRadiusMedium
    color: root.selected && root.nodeEnabled ? theme.shellHover
        : mouse.pressed && root.nodeEnabled ? theme.shellPressed : "transparent"
    opacity: root.nodeEnabled ? 1 : theme.opacityMuted

    Row {
        anchors.fill: parent
        anchors.leftMargin: theme.contextMenuRowHorizontalMargin
        anchors.rightMargin: theme.contextMenuRowHorizontalMargin
        spacing: theme.contextMenuRowSpacing

        Item {
            id: iconSlot
            width: theme.contextMenuIconSlotWidth
            height: parent.height

            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                sourceSize: Qt.size(18, 18)
                visible: root.icon !== "" && (root.icon.indexOf("image://") === 0
                                               || root.icon.indexOf("file://") === 0
                                               || root.icon.indexOf("qrc:/") === 0
                                               || root.icon.indexOf("/") === 0)
                source: visible ? root.icon : ""
                fillMode: Image.PreserveAspectFit
            }

            Text {
                anchors.centerIn: parent
                visible: root.icon !== "" && !parent.children[0].visible
                text: root.icon
                color: root.destructive ? theme.shellIconWarning : theme.shellIconMain
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
                elide: Text.ElideRight
            }

            Text {
                anchors.centerIn: parent
                visible: root.checkType !== 0
                text: root.checkType === 2
                    ? (root.checkState === 2 ? "●" : "○")
                    : (root.checkState === 2 ? "✓" : "")
                color: root.nodeEnabled ? theme.shellTextActive : theme.shellTextSecondary
                font.pixelSize: theme.fontSizeBody
            }
        }

        Text {
            id: labelText
            LayoutMirroring.enabled: false
            width: Math.max(0, parent.width - iconSlot.width
                                - (shortcutText.visible ? shortcutText.implicitWidth : 0)
                                - (arrowText.visible ? arrowText.implicitWidth : 0)
                                - parent.spacing * (1 + (shortcutText.visible ? 1 : 0)
                                                     + (arrowText.visible ? 1 : 0)))
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            color: root.destructive ? theme.shellIconWarning : theme.shellTextActive
            elide: Text.ElideRight
            font { family: theme.fontFamily; pixelSize: theme.fontSizeBody; weight: Font.Medium }
        }

        Text {
            id: shortcutText
            anchors.verticalCenter: parent.verticalCenter
            text: root.shortcut
            color: theme.shellTextSecondary
            font { family: theme.fontFamily; pixelSize: theme.fontSizeSmall }
            visible: root.shortcut !== ""
        }

        Text {
            id: arrowText
            anchors.verticalCenter: parent.verticalCenter
            text: root.hasChildren ? "›" : ""
            color: theme.shellTextSecondary
            font { family: theme.fontFamily; pixelSize: theme.fontSizeBody; weight: Font.Medium }
            visible: root.hasChildren
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.nodeEnabled
        hoverEnabled: true
        cursorShape: root.nodeEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onEntered: root.hovered()
        onClicked: root.triggered()
    }
}
