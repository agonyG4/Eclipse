import QtQuick
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared

Window {
    id: window
    objectName: "dockTooltipSurface"

    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
    property string title: ""
    property rect anchorRect: Qt.rect(0, 0, 0, 0)
    property string dockPosition: "bottom"
    property bool tooltipVisible: false

    readonly property int cardWidth: Math.min(280, Math.max(72, tooltipText.implicitWidth + 32))
    readonly property int cardHeight: DockSurfaceGeometry.tooltipBodyHeight
    readonly property int pointerDepth: DockSurfaceGeometry.tooltipPointerDepth
    readonly property var cardPlacement: DockSurfaceGeometry.tooltipPlacementInOutput(
        Math.max(1, outputWidth), Math.max(1, outputHeight), anchorRect,
        cardWidth, cardHeight, dockPosition)
    readonly property rect cardRect: cardPlacement.cardRect

    visible: tooltipVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowTransparentForInput
        | Qt.WindowDoesNotAcceptFocus
    width: Math.max(1, outputWidth)
    height: Math.max(1, outputHeight)

    Shared.ShellMaterialTheme {
        id: material
        objectName: "dockTooltipMaterial"
    }

    function showTooltip(text, visualAnchorRect, position) {
        title = text
        anchorRect = visualAnchorRect
        dockPosition = position
        tooltipVisible = text.length > 0
    }

    function hideTooltip() {
        tooltipVisible = false
        title = ""
    }

    BackdropEffectRegions {
        objectName: "dockTooltipBackdropRegions"
        anchors.fill: parent
        enabled: material.isFrosted && window.tooltipVisible

        BackdropRegion {
            item: tooltipBackdropBody
            enabled: tooltipBackdropBody.visible
            radius: window.cardHeight / 2
        }

        BackdropRegion {
            item: tooltipBubble
            enabled: tooltipBubble.visible
            localRectangles: tooltipBubble.backdropTailRectangles
            radius: 0
        }
    }

    // The compositor rounds this body proxy; the vector tail contributes only
    // bounded local scanlines from the same one-piece bubble geometry.
    Item {
        id: tooltipBackdropBody
        objectName: "dockTooltipBackdropBody"
        x: window.cardRect.x
        y: window.cardRect.y
        width: window.cardRect.width
        height: window.cardRect.height
        visible: window.tooltipVisible && window.title.length > 0
    }

    DockTooltipBubble {
        id: tooltipBubble
        x: window.cardRect.x - (cardPlacement.pointerEdge === "left" ? pointerDepth : 0)
        y: window.cardRect.y - (cardPlacement.pointerEdge === "top" ? pointerDepth : 0)
        cardWidth: window.cardRect.width
        bodyHeight: window.cardRect.height
        pointerEdge: window.cardPlacement.pointerEdge
        pointerCenter: window.cardPlacement.pointerCenter
        fillColor: material.tooltipSurface
        borderColor: material.tooltipBorder
        shadowColor: material.tooltipShadow
        visible: window.tooltipVisible && window.title.length > 0
        z: 1
    }

    Text {
        id: tooltipText
        objectName: "dockTooltipText"
        x: window.cardRect.x + 16
        y: window.cardRect.y
        width: window.cardRect.width - 32
        height: window.cardRect.height
        text: window.title
        color: material.textActive
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        maximumLineCount: 1
        font.family: material.fontFamily
        font.pixelSize: material.tooltipFontSize
        font.weight: Font.Medium
        visible: window.tooltipVisible && window.title.length > 0
        z: 2
    }
}
