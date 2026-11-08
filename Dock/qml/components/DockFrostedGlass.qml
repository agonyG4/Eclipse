import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared

// The frosted press plate with a blur of its own. The held icon moves into this
// layer as well, since the glass would otherwise blur it along with the backdrop.
GlassLayer {
    id: root
    objectName: "dockFrostedGlass"

    // The DockAppDelegate currently pressed, or null.
    property Item source: null
    property bool lightMaterial: false
    readonly property real amount: source ? source.frostedAmount : 0
    // True once the copy is on screen, so the delegate can hide its own icon
    // without leaving a frame with no icon at all.
    readonly property bool iconPresented: source !== null && presentedFrames >= 2
    property int presentedFrames: 0

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
        plate.x = rect.x - inset
        plate.y = rect.y - inset
        plate.width = rect.width + inset * 2
        plate.height = rect.height + inset * 2
        plate.radius = (icon.iconRadius + root.source.plateInset) * iconScale
    }

    onSourceChanged: {
        root.presentedFrames = 0
        root.syncGeometry()
    }

    // Tracks the icon through magnification and the bounce on the window's own
    // frames while held, and stays idle otherwise.
    FrameAnimation {
        running: root.source !== null
        onTriggered: {
            root.syncGeometry()
            if (iconCopy.ready && root.presentedFrames < 2)
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
        retainWhileLoading: true
        maximumPresentationScale: Math.max(1, DockController.magnificationScale)
        iconRadius: 10
        fallbackRadius: 10

        layer.enabled: root.source !== null && root.source.pressAmount > 0.001
        layer.textureSize: Qt.size(Math.ceil(width * Math.max(1, scale) * Screen.devicePixelRatio),
                                   Math.ceil(height * Math.max(1, scale) * Screen.devicePixelRatio))
        layer.smooth: true
        layer.effect: MultiEffect {
            brightness: root.source ? -root.source.pressStrength * root.source.pressAmount : 0
        }
    }
}
