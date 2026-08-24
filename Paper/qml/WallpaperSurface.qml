import QtQuick
import QtQuick.Window

Window {
    id: root

    color: "#10131a"
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput

    property string wallpaperSource: ""
    property string wallpaperFit: "cover"
    property string pendingSource: ""
    property int pendingGeneration: -1
    property int activeSlot: 0
    property int outputWidth: 1
    property int outputHeight: 1
    property int wallpaperGeneration: 0
    property bool debugEnabled: false
    property int loadStartCount: 0
    property int loadReadyCount: 0
    property int loadErrorCount: 0
    property int promotionCount: 0
    property int visibleGeneration: -1
    readonly property int frontSlotGeneration: front.requestedGeneration
    readonly property int backSlotGeneration: back.requestedGeneration

    function fillModeFor(fit) {
        switch (fit) {
        case "contain": return Image.PreserveAspectFit
        case "stretch": return Image.Stretch
        case "center": return Image.Pad
        case "tile": return Image.Tile
        default: return Image.PreserveAspectCrop
        }
    }

    function rendererSource(source, generation) {
        if (source.length === 0)
            return ""
        var separator = source.indexOf("?") >= 0 ? "&" : "?"
        return source + separator + "astreaGeneration=" + generation
    }

    function beginLoad() {
        if (wallpaperSource.length === 0)
            return
        pendingSource = wallpaperSource
        pendingGeneration = wallpaperGeneration
        var slot = activeSlot === 0 ? back : front
        if (slot.status === Image.Loading)
            return
        slot.requestedSource = wallpaperSource
        slot.requestedGeneration = wallpaperGeneration
        slot.source = root.rendererSource(wallpaperSource, wallpaperGeneration)
        loadStartCount += 1
    }

    function completeLoad(slot, slotIndex) {
        loadReadyCount += 1
        if (slot.requestedSource !== wallpaperSource
                || slot.requestedGeneration !== wallpaperGeneration) {
            beginLoad()
            return
        }
        activeSlot = slotIndex
        visibleGeneration = slot.requestedGeneration
        promotionCount += 1
        pendingSource = ""
        pendingGeneration = -1
    }

    function failedLoad(slot) {
        loadErrorCount += 1
        if (slot.requestedSource !== wallpaperSource
                || slot.requestedGeneration !== wallpaperGeneration)
            beginLoad()
    }

    onWallpaperSourceChanged: beginLoad()
    onWallpaperGenerationChanged: beginLoad()

    Image {
        id: front
        objectName: "front"
        anchors.fill: parent
        asynchronous: true
        cache: true
        property string requestedSource: ""
        property int requestedGeneration: -1
        fillMode: root.fillModeFor(root.wallpaperFit)
        visible: root.activeSlot === 0

        onStatusChanged: {
            if (status === Image.Ready)
                root.completeLoad(front, 0)
            else if (status === Image.Error)
                root.failedLoad(front)
        }
    }

    Image {
        id: back
        objectName: "back"
        anchors.fill: parent
        asynchronous: true
        cache: true
        property string requestedSource: ""
        property int requestedGeneration: -1
        fillMode: root.fillModeFor(root.wallpaperFit)
        visible: root.activeSlot === 1

        onStatusChanged: {
            if (status === Image.Ready)
                root.completeLoad(back, 1)
            else if (status === Image.Error)
                root.failedLoad(back)
        }
    }

    Component.onCompleted: beginLoad()
}
