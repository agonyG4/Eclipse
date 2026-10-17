import QtQuick
import QtQuick.Effects

Item {
    id: root

    objectName: "screenshotVisual"
    property url source
    property string imageObjectName: "screenshotImage"
    property real imageWidth: 240
    property real imageHeight: 160
    property real cornerRadius: 10
    property bool borderEnabled: true
    property real borderWidth: 1
    property color borderColor: "#b3ffffff"
    property bool shadowEnabled: true
    property real shadowStrength: 0.55

    readonly property real effectiveShadowStrength: Math.max(0, Math.min(1, shadowStrength))
    readonly property real shadowPadding: shadowEnabled
                                         ? Math.ceil(8 + 12 * effectiveShadowStrength) : 0
    readonly property real effectiveCornerRadius: Math.max(0, Math.min(
        cornerRadius, imageWidth / 2, imageHeight / 2))
    readonly property real effectiveBorderWidth: borderEnabled
                                                 ? Math.max(0, Math.min(
                                                     borderWidth, imageWidth / 2,
                                                     imageHeight / 2)) : 0

    implicitWidth: imageWidth + 2 * shadowPadding
    implicitHeight: imageHeight + 2 * shadowPadding
    width: implicitWidth
    height: implicitHeight
    clip: false

    Item {
        id: shadowLayer
        objectName: "screenshotShadowLayer"
        anchors.fill: parent
        visible: root.shadowEnabled
        layer.enabled: visible
        layer.smooth: true
        layer.effect: MultiEffect {
            objectName: "screenshotShadowEffect"
            shadowEnabled: true
            shadowColor: "#e0000000"
            shadowBlur: 0.18 + 0.58 * root.effectiveShadowStrength
            shadowOpacity: 0.72 * root.effectiveShadowStrength
            shadowVerticalOffset: 3 + 7 * root.effectiveShadowStrength
            shadowScale: 1.0
        }

        Rectangle {
            objectName: "screenshotShadowShape"
            x: root.shadowPadding
            y: root.shadowPadding
            width: root.imageWidth
            height: root.imageHeight
            radius: root.effectiveCornerRadius
            color: "white"
            antialiasing: true
        }
    }

    Item {
        id: imageFrame
        objectName: "screenshotImageFrame"
        x: root.shadowPadding
        y: root.shadowPadding
        width: root.imageWidth
        height: root.imageHeight
        clip: false

        Item {
            id: imageMask
            objectName: "screenshotMaskSource"
            anchors.fill: parent
            visible: false
            layer.enabled: true
            layer.smooth: true

            Rectangle {
                objectName: "screenshotMaskShape"
                anchors.fill: parent
                radius: root.effectiveCornerRadius
                color: "white"
                antialiasing: true
            }
        }

        Image {
            id: screenshotImage
            objectName: root.imageObjectName
            anchors.fill: parent
            source: root.source
            fillMode: Image.PreserveAspectFit
            asynchronous: false
            cache: false
            smooth: true
            mipmap: true
            layer.enabled: true
            layer.smooth: true
            layer.mipmap: true
            layer.effect: MultiEffect {
                objectName: "screenshotMaskEffect"
                maskEnabled: true
                maskSource: imageMask
                maskThresholdMin: 0.4
                maskSpreadAtMin: 0.6
            }
        }

        Rectangle {
            id: screenshotBorder
            objectName: "screenshotBorder"
            anchors.fill: parent
            visible: root.borderEnabled && root.effectiveBorderWidth > 0
            color: "transparent"
            radius: root.effectiveCornerRadius
            border.width: root.effectiveBorderWidth
            border.color: root.borderColor
            antialiasing: true
        }
    }
}
