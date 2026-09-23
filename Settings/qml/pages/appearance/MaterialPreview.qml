import QtQuick
import QtQuick.Window
import Astrea.Effects
import "../../components" as Components

Item {
    id: root
    objectName: "materialPreview"

    property var controller
    property url wallpaperSource
    property string wallpaperFit: "cover"
    property string themeVariant: "dark"
    property bool liveEffectEnabled: true
    property bool hasLoadedWallpaper: false

    readonly property real materialPosition: controller ? controller.materialPosition : 0.5
    readonly property real effectiveBlur: controller ? controller.effectiveBlur : 0.0
    readonly property real effectiveSaturation: controller ? controller.effectiveSaturation : 1.0
    readonly property real effectiveNoise: controller ? controller.effectiveNoise : 0.0
    readonly property bool wallpaperReady: hasLoadedWallpaper
        && (wallpaperImage.status === Image.Ready || wallpaperImage.status === Image.Loading)
    readonly property bool liveMaterialAvailable: liveMaterial.effectAvailable
    readonly property bool liveMaterialActive: liveMaterial.effectActive
    readonly property bool usingFallback: !liveMaterialActive

    function fillModeFor(fit) {
        switch (fit) {
        case "contain": return Image.PreserveAspectFit
        case "stretch": return Image.Stretch
        case "center": return Image.Pad
        case "tile": return Image.Tile
        default: return Image.PreserveAspectCrop
        }
    }

    Rectangle {
        id: materialPreviewFallback
        objectName: "materialPreviewFallback"
        anchors.fill: parent
        radius: 9
        color: Components.Theme.isLight ? "#e3eaf3" : "#202735"
        visible: !root.wallpaperReady
    }

    Image {
        id: wallpaperImage
        objectName: "materialPreviewWallpaper"
        anchors.fill: parent
        source: root.wallpaperSource
        fillMode: root.fillModeFor(root.wallpaperFit)
        asynchronous: true
        cache: true
        retainWhileLoading: true
        smooth: true
        mipmap: true
        sourceSize.width: Math.max(1, Math.ceil(root.width * Screen.devicePixelRatio))
        sourceSize.height: Math.max(1, Math.ceil(root.height * Screen.devicePixelRatio))
        visible: root.wallpaperReady
            && (status === Image.Ready || status === Image.Loading)

        onStatusChanged: {
            if (status === Image.Ready)
                root.hasLoadedWallpaper = true
            else if (status === Image.Null && source.toString().length === 0)
                root.hasLoadedWallpaper = false
        }
        onSourceChanged: if (source.toString().length === 0) root.hasLoadedWallpaper = false
    }

    // This sends only the surface's semantic blur request.
    BackdropEffectSurface {
        id: liveMaterial
        objectName: "materialPreviewLiveEffect"
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        visible: root.visible && root.liveEffectEnabled
        effectEnabled: root.visible && root.liveEffectEnabled && effectAvailable
        cornerRadius: 8

        content: Component {
            MaterialShowcase {
                anchors.fill: parent
                fallbackApproximation: false
                themeVariant: root.themeVariant
                effectiveBlur: root.effectiveBlur
                effectiveSaturation: root.effectiveSaturation
                effectiveNoise: root.effectiveNoise
            }
        }
    }

    // Local geometry is explicitly an approximation, used when a semantic
    // compositor surface cannot be created or activated.
    MaterialShowcase {
        objectName: "materialPreviewFallbackShowcase"
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        fallbackApproximation: true
        themeVariant: root.themeVariant
        effectiveBlur: root.effectiveBlur
        effectiveSaturation: root.effectiveSaturation
        effectiveNoise: root.effectiveNoise
        visible: root.usingFallback
    }
}
