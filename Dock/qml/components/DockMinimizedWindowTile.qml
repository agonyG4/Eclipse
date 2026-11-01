import QtQuick
import QtQuick.Effects
import Astrea.Shared as Shared

// The visual of a minimized window in the Dock, after macOS: the window
// scaled into the icon square with rounded corners, a hairline edge and a
// soft contact shadow, badged with its app icon.
//
// It renders through the same AstreaAppIcon path as the Dock's app icons:
// the source is decoded at the magnified size for the screen's DPR and
// sampled directly by the rounded-corner shader, and shadows are analytic.
// No offscreen layers, so it stays as sharp as its neighbours when magnified.
Item {
    id: root

    property url source
    // Pixel size of the captured window image; sets the tile's aspect.
    property size sourceSize: Qt.size(0, 0)
    property string iconName: ""
    property string iconPath: ""
    property string appName: ""
    property real maximumPresentationScale: 1
    property bool pressed: false
    // The current magnification scale; hairlines stay one device pixel wide.
    property real presentationScale: 1
    readonly property bool imageReady: windowImage.ready
    readonly property real fitScale: sourceSize.width > 0 && sourceSize.height > 0
        ? Math.min(width / sourceSize.width, height / sourceSize.height) : 0
    readonly property real paintedWidth: Math.round(sourceSize.width * fitScale)
    readonly property real paintedHeight: Math.round(sourceSize.height * fitScale)
    readonly property int cornerRadius: Math.max(3, Math.round(Math.min(width, height) * 0.08))

    RectangularShadow {
        anchors.fill: windowImage
        visible: windowImage.visible
        radius: root.cornerRadius
        blur: 5
        spread: 0
        offset.y: 1.5
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    Shared.AstreaAppIcon {
        id: windowImage
        objectName: root.objectName.length > 0 ? root.objectName + "-image" : ""
        anchors.centerIn: parent
        width: Math.max(1, root.paintedWidth)
        height: Math.max(1, root.paintedHeight)
        iconUrl: root.fitScale > 0 ? String(root.source) : ""
        iconSize: Math.max(root.width, root.height)
        maximumPresentationScale: root.maximumPresentationScale
        iconRadius: root.cornerRadius
        showFallbackText: false
        appearanceOverride: "default"
        // In-memory capture: swap synchronously so a refresh never flashes.
        asynchronous: false
        visible: root.imageReady
        opacity: root.pressed ? 0.72 : 1
        Behavior on opacity { NumberAnimation { duration: 90 } }
    }

    // A light hairline keeps dark and translucent windows legible on the dark
    // Dock material, like the outline macOS draws around windows.
    Rectangle {
        anchors.fill: windowImage
        visible: windowImage.visible
        radius: root.cornerRadius
        color: "transparent"
        border.width: 1 / Math.max(1, root.presentationScale)
        border.color: Qt.rgba(1, 1, 1, 0.24)
        antialiasing: true
    }

    Shared.AstreaAppIcon {
        anchors.centerIn: parent
        width: parent.width * 0.78
        height: width
        iconName: root.iconName
        iconPath: root.iconPath
        appName: root.appName
        iconSize: width
        maximumPresentationScale: root.maximumPresentationScale
        iconRadius: 10
        fallbackRadius: 10
        opacity: 0.85
        visible: !root.imageReady
    }

    RectangularShadow {
        anchors.fill: badge
        visible: badge.visible
        radius: badge.iconRadius
        blur: 4
        offset.y: 1
        color: Qt.rgba(0, 0, 0, 0.4)
    }

    Shared.AstreaAppIcon {
        id: badge
        objectName: root.objectName.length > 0 ? root.objectName + "-badge" : ""
        width: Math.round(root.width * 0.36)
        height: width
        x: Math.round(root.width / 2 + root.paintedWidth / 2 - width * 0.78)
        y: Math.round(root.height / 2 + root.paintedHeight / 2 - height * 0.78)
        iconName: root.iconName
        iconPath: root.iconPath
        appName: root.appName
        iconSize: width
        maximumPresentationScale: root.maximumPresentationScale
        iconRadius: Math.round(width * 0.225)
        fallbackRadius: Math.round(width * 0.225)
        visible: root.imageReady
    }
}
