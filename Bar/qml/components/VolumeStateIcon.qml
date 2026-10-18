import QtQuick

// Discrete volume icon states with animated transitions between symbols.
// Only 0%, 1-33%, 34-66%, and 67%+ cause a change in the artwork.
// The AppleOS SVG geometry is user-local and never bundled into the app.
Item {
    id: root

    property real volume: 0
    property bool muted: false
    property bool stateAvailable: true
    property color foreground: "#ffffff"
    property color mutedForeground: foreground

    readonly property bool silent: !stateAvailable || muted
        || !Number.isFinite(volume) || Math.round(Math.max(0, volume)) === 0
    readonly property int waveCount: silent ? 0
        : volume <= 33 ? 1 : volume <= 66 ? 2 : 3
    readonly property string iconKey: silent ? "volume-muted"
        : waveCount === 1 ? "volume-low" : "volume-high"
    readonly property bool appleTheme: typeof ThemeController !== "undefined"
        && ThemeController !== null && ThemeController.navigationIconTheme === "apple"
    readonly property bool layersReady: speaker.hasIcon && speaker.artworkReady
        && nearWave.hasIcon && nearWave.artworkReady
        && middleWave.hasIcon && middleWave.artworkReady
        && farWave.hasIcon && farWave.artworkReady
    readonly property bool hasIcon: appleTheme && muteIcon.hasIcon
        && muteIcon.artworkReady && layersReady
    readonly property url resolvedUrl: silent ? muteIcon.resolvedUrl : speaker.resolvedUrl

    // The target is an INTEGER state. Changing volume within a band must
    // not animate or move the glyph. Interpolation is used only when the
    // selected icon state changes, and can reverse during a transition.
    readonly property real targetWaveProgress: waveCount
    property real animatedWaveProgress: targetWaveProgress
    Behavior on animatedWaveProgress {
        NumberAnimation { duration: 165; easing.type: Easing.OutCubic }
    }

    function stage(value) {
        return Math.max(0, Math.min(1, value))
    }

    readonly property real nearProgress: stage(animatedWaveProgress)
    readonly property real middleProgress: stage(animatedWaveProgress - 1)
    readonly property real farProgress: stage(animatedWaveProgress - 2)

    // Optical bounds of the actual 16px local SF layers, compared against
    // the 30fps iPhone recording. Keep the silhouette centered at x=8
    // as the icon transitions from one discrete state to another.
    // The offset and the scale are calculated from the *same* interpolant:
    // no discontinuity as the second or third wave becomes visible.
    readonly property real voicedOpticalScale: 1 + 0.04 * middleProgress
        + 0.04 * farProgress
    // Calibrated against the alpha centroid of the actual 16px local SVGs.
    // The cubic correction compensates for the far wave's asymmetric pixel
    // coverage; all stages settle within roughly 0.1px of x=8.
    readonly property real opticalCentroidCorrectionX: 0.27 + 0.14 * nearProgress
        - 0.46 * middleProgress - 0.05 * farProgress
        - 0.65 * farProgress * farProgress * farProgress
    readonly property real voicedOpticalOffsetX:
        (3.5 - nearProgress - middleProgress - 1.5 * farProgress)
        * voicedOpticalScale + opticalCentroidCorrectionX
    readonly property real mutedOpticalOffsetX: 1.735

    width: 16
    height: 16
    visible: hasIcon

    Item {
        id: voicedGroup
        objectName: "volumeVoicedGroup"
        width: root.width
        height: root.height
        x: root.voicedOpticalOffsetX
        y: 0
        transformOrigin: Item.Center
        scale: root.voicedOpticalScale
        opacity: root.silent ? 0 : 1
        Behavior on opacity {
            NumberAnimation { duration: 110; easing.type: Easing.OutQuad }
        }

        AppleIcon {
            id: speaker
            objectName: "volumeSpeakerBase"
            anchors.fill: parent
            iconKey: "volume-speaker"
            foreground: root.foreground
        }

        AppleIcon {
            id: nearWave
            objectName: "volumeNearWave"
            anchors.fill: parent
            iconKey: "volume-wave-near"
            foreground: root.foreground
            transformOrigin: Item.Left
            opacity: root.nearProgress
            scale: 0.92 + 0.08 * root.nearProgress
        }

        AppleIcon {
            id: middleWave
            objectName: "volumeMiddleWave"
            anchors.fill: parent
            iconKey: "volume-wave-middle"
            foreground: root.foreground
            transformOrigin: Item.Left
            opacity: root.middleProgress
            scale: 0.92 + 0.08 * root.middleProgress
        }

        AppleIcon {
            id: farWave
            objectName: "volumeFarWave"
            anchors.fill: parent
            iconKey: "volume-wave-far"
            foreground: root.foreground
            transformOrigin: Item.Left
            opacity: root.farProgress
            scale: 0.92 + 0.08 * root.farProgress
        }
    }

    AppleIcon {
        id: muteIcon
        objectName: "volumeMuteSlash"
        width: root.width
        height: root.height
        x: root.mutedOpticalOffsetX
        y: 0
        iconKey: "volume-muted"
        foreground: root.mutedForeground
        opacity: root.silent ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: 110; easing.type: Easing.OutQuad }
        }
    }
}
