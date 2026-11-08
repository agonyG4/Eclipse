import QtQuick
import Astrea.Effects

// An independent blur surface above whatever is drawn beneath it, including the
// rest of its own window. A window's surface has one blur pass, so a piece of UI
// that must blur its parent, such as a pressed plate or a glass button, needs a
// surface of its own. Everything above that glass has to live in this layer too,
// because anything left in the parent window gets blurred with the backdrop.
//
// The content children are drawn in the layer. Input passes through to the parent
// window, which keeps owning handlers and hit testing.
//
// The compositor blur has no strength, only a region, so never fade glass: point
// glassItem at an item in the content and animate that item's size or scale. The
// blur follows it every frame. With no visible glassItem the surface stays mapped
// but requests no blur at all, which is what makes the first press immediate.
Item {
    id: root

    property Item glassItem: null
    property real glassRadius: 0
    property bool surfaceEnabled: true
    default property alias contentData: contentHost.data

    readonly property alias surfaceWindow: childWindow
    readonly property alias backdropRegions: regions

    WindowContainer {
        anchors.fill: parent
        window: AstreaEffectChildWindow {
            id: childWindow
            objectName: "glassLayerWindow"
            width: root.width
            height: root.height
            visible: root.visible && root.surfaceEnabled
            // The blur comes from the regions below, not the whole window.
            effectEnabled: false

            Item {
                id: contentHost
                parent: childWindow.contentItem
                anchors.fill: parent
            }

            BackdropEffectRegions {
                id: regions
                objectName: "glassLayerRegions"
                parent: childWindow.contentItem
                anchors.fill: parent

                BackdropRegion {
                    item: root.glassItem
                    enabled: root.glassItem !== null
                    radius: root.glassRadius
                }
            }
        }
    }
}
