import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var workspaceModel: null
    signal workspaceActivated(string workspaceId)
    implicitWidth: row.implicitWidth
    implicitHeight: theme.workspaceDotSize
    width: implicitWidth
    height: implicitHeight

    Row {
        id: row
        objectName: "workspaceRow"
        spacing: theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        Repeater {
            objectName: "workspaceRepeater"
            model: root.workspaceModel

            delegate: Item {
                objectName: "workspaceDelegate"
                required property string id
                required property bool active
                required property bool occupied
                required property bool urgent
                property string workspaceId: id

                width: active ? theme.workspaceActiveWidth : theme.workspaceDotSize
                height: theme.workspaceDotSize

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: theme.workspaceDotSize
                    radius: height / 2
                    color: urgent ? theme.shellIconWarning
                        : active ? theme.workspaceActive
                        : occupied ? theme.workspaceInactive
                        : theme.shellIconMuted
                    Behavior on width {
                        NumberAnimation {
                            duration: theme.animationNormal
                            easing.type: Easing.OutExpo
                        }
                    }
                    Behavior on color {
                        ColorAnimation { duration: theme.animationNormal - 20 }
                    }
                }

                MouseArea {
                    id: hitTarget
                    objectName: "workspaceHitTarget"
                    anchors.fill: parent
                    anchors.topMargin: -10
                    anchors.bottomMargin: -10
                    anchors.leftMargin: -6
                    anchors.rightMargin: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.workspaceActivated(workspaceId)
                }
            }
        }
    }
}
