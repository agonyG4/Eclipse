import QtQuick

/**
 * WallpaperPreviewImage - The current wallpaper, decoded at a fixed preview
 * size. Every instance shares one cache key (source, size and fill), so a
 * hidden copy created at startup makes later previews appear at once.
 */
Image {
    id: root

    property var wallpaper: null
    readonly property bool hasUsableSource: Boolean(root.wallpaper)
        && Boolean(root.wallpaper.effectivePreviewUrl)
        && String(root.wallpaper.effectivePreviewUrl) !== ""

    source: root.hasUsableSource ? root.wallpaper.effectivePreviewUrl : ""
    fillMode: {
        if (!root.wallpaper || root.wallpaper.effectiveFit === "cover")
            return Image.PreserveAspectCrop
        if (root.wallpaper.effectiveFit === "contain")
            return Image.PreserveAspectFit
        if (root.wallpaper.effectiveFit === "center")
            return Image.Pad
        if (root.wallpaper.effectiveFit === "tile")
            return Image.Tile
        return Image.Stretch
    }
    asynchronous: true
    cache: true
    retainWhileLoading: true
    smooth: true
    mipmap: true
    // A fixed decode bound avoids reloading at every animation frame and keeps
    // the cache key identical across instances.
    sourceSize.width: 1024
    sourceSize.height: 1024
}
