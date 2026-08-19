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
        anchors.centerIn: parent
        spacing: 8

        Text {
            id: date
            objectName: "clockDate"
            text: root.clockService ? root.clockService.dateText : ""
            color: theme.shellTextSecondary
            font.pixelSize: 10
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
            font.weight: Font.Medium
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
        NumberAnimation { target: time; property: "opacity"; to: 0.25; duration: 75 }
        NumberAnimation { target: time; property: "opacity"; to: 1; duration: 125 }
    }

    Connections {
        target: root.clockService
        function onChanged() { minuteTransition.restart() }
    }
}
