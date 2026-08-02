import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property var controller
    property bool windowMaximized: false
    property bool sidebarCollapsed: false
    property real sidebarWidth: sidebarCollapsed ? 78 : 256

    Behavior on sidebarWidth {
        NumberAnimation {
            duration: Theme.animationNormal
            easing.type: Easing.OutCubic
        }
    }

    signal moveWindowRequested()
    signal minimizeRequested()
    signal maximizeRestoreRequested()
    signal closeRequested()

    radius: windowMaximized ? 0 : Theme.radiusWindow
    color: "#10243F"
    border.width: 1
    border.color: Theme.windowBorder
    clip: true

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        opacity: 0.92
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.windowGradientStart }
            GradientStop { position: 0.46; color: Theme.windowBackground }
            GradientStop { position: 1.0; color: Theme.windowGradientEnd }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowWash
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        width: parent.width * 0.58
        height: parent.height * 0.54
        radius: width * 0.5
        color: Theme.windowGlow
        opacity: Theme.dark ? 0.12 : 0.22
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, root.radius - 1)
        color: "transparent"
        border.width: 1
        border.color: Theme.windowHighlight
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        WindowTitleBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            collapsed: root.sidebarCollapsed
            maximized: root.windowMaximized
            onCollapseRequested: root.sidebarCollapsed = !root.sidebarCollapsed
            onMoveRequested: root.moveWindowRequested()
            onMinimizeRequested: root.minimizeRequested()
            onMaximizeRestoreRequested: root.maximizeRestoreRequested()
            onCloseRequested: root.closeRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SettingsSidebar {
                Layout.preferredWidth: root.sidebarWidth
                Layout.fillHeight: true
                controller: root.controller
                collapsed: root.sidebarCollapsed
            }

            EmptyContent {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sectionTitle: root.controller.selectedSectionTitle
            }
        }
    }
}
