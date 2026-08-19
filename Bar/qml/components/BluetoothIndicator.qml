import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var bluetoothService: null
    objectName: "bluetoothIndicator"
    implicitWidth: 22
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight

    Text {
        id: icon
        objectName: "bluetoothIcon"
        anchors.centerIn: parent
        text: "\uf293"
        color: !root.bluetoothService || !root.bluetoothService.available
            ? theme.shellIconMuted
            : root.bluetoothService.connectedCount > 0
                ? theme.shellIconAccent : theme.shellIconMain
        font.family: "Symbols Nerd Font"
        font.pixelSize: 15
    }

    SequentialAnimation {
        id: scanPulse
        running: Boolean(root.bluetoothService && root.bluetoothService.scanning)
        loops: Animation.Infinite
        NumberAnimation { target: icon; property: "opacity"; from: 1; to: 0.35; duration: 420 }
        NumberAnimation { target: icon; property: "opacity"; from: 0.35; to: 1; duration: 420 }
    }
}
