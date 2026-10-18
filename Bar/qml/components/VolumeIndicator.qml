import QtQuick

TopbarIndicator {
    id: root

    ShellBarTheme { id: theme }

    property var audioService: null
    mouseAreaObjectName: "volumeWheelArea"
    objectName: "volumeIndicator"
    fixedWidth: 28
    height: 34

    readonly property bool defaultStateAvailable: Boolean(root.audioService
        && root.audioService.defaultStateAvailable)

    signal volumeChanged(real value)

    onWheel: event => {
        if (!root.audioService || !root.defaultStateAvailable)
            return
        root.audioService.adjustVolume(event.angleDelta.y > 0 ? 2 : -2)
        event.accepted = true
    }

    Text {
        id: icon
        objectName: "volumeIcon"
        visible: !appleIcon.hasIcon
        text: !root.defaultStateAvailable || root.audioService.muted
            || Math.round(Math.max(0, root.audioService.volume)) === 0
            ? "󰝟"
            : root.audioService.volume < 34 ? "󰕿"
            : root.audioService.volume < 67 ? "󰖀" : "󰕾"
        color: !root.defaultStateAvailable || !root.audioService
            || root.audioService.available === false
            ? theme.shellIconMuted
            : root.audioService.muted ? theme.shellIconMuted : theme.shellIconMain
        font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
        Behavior on color { ColorAnimation { duration: theme.animationFast } }
    }

    VolumeStateIcon {
        id: appleIcon
        objectName: "volumeAppleIcon"
        width: 16
        height: 16
        volume: root.audioService ? root.audioService.volume : 0
        muted: root.audioService ? root.audioService.muted : false
        stateAvailable: root.defaultStateAvailable
        foreground: !root.defaultStateAvailable || !root.audioService
            || root.audioService.available === false
            ? theme.shellIconMuted
            : theme.isLight ? theme.shellTextActive : "#ffffff"
        mutedForeground: root.defaultStateAvailable
            ? (theme.isLight ? theme.shellTextActive : "#ffffff")
            : theme.shellIconMuted
    }
}
