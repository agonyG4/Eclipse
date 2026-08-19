import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var workspaceModel: null
    implicitWidth: row.implicitWidth
    implicitHeight: theme.workspaceDotSize
    width: implicitWidth
    height: implicitHeight

    Row {
        id: row
        spacing: 4
        anchors.verticalCenter: parent.verticalCenter

        Repeater {
            model: root.workspaceModel

            delegate: Item {
                required property bool active
                required property bool occupied
                required property bool urgent

                width: active ? theme.workspaceActiveWidth : theme.workspaceDotSize
                height: theme.workspaceDotSize

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: theme.workspaceDotSize
                    radius: height / 2
                    color: urgent ? "#ff375f"
                        : active ? theme.shellTextActive
                        : occupied ? theme.shellTextSecondary
                        : theme.shellIconMuted
                    Behavior on width {
                        NumberAnimation {
                            duration: theme.animationQuick
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }
}
