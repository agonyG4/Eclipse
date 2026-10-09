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

    readonly property int horizontalTextPadding: 16
    readonly property int minimumCardWidth: 72
    readonly property int designMaximumCardWidth: 420
    readonly property int outputEdgeInset: 8
    readonly property int availableWidth: Math.max(1, outputWidth - 2 * outputEdgeInset)
    readonly property int effectiveMaximumCardWidth:
        Math.min(designMaximumCardWidth, availableWidth)
    readonly property int effectiveMinimumCardWidth:
        Math.min(minimumCardWidth, effectiveMaximumCardWidth)
    readonly property real naturalTextWidth: tooltipFontMetrics.advanceWidth(title)
    readonly property int naturalCardWidth:
        Math.ceil(naturalTextWidth) + 2 * horizontalTextPadding
    readonly property int cardWidth: Math.max(effectiveMinimumCardWidth,
                                               Math.min(naturalCardWidth,
                                                        effectiveMaximumCardWidth))
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

    FontMetrics {
        id: tooltipFontMetrics
        objectName: "dockTooltipFontMetrics"
        font.family: material.fontFamily
        font.pixelSize: material.tooltipFontSize
        font.weight: Font.Medium
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

        analyticCoverage: BackdropAnalyticCoverage {
            item: tooltipBubble
            roundedRectEnabled: true
            roundedRect: tooltipBubble.bodyRect
            radius: tooltipBubble.bodyRadius
            triangleEnabled: true
            triangleA: tooltipBubble.tailBaseA
            triangleB: tooltipBubble.tailBaseB
            triangleC: tooltipBubble.tailTip
        }

        BackdropRegion {
            item: tooltipBubble
            enabled: tooltipBubble.visible
            localRectangles: tooltipBubble.backdropRectangles
            radius: 0
        }
    }

    DockTooltipBubble {
        id: tooltipBubble
        x: window.cardRect.x
            - (cardPlacement.pointerEdge === "left" ? pointerDepth : 0)
            - renderPadding
        y: window.cardRect.y
            - (cardPlacement.pointerEdge === "top" ? pointerDepth : 0)
            - renderPadding
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
        x: window.cardRect.x + window.horizontalTextPadding
        y: window.cardRect.y
        width: window.cardRect.width - 2 * window.horizontalTextPadding
        height: window.cardRect.height
        text: window.title
        textFormat: Text.PlainText
        color: material.textActive
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        maximumLineCount: 1
        font: tooltipFontMetrics.font
        visible: window.tooltipVisible && window.title.length > 0
        z: 2
    }
}
