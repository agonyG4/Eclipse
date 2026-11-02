import QtQuick

// File metadata and thumbnail generation belong to the caller. This component
// only composes their results, preserving artwork colors and aspect ratio.
AstreaIcon {
    id: root
    property url thumbnailSource
    property int thumbnailPixelSize: effectiveSourcePixelSize
    property bool thumbnailAsynchronous: true
    property var emblemSources: []
    property int emblemSize: 18
    readonly property int thumbnailStatus: thumbnailLoader.item ? thumbnailLoader.item.status : Image.Null
    readonly property bool thumbnailReady: thumbnailStatus === Image.Ready
    iconRadius: 0
    appearanceOverride: "default"
    showFallbackText: false
    imageVisible: !thumbnailReady

    // A replaced source cannot display pixels from the previous file. The
    // ordinary icon remains visible until the requested preview is ready.
    Loader {
        id: thumbnailLoader
        anchors.fill: parent
        active: root.thumbnailSource.toString() !== ""
        sourceComponent: Image {
            source: root.thumbnailSource
            sourceSize: Qt.size(root.thumbnailPixelSize, root.thumbnailPixelSize)
            asynchronous: root.thumbnailAsynchronous
            visible: root.thumbnailReady
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            cache: true
        }
    }
    Repeater {
        model: root.emblemSources
        delegate: AstreaIcon {
            required property int index
            required property var modelData
            width: root.emblemSize
            height: root.emblemSize
            x: root.width - width - index * (width + 2)
            y: root.height - height
            source: String(modelData)
            iconSize: root.emblemSize
            iconRadius: 0
            appearanceOverride: "default"
            showFallbackText: false
            asynchronous: false
            devicePixelRatioOverride: root.effectiveDevicePixelRatio
        }
    }
}
