import QtQuick

TopbarIndicator {
    id: root

    ShellBarTheme { id: theme }

    property var clockService: null
    popupKind: 2
    objectName: "clock"
    implicitWidth: row.implicitWidth
    implicitHeight: 36
    width: implicitWidth
    height: implicitHeight

    Row {
        id: row
        objectName: "clockRow"
        spacing: 0

        Item {
            width: date.implicitWidth + 8
            height: root.height

            Text {
                id: date
                objectName: "clockDate"
                anchors.fill: parent
                text: root.clockService ? root.clockService.dateText : ""
                color: theme.shellTextSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font {
                    family: theme.fontFamilyDisplay
                    pixelSize: theme.fontSizeSmall
                    weight: Font.Medium
                    letterSpacing: 0.3
                }
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
            radius: 1
            color: theme.shellSeparator
            y: (root.height - height) / 2
        }

        Item {
            width: time.implicitWidth + 16
            height: root.height

            Text {
                id: time
                objectName: "clockTime"
                anchors.fill: parent
                text: root.clockService ? root.clockService.timeText : ""
                color: theme.shellTextActive
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font {
                    family: theme.fontFamilyDisplay
                    pixelSize: theme.fontSizeTitle
                    weight: Font.Medium
                    letterSpacing: 0.35
                }
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
