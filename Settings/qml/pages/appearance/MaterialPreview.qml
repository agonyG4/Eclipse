import QtQuick
import QtQuick.Window
import Astrea.Effects
import "../../components" as Components

Item {
    id: root
    objectName: "materialPreview"

    property url wallpaperSource
    property string wallpaperFit: "cover"
    property string themeVariant: "dark"
    property string materialId: "default"

    // Reserved for a future renderer-owned preview frame. No transport is
    // defined here; the local presentation below is intentionally temporary.
    property url rendererPreviewSource
    property bool rendererPreviewReady: false
    property bool hasLoadedWallpaper: false
    readonly property bool rendererPreviewRequested: rendererPreviewReady
        && rendererPreviewSource.toString().length > 0
    readonly property bool usingRendererPreview: rendererPreviewRequested
        && rendererFrame.status === Image.Ready
    readonly property bool rendererPreviewFailed: rendererPreviewRequested
        && rendererFrame.status === Image.Error
    readonly property bool wallpaperReady: hasLoadedWallpaper
        && (wallpaperImage.status === Image.Ready || wallpaperImage.status === Image.Loading)
    readonly property bool liveFrostedAvailable: liveFrosted.effectAvailable
    readonly property bool liveFrostedActive: liveFrosted.effectActive

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
        visible: !root.usingRendererPreview && !root.wallpaperReady
    }

    Image {
        id: wallpaperImage
        objectName: "materialPreviewWallpaper"
        anchors.fill: parent
        source: root.usingRendererPreview ? "" : root.wallpaperSource
        fillMode: root.fillModeFor(root.wallpaperFit)
        asynchronous: true
        cache: true
        retainWhileLoading: true
        smooth: true
        mipmap: true
        sourceSize.width: Math.max(1, Math.ceil(root.width * Screen.devicePixelRatio))
        sourceSize.height: Math.max(1, Math.ceil(root.height * Screen.devicePixelRatio))
        visible: !root.usingRendererPreview && root.wallpaperReady
            && (status === Image.Ready || status === Image.Loading)

        onStatusChanged: {
            if (status === Image.Ready)
                root.hasLoadedWallpaper = true
            else if (status === Image.Null && source.toString().length === 0)
                root.hasLoadedWallpaper = false
        }

        onSourceChanged: {
            if (source.toString().length === 0)
                root.hasLoadedWallpaper = false
        }
    }

    Image {
        id: rendererFrame
        objectName: "materialPreviewRendererFrame"
        anchors.fill: parent
        source: root.rendererPreviewRequested ? root.rendererPreviewSource : ""
        fillMode: Image.Stretch
        asynchronous: true
        cache: true
        retainWhileLoading: true
        smooth: true
        mipmap: true
        visible: root.usingRendererPreview
    }

    BackdropEffectSurface {
        id: liveFrosted
        objectName: "materialPreviewLiveFrosted"
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        visible: root.materialId === "frosted" && !root.usingRendererPreview
        effectEnabled: visible
        cornerRadius: 8

        content: Component {
            MaterialShowcase {
                anchors.fill: parent
                canonicalIdentity: liveFrosted.effectActive
                themeVariant: root.themeVariant
                materialId: root.materialId
            }
        }
    }

    // Fallback remains the canonical preview when the compositor or Qt
    // Wayland path cannot provide a live child surface.
    MaterialShowcase {
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        canonicalIdentity: !liveFrosted.effectActive
        themeVariant: root.themeVariant
        materialId: root.materialId
        visible: !root.usingRendererPreview && !liveFrosted.effectActive
    }
}
