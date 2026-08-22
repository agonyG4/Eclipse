import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var workspaceModel: null
    property bool activationAvailable: false
    readonly property int workspaceCount: repeater.count
    readonly property int stableWidth: workspaceCount <= 0 ? 0
        : theme.workspaceActiveWidth
          + Math.max(0, workspaceCount - 1)
              * (theme.workspaceDotSize + theme.spacingSmall)
    readonly property int reservedWorkspaceSlots: 10
    readonly property int reservedWidth: theme.workspaceActiveWidth
        + Math.max(0, reservedWorkspaceSlots - 1)
            * (theme.workspaceDotSize + theme.spacingSmall)
    signal workspaceActivated(string workspaceId)
    implicitWidth: stableWidth
    implicitHeight: theme.workspaceDotSize
    width: implicitWidth
    height: implicitHeight
    clip: true

    Row {
        id: row
        objectName: "workspaceRow"
        anchors.left: parent.left
        spacing: theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        Repeater {
            id: repeater
            objectName: "workspaceRepeater"
            model: root.workspaceModel

            delegate: Item {
                objectName: "workspaceDelegate"
                required property string id
                required property bool active
                required property bool occupied
                required property bool urgent
                required property string protocolId
                property string workspaceId: protocolId.length > 0 ? protocolId : id
                readonly property int workspaceWidthAnimationDuration: theme.animationNormal
                readonly property int workspaceColorAnimationDuration: theme.animationNormal - 20
                readonly property int workspaceWidthAnimationEasing: Easing.OutExpo

                width: active ? theme.workspaceActiveWidth : theme.workspaceDotSize
                height: theme.workspaceDotSize

                Behavior on width {
                    NumberAnimation {
                        objectName: "workspaceWidthAnimation"
                        duration: theme.animationNormal
                        easing.type: Easing.OutExpo
                    }
                }

                Rectangle {
                    objectName: "workspaceDot"
                    anchors.centerIn: parent
                    width: parent.width
                    height: theme.workspaceDotSize
                    radius: height / 2
                    color: active ? theme.workspaceActive : theme.workspaceInactive
                    Behavior on color {
                        ColorAnimation {
                            objectName: "workspaceColorAnimation"
                            duration: theme.animationNormal - 20
                        }
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
                    enabled: root.activationAvailable
                    hoverEnabled: root.activationAvailable
                    cursorShape: root.activationAvailable ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.workspaceActivated(workspaceId)
                }
            }
        }
    }
}
