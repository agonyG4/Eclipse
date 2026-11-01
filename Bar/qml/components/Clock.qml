import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var clockService: null
    property var statusLayout: null
    objectName: "clock"
    implicitWidth: clockButton.width
    implicitHeight: 36
    width: clockButton.width
    height: 36
    onWidthChanged: {
        if (root.statusLayout)
            root.statusLayout.rebuild()
    }

    TopbarIndicator {
        id: clockButton
        objectName: "clockButton"
        anchors.centerIn: parent
        interactive: false
        reorderable: !!root.statusLayout
        statusLayout: root.statusLayout
        orderKey: "builtin:clock"
        reorderItem: root
        horizontalPadding: theme.spacingTiny
        spacing: 0

        Row {
            id: row
            objectName: "clockRow"
            spacing: 0

            Item {
                width: date.implicitWidth + 8
                height: clockButton.height

                Text {
                    id: date
                    objectName: "clockDate"
                    anchors.fill: parent
                    text: root.clockService ? root.clockService.dateText : ""
                    color: theme.shellTextSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: theme.typography.font("caption")
                    lineHeight: theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    renderType: Text.NativeRendering
                    Behavior on text {
                        SequentialAnimation {
                            NumberAnimation { target: date; property: "opacity"; to: 0; duration: theme.animationQuick }
                            NumberAnimation { target: date; property: "opacity"; to: 1; duration: theme.animationQuick }
                        }
                    }
                }
            }

            Rectangle {
                objectName: "clockSeparator"
                width: 1
                height: 16
                color: theme.shellSeparator
                anchors.verticalCenter: parent.verticalCenter
            }

            Item {
                width: time.implicitWidth + 16
                height: clockButton.height

                Text {
                    id: time
                    objectName: "clockTime"
                    anchors.fill: parent
                    text: root.clockService ? root.clockService.timeText : ""
                    color: theme.shellTextActive
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: theme.typography.font("label")
                    lineHeight: theme.typography.role("label").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    renderType: Text.NativeRendering
                    Behavior on text {
                        SequentialAnimation {
                            NumberAnimation { target: time; property: "opacity"; to: 0; duration: theme.animationFast; easing.type: Easing.InQuad }
                            NumberAnimation { target: time; property: "opacity"; to: 1; duration: theme.animationNormal; easing.type: Easing.OutQuad }
                        }
                    }
                }
            }
        }
    }
}
