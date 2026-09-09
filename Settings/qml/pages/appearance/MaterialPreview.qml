import QtQuick
import QtQuick.Window
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
    readonly property real materialOpacity: materialId === "transparent" ? 0.58
        : materialId === "frosted" ? 0.80 : 0.96
    readonly property color lightSurface: "#fbfcff"
    readonly property color lightText: "#4c5665"
    readonly property color darkSurface: "#272e3a"
    readonly property color darkText: "#d8deea"

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

    // Transitional fallback only: a renderer will eventually own canonical
    // Transparent/Frosted material evaluation for a specific wallpaper input.
    Rectangle {
        id: showcase
        objectName: "materialPreviewShowcase"
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        radius: 8
        clip: true
        visible: !root.usingRendererPreview
        color: root.themeVariant === "auto"
            ? "transparent"
            : root.themeVariant === "light"
                ? root.lightSurface
                : root.themeVariant === "dark"
                    ? root.darkSurface
                    : Components.Theme.isLight ? root.lightSurface : root.darkSurface
        opacity: root.themeVariant === "auto" ? 0.96 : root.materialOpacity
        border.width: 1
        border.color: root.themeVariant === "auto"
            ? "#9aa8b9"
            : root.themeVariant === "light" ? "#d2d9e4" : "#3b4554"

        Rectangle {
            visible: root.themeVariant === "auto"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: parent.width / 2
            color: root.lightSurface
        }

        Rectangle {
            visible: root.themeVariant === "auto"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: parent.width / 2
            color: root.darkSurface
        }

        Row {
            visible: root.themeVariant !== "auto"
            x: 8
            y: 7
            spacing: 4

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 5
                    height: 5
                    radius: 3
                    color: root.themeVariant === "light" ? root.lightText
                        : root.themeVariant === "dark" ? root.darkText
                            : Components.Theme.isLight ? root.lightText : root.darkText
                    opacity: 0.72
                }
            }
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            x: 10
            y: 24
            width: parent.width * 0.43
            height: 5
            radius: 2
            color: root.themeVariant === "light" ? root.lightText
                : root.themeVariant === "dark" ? root.darkText
                    : Components.Theme.isLight ? root.lightText : root.darkText
            opacity: 0.52
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            x: 10
            y: 36
            width: parent.width * 0.66
            height: 4
            radius: 2
            color: root.themeVariant === "light" ? root.lightText
                : root.themeVariant === "dark" ? root.darkText
                    : Components.Theme.isLight ? root.lightText : root.darkText
            opacity: 0.28
        }

        Item {
            visible: root.themeVariant === "auto"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 2
            clip: true

            Row {
                x: 8
                y: 7
                spacing: 4

                Repeater {
                    model: 3
                    delegate: Rectangle {
                        width: 5
                        height: 5
                        radius: 3
                        color: root.lightText
                        opacity: 0.72
                    }
                }
            }

            Rectangle {
                x: 10
                y: 24
                width: parent.width * 0.72
                height: 5
                radius: 2
                color: root.lightText
                opacity: 0.52
            }

            Rectangle {
                x: 10
                y: 36
                width: parent.width * 0.88
                height: 4
                radius: 2
                color: root.lightText
                opacity: 0.28
            }
        }

        Item {
            visible: root.themeVariant === "auto"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 2
            clip: true

            Row {
                x: 8
                y: 7
                spacing: 4

                Repeater {
                    model: 3
                    delegate: Rectangle {
                        width: 5
                        height: 5
                        radius: 3
                        color: root.darkText
                        opacity: 0.72
                    }
                }
            }

            Rectangle {
                x: 10
                y: 24
                width: parent.width * 0.72
                height: 5
                radius: 2
                color: root.darkText
                opacity: 0.52
            }

            Rectangle {
                x: 10
                y: 36
                width: parent.width * 0.88
                height: 4
                radius: 2
                color: root.darkText
                opacity: 0.28
            }
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 12
            color: root.themeVariant === "light" ? "#d1dce9"
                : root.themeVariant === "dark" ? "#3a4657"
                    : Components.Theme.isLight ? "#d1dce9" : "#3a4657"
            opacity: root.materialId === "transparent" ? 0.42
                : root.materialId === "frosted" ? 0.65 : 0.90
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 10
            anchors.bottomMargin: 8
            width: 18
            height: 3
            radius: 2
            color: Components.Theme.accent
            opacity: 0.72
        }
    }
}
