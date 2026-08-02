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

    implicitHeight: 40
    activeFocusOnTab: itemEnabled
    opacity: itemEnabled ? 1 : Theme.opacityDisabled

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: Theme.radiusMedium
        color: root.selected
            ? Theme.surfaceSelected
            : pressArea.pressed ? Theme.surfacePressed
            : hoverHandler.hovered ? Theme.surfaceHover
            : "transparent"
        border.width: root.selected || root.activeFocus ? 1 : 0
        border.color: root.selected ? Theme.accent : Theme.focusRing

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: root.selected ? 22 : 0
            radius: 1.5
            color: Theme.accent
            visible: root.selected

            Behavior on height {
                NumberAnimation {
                    duration: Theme.animationNormal
                    easing.type: Easing.OutCubic
                }
            }
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 15 : 16
        anchors.rightMargin: 12
        spacing: 12

        AstreaAppIcon {
            Layout.preferredWidth: root.compact ? 24 : 28
            Layout.preferredHeight: root.compact ? 24 : 28
            Layout.alignment: Qt.AlignVCenter
            iconName: root.iconName
            appName: root.title
            iconSize: root.compact ? 20 : 24
            sourcePixelSize: root.compact ? 48 : 56
            iconRadius: 7
            fallbackRadius: 7
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
