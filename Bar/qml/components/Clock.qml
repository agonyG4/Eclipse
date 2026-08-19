import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var clockService: null
    property bool interactive: false
    signal clicked()
    objectName: "clock"
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    width: implicitWidth
    height: implicitHeight

    Row {
        id: row
        objectName: "clockRow"
        anchors.centerIn: parent
        spacing: 8

        Text {
            id: date
            objectName: "clockDate"
            text: root.clockService ? root.clockService.dateText : ""
            color: theme.shellTextSecondary
            font.pixelSize: 10
            font.family: theme.shellFontFamily
            font.weight: theme.shellFontWeightNormal
            font.letterSpacing: theme.shellClockTracking
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            objectName: "clockSeparator"
            width: 1
            height: 16
            radius: 1
            color: theme.shellSeparator
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: time
            objectName: "clockTime"
            text: root.clockService ? root.clockService.timeText : ""
            color: theme.shellTextMain
            font.pixelSize: 13
            font.family: theme.shellFontFamily
            font.weight: theme.shellFontWeightMedium
            font.letterSpacing: theme.shellClockTracking
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.clicked()
    }

    SequentialAnimation {
        id: minuteTransition
        NumberAnimation { target: time; property: "opacity"; to: 0.25; duration: theme.animationQuick / 2 }
        NumberAnimation { target: time; property: "opacity"; to: 1; duration: theme.animationQuick }
    }

    Connections {
        target: root.clockService
        function onChanged() { minuteTransition.restart() }
    }
}
