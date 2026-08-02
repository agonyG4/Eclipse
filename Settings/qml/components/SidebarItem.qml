import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Astrea.Shared 1.0

Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property bool compact: false
    property bool selected: false
    property bool itemEnabled: true

    signal clicked()

    implicitHeight: 42
    activeFocusOnTab: itemEnabled
    opacity: itemEnabled ? 1 : Theme.opacityDisabled

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: root.selected
            ? Theme.surfaceSelected
            : pressArea.pressed ? Theme.surfacePressed
            : hoverHandler.hovered ? Theme.surfaceHover
            : "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.focusRing

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 15 : 12
        anchors.rightMargin: 12
        spacing: 11

        AstreaAppIcon {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter
            iconName: root.iconName
            appName: root.title
            iconSize: 24
            sourcePixelSize: 48
            iconRadius: 5
            fallbackRadius: 5
            fallbackFontSize: 10
            fallbackColor: Theme.surfaceHover
            fallbackTextColor: root.selected ? Theme.accent : Theme.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 0
            visible: !root.compact

            Text {
                Layout.fillWidth: true
                text: root.title
                color: root.selected ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                font.weight: root.selected ? Theme.fontWeightDemiBold : Theme.fontWeightMedium
                elide: Text.ElideRight
            }
        }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.itemEnabled
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        enabled: root.itemEnabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: root.compact && hoverHandler.hovered
    ToolTip.text: root.title
    ToolTip.delay: 450

    Keys.onSpacePressed: if (root.itemEnabled) root.clicked()
    Keys.onEnterPressed: if (root.itemEnabled) root.clicked()
    Accessible.role: Accessible.Button
    Accessible.name: root.title
    Accessible.description: root.subtitle
}
