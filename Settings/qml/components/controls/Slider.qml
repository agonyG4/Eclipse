import QtQuick
import QtQuick.Controls as QQC2
import "../.." as Components

QQC2.Slider {
    id: root

    implicitWidth: 216
    implicitHeight: 32

    // The presentation can hide the endpoint glyphs without changing the
    // inherited Slider interaction contract.
    property bool showEndpointGlyphs: true

    readonly property real trackHeight: 4
    readonly property real thumbWidth: 28
    readonly property real thumbHeight: 18
    readonly property real endpointSize: 18
    readonly property real endpointGap: 9
    readonly property real trackStart: showEndpointGlyphs
        ? endpointSize + endpointGap
        : thumbWidth / 2
    readonly property real trackEnd: trackStart
    readonly property real trackWidth: Math.max(0, width - trackStart - trackEnd)
    readonly property color neutralTrackColor: Qt.rgba(
        Components.Theme.textTertiary.r,
        Components.Theme.textTertiary.g,
        Components.Theme.textTertiary.b,
        Components.Theme.isLight ? 0.28 : 0.36)
    readonly property color thumbSurface: Components.Theme.isLight
        ? Qt.rgba(1, 1, 1, 0.98)
        : Components.Theme.cardBg
    readonly property color thumbBorder: Qt.rgba(
        Components.Theme.textPrimary.r,
        Components.Theme.textPrimary.g,
        Components.Theme.textPrimary.b,
        Components.Theme.isLight ? 0.18 : 0.28)
    readonly property color thumbShadow: Qt.rgba(0, 0, 0, Components.Theme.isLight ? 0.18 : 0.46)
    readonly property color endpointColor: Components.Theme.isLight
        ? Components.Theme.textSecondary
        : Components.Theme.textTertiary

    background: Item {
        implicitWidth: root.implicitWidth
        implicitHeight: root.trackHeight

        Item {
            visible: root.showEndpointGlyphs
            width: root.endpointSize
            height: root.endpointSize
            x: 0
            y: (root.height - height) / 2
            opacity: root.enabled ? 1 : Components.Theme.opacityDisabled

            Rectangle {
                width: 8
                height: 12
                radius: 3
                anchors.centerIn: parent
                color: root.endpointColor
                opacity: 0.8
            }
            Rectangle {
                width: 4
                height: 8
                radius: 2
                anchors.centerIn: parent
                color: Components.Theme.cardBg
                opacity: 0.45
            }
        }

        Item {
            visible: root.showEndpointGlyphs
            width: root.endpointSize
            height: root.endpointSize
            x: root.width - width
            y: (root.height - height) / 2
            opacity: root.enabled ? 1 : Components.Theme.opacityDisabled

            Rectangle {
                width: 14
                height: 12
                radius: 3
                anchors.centerIn: parent
                color: root.endpointColor
                opacity: 0.8
            }
            Rectangle {
                width: 10
                height: 8
                radius: 2
                anchors.centerIn: parent
                color: Components.Theme.cardBg
                opacity: 0.45
            }
        }

        Rectangle {
            x: root.trackStart
            y: (root.height - height) / 2
            width: root.trackWidth
            height: root.trackHeight
            radius: height / 2
            color: root.neutralTrackColor
            opacity: root.enabled ? 1 : Components.Theme.opacityMuted

            Rectangle {
                width: root.visualPosition * parent.width
                height: parent.height
                radius: height / 2
                color: Components.Theme.accent
            }
        }
    }

    handle: Item {
        width: root.thumbWidth
        height: root.thumbHeight
        x: root.trackStart + root.visualPosition * root.trackWidth - width / 2
        y: (root.height - height) / 2
        scale: root.pressed ? 1.04 : (root.hovered ? 1.02 : 1.0)
        opacity: root.enabled ? 1 : Components.Theme.opacityMuted

        Rectangle {
            x: 0
            y: 2
            width: parent.width
            height: parent.height
            radius: height / 2
            color: root.thumbShadow
        }

        Rectangle {
            id: handleBody
            anchors.fill: parent
            radius: height / 2
            color: root.thumbSurface
            border.width: 1
            border.color: root.thumbBorder
        }

        Behavior on scale {
            NumberAnimation {
                duration: Components.Theme.animationMicro
                easing.type: Easing.OutCubic
            }
        }
        Behavior on opacity {
            NumberAnimation {
                duration: Components.Theme.animationMicro
            }
        }
    }
}
