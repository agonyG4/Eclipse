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

    // The three settled glyphs use pixel-aligned optical offsets measured
    // from the local SF Symbols artwork. Animate the entire speaker-and-waves
    // group only when the icon state changes; no scale or intra-band movement.
    // Smooth filtering preserves the original anti-aliased 16px SVG edges.
    readonly property real voicedOpticalScale: 1
    readonly property real voicedOpticalOffsetX: waveCount <= 1 ? 3
        : waveCount === 2 ? 1 : 0
    readonly property real mutedOpticalOffsetX: 2

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
        Behavior on x { NumberAnimation { duration: 165; easing.type: Easing.OutCubic } }
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
            opacity: root.nearProgress
        }

        AppleIcon {
            id: middleWave
            objectName: "volumeMiddleWave"
            anchors.fill: parent
            iconKey: "volume-wave-middle"
            foreground: root.foreground
            opacity: root.middleProgress
        }

        AppleIcon {
            id: farWave
            objectName: "volumeFarWave"
            anchors.fill: parent
            iconKey: "volume-wave-far"
            foreground: root.foreground
            opacity: root.farProgress
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
