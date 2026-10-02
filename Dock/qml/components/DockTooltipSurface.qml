import QtQuick
import QtQuick.Window
import QtQuick.Effects
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

    readonly property int cardWidth: Math.min(260, Math.max(64, tooltipText.implicitWidth + 24))
    readonly property int cardHeight: 34
    readonly property int pointerSize: 12
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

    Shared.ShellMaterialTheme { id: material }

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
        anchors.fill: parent
        enabled: material.isFrosted

        BackdropRegion {
            item: tooltipCard
            enabled: tooltipCard.visible
            radius: tooltipCard.radius
        }
    }

    Rectangle {
        id: tooltipPointer
        objectName: "dockTooltipPointer"
        z: 0
        width: window.pointerSize
        height: window.pointerSize
        x: window.cardPlacement.pointerEdge === "left"
            ? window.cardRect.x - width / 2
            : window.cardPlacement.pointerEdge === "right"
                ? window.cardRect.x + window.cardRect.width - width / 2
                : window.cardRect.x + window.cardPlacement.pointerCenter - width / 2
        y: window.cardPlacement.pointerEdge === "top"
            ? window.cardRect.y - height / 2
            : window.cardPlacement.pointerEdge === "bottom"
                ? window.cardRect.y + window.cardRect.height - height / 2
                : window.cardRect.y + window.cardPlacement.pointerCenter - height / 2
        rotation: 45
        transformOrigin: Item.Center
        radius: 1
        color: material.surfaceElevated
        border.color: material.border
        border.width: 1
        visible: tooltipCard.visible

        layer.enabled: visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: material.shadow
            shadowBlur: 0.28
            shadowVerticalOffset: 2
        }
    }

    Rectangle {
        id: tooltipCard
        objectName: "dockTooltipCard"
        z: 1
        x: window.cardRect.x
        y: window.cardRect.y
        width: window.cardRect.width
        height: window.cardRect.height
        radius: material.radiusMedium
        color: material.surfaceElevated
        border.color: material.border
        border.width: 1
        visible: window.tooltipVisible && window.title.length > 0

        layer.enabled: visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: material.shadow
            shadowBlur: 0.36
            shadowVerticalOffset: 3
        }

        Text {
            id: tooltipText
            objectName: "dockTooltipText"
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            text: window.title
            color: material.textActive
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            maximumLineCount: 1
            font.family: material.fontFamily
            font.pixelSize: material.fontSizeCaption
            font.weight: Font.Medium
        }
    }
}
