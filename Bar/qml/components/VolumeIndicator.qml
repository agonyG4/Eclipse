import QtQuick

TopbarIndicator {
    id: root

    ShellBarTheme { id: theme }

    property var audioService: null
    mouseAreaObjectName: "volumeWheelArea"
    objectName: "volumeIndicator"
    fixedWidth: 28
    height: 34

    signal volumeChanged(real value)

    onWheel: event => {
        if (!root.audioService)
            return
        root.audioService.adjustVolume(event.angleDelta.y > 0 ? 2 : -2)
        event.accepted = true
    }

    Text {
        id: icon
        objectName: "volumeIcon"
        text: !root.audioService || root.audioService.muted || root.audioService.volume === 0
            ? "󰝟"
            : root.audioService.volume < 34 ? "󰕿"
            : root.audioService.volume < 67 ? "󰖀" : "󰕾"
        color: !root.audioService || root.audioService.available === false
            ? theme.shellIconMuted
            : root.audioService.muted ? theme.shellIconMuted : theme.shellIconMain
        font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
        Behavior on color { ColorAnimation { duration: theme.animationFast } }
    }
}
