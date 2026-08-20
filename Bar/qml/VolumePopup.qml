import QtQuick
import "components"

PopupCard {
    id: root

    ShellBarTheme { id: theme }

    property var audioService: null
    readonly property real volume: root.audioService ? root.audioService.volume : 0
    readonly property bool muted: Boolean(root.audioService && root.audioService.muted)

    objectName: "volumePopupCard"
    implicitWidth: 300
    cardPadding: 18
    contentSpacing: 14

    function setVolumeFromPosition(position, width) {
        if (!root.audioService || width <= 0)
            return
        const next = Math.round(Math.max(0, Math.min(width, position)) / width * 100)
        const delta = next - Math.round(root.volume)
        if (delta !== 0)
            root.audioService.adjustVolume(delta)
    }

    PopupHeader {
        title: "Volume"
        trailingWidth: 28
        trailingHeight: 28

        Rectangle {
            id: muteButton
            anchors.centerIn: parent
            width: 28
            height: 28
            radius: theme.shellRadiusLarge
            color: root.muted ? Qt.rgba(1, 0.23, 0.19, 0.25)
                             : muteMouse.containsMouse ? theme.shellSeparator : Qt.rgba(1, 1, 1, 0.07)
            border.width: 1
            border.color: root.muted ? Qt.rgba(1, 0.23, 0.19, 0.40)
                                     : Qt.rgba(1, 1, 1, 0.08)

            Text {
                anchors.centerIn: parent
                text: root.muted || root.volume === 0 ? "󰝟"
                    : root.volume < 34 ? "󰕿"
                    : root.volume < 67 ? "󰖀" : "󰕾"
                color: root.muted ? theme.shellIconWarning : theme.shellIconMain
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeBody }
            }

            MouseArea {
                id: muteMouse
                anchors.fill: parent
                enabled: Boolean(root.audioService)
                hoverEnabled: enabled
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.audioService.setMuted(!root.muted)
            }
        }
    }

    Row {
        width: parent.width
        spacing: theme.spacingMedium

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "󰕿"
            color: theme.shellIconMuted
            font { family: theme.iconFontFamily; pixelSize: theme.fontSizeTitle }
        }

        Item {
            id: sliderArea
            objectName: "volumeSlider"
            width: parent.width - 14 - 14 - theme.spacingMedium
            height: 30
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                id: track
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 5
                radius: 3
                color: theme.shellSeparator

                Rectangle {
                    width: Math.max(radius * 2, track.width * Math.min(1, root.volume / 100))
                    height: parent.height
                    radius: parent.radius
                    color: root.muted ? theme.shellIconMuted : theme.shellTextActive
                    opacity: root.muted ? 0.65 : 0.75
                    Behavior on width {
                        NumberAnimation { duration: theme.animationSlider; easing.type: Easing.OutCubic }
                    }
                }
            }

            Rectangle {
                anchors.verticalCenter: track.verticalCenter
                x: Math.max(0, Math.min(track.width - width,
                    track.width * Math.min(1, root.volume / 100) - width / 2))
                width: sliderMouse.pressed ? 22 : sliderMouse.containsMouse ? 20 : 16
                height: width
                radius: width / 2
                color: theme.shellTextActive

                Behavior on width {
                    NumberAnimation { duration: theme.animationMicro; easing.type: Easing.OutCubic }
                }
            }

            MouseArea {
                id: sliderMouse
                anchors.fill: parent
                anchors.topMargin: -10
                anchors.bottomMargin: -10
                enabled: Boolean(root.audioService)
                hoverEnabled: enabled
                preventStealing: true
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onPressed: event => root.setVolumeFromPosition(event.x, track.width)
                onPositionChanged: event => {
                    if (pressed)
                        root.setVolumeFromPosition(event.x, track.width)
                }
                onWheel: event => {
                    if (!root.audioService)
                        return
                    root.audioService.adjustVolume(event.angleDelta.y > 0 ? 2 : -2)
                    event.accepted = true
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "󰕾"
            color: theme.shellIconMuted
            font { family: theme.iconFontFamily; pixelSize: theme.fontSizeTitle }
        }
    }
}
