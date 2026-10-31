import QtQuick
import ".." as Components

// macOS press feedback: darken the instant a control is pressed and ease
// back on release. Fill the control and match its corner radius.
Rectangle {
    id: tint

    property bool pressed: false
    // Accent-filled controls need a deeper shade to read as pressed.
    property bool onAccent: false

    color: onAccent ? Qt.rgba(0, 0, 0, 0.18)
        : Components.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.08)
        : Qt.rgba(1, 1, 1, 0.08)
    opacity: 0

    onPressedChanged: {
        if (pressed) {
            release.stop()
            opacity = 1
        } else {
            release.restart()
        }
    }

    NumberAnimation {
        id: release
        target: tint
        property: "opacity"
        to: 0
        duration: Components.Theme.animationNormal
        easing.type: Components.Theme.easingStandard
    }
}
