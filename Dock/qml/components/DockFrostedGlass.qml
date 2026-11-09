import QtQuick
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared

// The frosted plate under the hovered or held icon, with a blur of its own. That
// icon moves into this layer as well, since the glass would otherwise blur it along
// with the backdrop.
GlassLayer {
    id: root
    objectName: "dockFrostedGlass"

    // The DockAppDelegate that owns the plate, or null.
    property Item source: null
    property bool lightMaterial: false
    readonly property real amount: Math.max(source ? source.frostedAmount : 0,
                                            handoffAmount * glide)
    // True once the copy is on screen, so the delegate can hide its own icon
    // without leaving a frame with no icon at all.
    readonly property bool iconPresented: source !== null && presentedFrames >= 2
    property int presentedFrames: 0

    // Moving between icons glides the plate instead of collapsing it.
    property real glide: 0
    property real handoffAmount: 0
    property rect handoffOffset: Qt.rect(0, 0, 0, 0)

    glassItem: source ? blurArea : null
    glassRadius: Math.min(plate.radius * root.amount, blurArea.width / 2, blurArea.height / 2)

    function syncGeometry() {
        if (!root.source)
            return
        const icon = root.source.iconVisual
        const rect = icon.mapToItem(root, 0, 0, icon.width, icon.height)
        const iconScale = icon.width > 0 ? rect.width / icon.width : 1
        const inset = root.source.plateInset * iconScale
        iconCopy.x = rect.x
        iconCopy.y = rect.y
        iconCopy.width = icon.width
        iconCopy.height = icon.height
        iconCopy.scale = iconScale
        plate.x = rect.x - inset + root.handoffOffset.x * root.glide
        plate.y = rect.y - inset + root.handoffOffset.y * root.glide
        plate.width = rect.width + inset * 2 + root.handoffOffset.width * root.glide
        plate.height = rect.height + inset * 2 + root.handoffOffset.height * root.glide
        plate.radius = (icon.iconRadius + root.source.plateInset) * iconScale
    }

    onSourceChanged: {
        const previous = Qt.rect(plate.x, plate.y, plate.width, plate.height)
        const previousAmount = root.plateAmount
        root.presentedFrames = 0
        glideAnimation.stop()
        root.glide = 0
        root.handoffOffset = Qt.rect(0, 0, 0, 0)
        root.syncGeometry()
        if (!root.source || previousAmount <= 0.01)
            return
        root.handoffAmount = previousAmount
        root.handoffOffset = Qt.rect(previous.x - plate.x, previous.y - plate.y,
                                     previous.width - plate.width, previous.height - plate.height)
        root.glide = 1
        root.syncGeometry()
        glideAnimation.duration = root.source.dockPanel
            ? root.source.dockPanel.animationDuration(140) : 120
        glideAnimation.restart()
    }
    onGlideChanged: root.syncGeometry()
    // The amount the plate last had, read when the source has already changed.
    property real plateAmount: 0
    onAmountChanged: {
        if (root.source)
            root.plateAmount = root.amount
    }

    NumberAnimation {
        id: glideAnimation
        target: root
        property: "glide"
        to: 0
        easing.type: Easing.OutCubic
    }

    // The icon only moves when the Dock's window renders, so following that window
    // leaves this surface idle under a resting pointer.
    Connections {
        target: root.source ? root.Window.window : null
        function onAfterAnimating() { root.syncGeometry() }
    }
    FrameAnimation {
        running: root.source !== null && (root.presentedFrames < 2 || root.amount < 0.999)
        onTriggered: {
            root.syncGeometry()
            // An icon with no image draws its fallback at once, in both places.
            if ((iconCopy.ready || !root.source.iconVisual.ready) && root.presentedFrames < 2)
                root.presentedFrames += 1
        }
    }

    Rectangle {
        id: plate
        objectName: "dockFrostedPlate"
        readonly property real shade: root.lightMaterial ? 0 : 1
        visible: root.source !== null
        color: Qt.rgba(shade, shade, shade, 0.16 * root.amount)
        border.width: 1
        border.color: Qt.rgba(shade, shade, shade, 0.13 * root.amount)

        Item {
            id: blurArea
            objectName: "dockFrostedBlurArea"
            anchors.centerIn: parent
            width: parent.width * root.amount
            height: parent.height * root.amount
        }
    }

    Shared.AstreaAppIcon {
        id: iconCopy
        objectName: "dockFrostedIcon"
        visible: root.source !== null
        transformOrigin: Item.TopLeft
        iconName: root.source ? root.source.iconName : ""
        iconPath: root.source ? root.source.iconPath : ""
        iconUrl: root.source ? root.source.iconUrl : ""
        appName: root.source ? root.source.displayName : ""
        iconSize: DockController.iconSize
        maximumPresentationScale: Math.max(1, DockController.magnificationScale)
        iconRadius: 10
        fallbackRadius: 10
    }
}
