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
    color: Theme.windowBackground
    border.width: 1
    border.color: Theme.windowBorder
    clip: true

    Rectangle {
        anchors.fill: parent
        color: Theme.windowWash
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        WindowTitleBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
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
