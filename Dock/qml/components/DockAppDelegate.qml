import QtQuick
import QtQuick.Controls.Basic
import Astrea.Shared as Shared

Item {
    id: root

    required property int index
    required property string desktopFileName
    required property string displayName
    required property string iconName
    required property string iconPath
    required property string iconUrl
    required property bool resolved
    required property bool launching
    required property bool runtimeKnown
    required property bool running
    required property bool active
    required property int windowCount

    property int iconSize: 48
    property bool hovered: mouseArea.containsMouse
    signal activated(int row)

    width: iconSize + 8
    height: iconSize + 14

    SystemPalette { id: systemPalette }

    Shared.AstreaAppIcon {
        id: appIcon
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        iconName: root.iconName
        iconPath: root.iconPath
        iconUrl: root.iconUrl
        appName: root.displayName
        sourcePixelSize: root.iconSize * 2
        iconRadius: 10
        fallbackRadius: 10
    }

    scale: hovered ? 1.1 : 1.0
    y: hovered ? -5 : 0
    Behavior on scale {
        NumberAnimation { duration: 125; easing.type: Easing.OutCubic }
    }
    Behavior on y {
        NumberAnimation { duration: 125; easing.type: Easing.OutCubic }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.activated(root.index)

        ToolTip.visible: containsMouse
        ToolTip.delay: 420
        ToolTip.text: root.runtimeKnown && root.running
            ? qsTr("%1 (%2 windows)").arg(root.displayName).arg(root.windowCount)
            : qsTr("%1").arg(root.displayName)
    }

    Rectangle {
        anchors.horizontalCenter: appIcon.horizontalCenter
        anchors.top: appIcon.bottom
        anchors.topMargin: 2
        width: root.active ? 18 : 8
        height: 3
        radius: 1.5
        color: systemPalette.highlight
        visible: root.runtimeKnown && root.running
    }

    Rectangle {
        anchors.fill: appIcon
        radius: appIcon.iconRadius
        color: "#18FFFFFF"
        visible: mouseArea.pressed
    }
}
