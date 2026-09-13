import QtQuick
import QtQuick.Controls as QQC2
import "../.." as Components

QQC2.Slider {
    id: root

    implicitWidth: 216
    implicitHeight: 32
    clip: false

    // The presentation can hide the endpoint glyphs without changing the
    // inherited Slider interaction contract.
    property bool showEndpointGlyphs: true
    property bool detentEnabled: false
    property real detentValue: 0
    property real detentSnapDistancePx: 8
    property real detentReleaseDistancePx: 14
    property string valueText: ""
    property bool modelValueEnabled: false
    property real modelValue: value

    signal valueEdited(real value)

    property bool detentLatched: false
    property real pulseScale: 1.0
    property int pulseSerial: 0

    leftPadding: showEndpointGlyphs
        ? endpointSize + endpointGap - thumbWidth / 2
        : 0
    rightPadding: leftPadding

    readonly property real trackHeight: 4
    readonly property real thumbWidth: 28
    readonly property real thumbHeight: 18
    readonly property real endpointSize: 18
    readonly property real endpointGap: 13
    readonly property real handleTravel: Math.max(0, availableWidth - thumbWidth)
    readonly property real trackStart: leftPadding + thumbWidth / 2
    readonly property real trackEnd: trackStart + handleTravel
    readonly property real trackWidth: Math.max(0, trackEnd - trackStart)
    readonly property real detentPosition: to === from
        ? 0
        : Math.max(0, Math.min(1, (detentValue - from) / (to - from)))
    readonly property real detentVisualPosition: mirrored ? 1 - detentPosition : detentPosition
    readonly property real effectiveVisualPosition: detentLatched
        ? detentVisualPosition
        : visualPosition
    readonly property real displayedValue: detentLatched ? detentValue : value
    readonly property color neutralTrackColor: Qt.rgba(
        Components.Theme.textTertiary.r,
        Components.Theme.textTertiary.g,
        Components.Theme.textTertiary.b,
        Components.Theme.isLight ? 0.28 : 0.36)
    readonly property color thumbSurface: Components.Theme.isLight
        ? Qt.rgba(1, 1, 1, 0.98)
        : Qt.rgba(
            Components.Theme.textPrimary.r,
            Components.Theme.textPrimary.g,
            Components.Theme.textPrimary.b,
            0.98)
    readonly property color thumbBorder: Qt.rgba(
        Components.Theme.textPrimary.r,
        Components.Theme.textPrimary.g,
        Components.Theme.textPrimary.b,
        Components.Theme.isLight ? 0.18 : 0.28)
    readonly property color thumbShadow: Qt.rgba(0, 0, 0, Components.Theme.isLight ? 0.18 : 0.46)
    readonly property color endpointColor: Components.Theme.isLight
        ? Components.Theme.textSecondary
        : Components.Theme.textTertiary

    property bool tooltipVisible: false
    property bool keyboardEditPending: false

    function syncNativeValueFromModel() {
        if (!root.modelValueEnabled || root.pressed || root.keyboardEditPending)
            return

        if (Math.abs(root.value - root.modelValue) <= 0.000001)
            return

        root.value = root.modelValue
    }

    function handleNativeMove() {
        if (!root.detentEnabled || !root.pressed || root.keyboardEditPending) {
            root.detentLatched = false
            root.valueEdited(root.value)
            return
        }

        const distance = Math.abs(root.visualPosition - root.detentVisualPosition)
            * root.handleTravel
        const threshold = root.detentLatched
            ? root.detentReleaseDistancePx
            : root.detentSnapDistancePx
        if (distance <= threshold) {
            if (!root.detentLatched) {
                root.detentLatched = true
                root.pulseSerial += 1
            }
            root.valueEdited(root.detentValue)
            return
        }

        root.detentLatched = false
        root.valueEdited(root.value)
    }

    onMoved: root.handleNativeMove()
    onModelValueChanged: root.syncNativeValueFromModel()
    onModelValueEnabledChanged: {
        if (modelValueEnabled)
            root.syncNativeValueFromModel()
        else
            detentLatched = false
    }
    onDetentEnabledChanged: if (!detentEnabled) detentLatched = false
    onPressedChanged: {
        if (pressed) {
            hoverTooltipTimer.stop()
            tooltipVisible = valueText !== ""
        } else if (hovered) {
            hoverTooltipTimer.restart()
        } else {
            tooltipVisible = false
        }
        if (!pressed) {
            detentLatched = false
            root.syncNativeValueFromModel()
        }
    }
    onHoveredChanged: {
        if (!hovered) {
            hoverTooltipTimer.stop()
            tooltipVisible = false
        } else if (!pressed) {
            hoverTooltipTimer.restart()
        }
    }
    onEnabledChanged: {
        if (!enabled) {
            hoverTooltipTimer.stop()
            tooltipVisible = false
            detentLatched = false
        }
    }
    onValueTextChanged: if (valueText === "") tooltipVisible = false

    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function(event) {
        const keyboardKey = event.key === Qt.Key_Left || event.key === Qt.Key_Right
            || event.key === Qt.Key_Up || event.key === Qt.Key_Down
            || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
            || event.key === Qt.Key_Home || event.key === Qt.Key_End
        if (keyboardKey) {
            root.keyboardEditPending = true
            keyboardEditReset.restart()
        }
        event.accepted = false
    }

    Timer {
        id: keyboardEditReset
        interval: 0
        repeat: false
        onTriggered: {
            root.keyboardEditPending = false
            root.syncNativeValueFromModel()
        }
    }

    Timer {
        id: hoverTooltipTimer
        interval: 350
        repeat: false
        onTriggered: if (root.enabled && root.hovered && !root.pressed && root.valueText !== "")
                         root.tooltipVisible = true
    }

    SequentialAnimation {
        id: pulseAnimation
        NumberAnimation {
            target: root
            property: "pulseScale"
            to: 1.06
            duration: 50
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "pulseScale"
            to: 1.0
            duration: 70
            easing.type: Easing.InOutCubic
        }
    }

    onPulseSerialChanged: pulseAnimation.restart()

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
                width: root.effectiveVisualPosition * parent.width
                height: parent.height
                radius: height / 2
                color: Components.Theme.accent
            }

            Rectangle {
                id: detentMarker
                objectName: "sliderDefaultDetentMarker"
                visible: root.detentEnabled
                x: Math.max(0, Math.min(parent.width - width,
                                         root.detentVisualPosition * parent.width - width / 2))
                y: -2
                width: 2.5
                height: parent.height + 4
                radius: width / 2
                color: root.detentMarkerColor
                opacity: root.enabled
                    ? (root.detentLatched ? 0.88 : 0.42)
                    : Components.Theme.opacityMuted

                Behavior on opacity {
                    NumberAnimation {
                        duration: Components.Theme.animationMicro
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }

    readonly property color detentMarkerColor: Components.Theme.isLight
        ? Components.Theme.textSecondary
        : Components.Theme.textPrimary

    handle: Item {
        width: root.thumbWidth
        height: root.thumbHeight
        x: root.leftPadding + root.effectiveVisualPosition * (root.availableWidth - width)
        y: (root.height - height) / 2
        scale: Math.max(root.pulseScale, root.pressed ? 1.04 : (root.hovered ? 1.02 : 1.0))
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

    Item {
        id: tooltipLayer
        visible: root.enabled && root.tooltipVisible && root.valueText !== ""
        enabled: false
        z: 10
        width: tooltipSurface.width
        height: tooltipSurface.height
        x: root.handle.x + root.handle.width / 2 - width / 2
        y: root.handle.y - height - 6
        opacity: visible ? 1 : 0

        Rectangle {
            id: tooltipSurface
            width: tooltipLabel.implicitWidth + 16
            height: tooltipLabel.implicitHeight + 8
            radius: Components.Theme.controlRadius
            color: Components.Theme.popupBg
            border.width: 1
            border.color: Components.Theme.cardBorder

            Text {
                id: tooltipLabel
                anchors.centerIn: parent
                text: root.valueText
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                font.weight: Components.Theme.fontWeightMedium
            }
        }
    }
}
