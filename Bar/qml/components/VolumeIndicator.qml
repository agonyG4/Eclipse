import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var audioService: null
    objectName: "volumeIndicator"
    implicitWidth: 22
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight

    Text {
        id: icon
        objectName: "volumeIcon"
        anchors.centerIn: parent
        text: !root.audioService || root.audioService.muted
            ? "\uf6a9"
            : root.audioService.volume < 34 ? "\uf026"
            : root.audioService.volume < 67 ? "\uf027" : "\uf028"
        color: !root.audioService || root.audioService.available === false
            ? theme.shellIconMuted
            : root.audioService.muted ? theme.shellIconWarning : theme.shellIconMain
        font.family: "Symbols Nerd Font"
        font.pixelSize: 15
    }

    MouseArea {
        id: wheelArea
        objectName: "volumeWheelArea"
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: function(wheel) {
            if (!root.audioService)
                return
            root.audioService.adjustVolume(wheel.angleDelta.y > 0 ? 2 : -2)
            wheel.accepted = true
        }
    }
}
