import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

Item {
    id: root
    objectName: "dockTooltipBubble"

    property real cardWidth: 72
    property real bodyHeight: DockSurfaceGeometry.tooltipBodyHeight
    property string pointerEdge: "bottom"
    property real pointerCenter: 0
    property color fillColor: "transparent"
    property color borderColor: "transparent"
    property color shadowColor: "transparent"
    readonly property int pointerWidth: DockSurfaceGeometry.tooltipPointerWidth
    readonly property int pointerDepth: DockSurfaceGeometry.tooltipPointerDepth
    readonly property real bodyRadius: bodyHeight / 2
    readonly property real bodyX: pointerEdge === "left" ? pointerDepth : 0
    readonly property real bodyY: pointerEdge === "top" ? pointerDepth : 0
    readonly property string pathData: buildPathData()

    width: cardWidth + (pointerEdge === "left" || pointerEdge === "right"
                        ? pointerDepth : 0)
    height: bodyHeight + (pointerEdge === "top" || pointerEdge === "bottom"
                          ? pointerDepth : 0)

    function number(value) {
        return Number(value.toFixed(3)).toString()
    }

    function buildPathData() {
        const x0 = bodyX
        const y0 = bodyY
        const x1 = bodyX + cardWidth
        const y1 = bodyY + bodyHeight
        const radius = Math.min(bodyRadius, cardWidth / 2, bodyHeight / 2)
        const halfPointer = pointerWidth / 2
        const center = pointerCenter

        if (pointerEdge === "bottom") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(x1 - radius)
                + " Q " + number(x1) + " " + number(y0) + " "
                + number(x1) + " " + number(y0 + radius)
                + " V " + number(y1 - radius)
                + " Q " + number(x1) + " " + number(y1) + " "
                + number(x1 - radius) + " " + number(y1)
                + " H " + number(center + halfPointer)
                + " L " + number(center) + " " + number(y1 + pointerDepth)
                + " L " + number(center - halfPointer) + " " + number(y1)
                + " H " + number(x0 + radius)
                + " Q " + number(x0) + " " + number(y1) + " "
                + number(x0) + " " + number(y1 - radius)
                + " V " + number(y0 + radius)
                + " Q " + number(x0) + " " + number(y0) + " "
                + number(x0 + radius) + " " + number(y0) + " Z"
        }

        if (pointerEdge === "top") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(center - halfPointer)
                + " L " + number(center) + " " + number(y0 - pointerDepth)
                + " L " + number(center + halfPointer) + " " + number(y0)
                + " H " + number(x1 - radius)
                + " Q " + number(x1) + " " + number(y0) + " "
                + number(x1) + " " + number(y0 + radius)
                + " V " + number(y1 - radius)
                + " Q " + number(x1) + " " + number(y1) + " "
                + number(x1 - radius) + " " + number(y1)
                + " H " + number(x0 + radius)
                + " Q " + number(x0) + " " + number(y1) + " "
                + number(x0) + " " + number(y1 - radius)
                + " V " + number(y0 + radius)
                + " Q " + number(x0) + " " + number(y0) + " "
                + number(x0 + radius) + " " + number(y0) + " Z"
        }

        if (pointerEdge === "left") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(x1 - radius)
                + " Q " + number(x1) + " " + number(y0) + " "
                + number(x1) + " " + number(y0 + radius)
                + " V " + number(y1 - radius)
                + " Q " + number(x1) + " " + number(y1) + " "
                + number(x1 - radius) + " " + number(y1)
                + " H " + number(x0 + radius)
                + " Q " + number(x0) + " " + number(y1) + " "
                + number(x0) + " " + number(y1 - radius)
                + " V " + number(center + halfPointer)
                + " L " + number(x0 - pointerDepth) + " " + number(center)
                + " L " + number(x0) + " " + number(center - halfPointer)
                + " V " + number(y0 + radius)
                + " Q " + number(x0) + " " + number(y0) + " "
                + number(x0 + radius) + " " + number(y0) + " Z"
        }

        return "M " + number(x0 + radius) + " " + number(y0)
            + " H " + number(x1 - radius)
            + " Q " + number(x1) + " " + number(y0) + " "
            + number(x1) + " " + number(y0 + radius)
            + " V " + number(center - halfPointer)
            + " L " + number(x1 + pointerDepth) + " " + number(center)
            + " L " + number(x1) + " " + number(center + halfPointer)
            + " V " + number(y1 - radius)
            + " Q " + number(x1) + " " + number(y1) + " "
            + number(x1 - radius) + " " + number(y1)
            + " H " + number(x0 + radius)
            + " Q " + number(x0) + " " + number(y1) + " "
            + number(x0) + " " + number(y1 - radius)
            + " V " + number(y0 + radius)
            + " Q " + number(x0) + " " + number(y0) + " "
            + number(x0 + radius) + " " + number(y0) + " Z"
    }

    Shape {
        id: bubbleShape
        objectName: "dockTooltipShape"
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        layer.enabled: root.visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: root.shadowColor
            shadowBlur: 0.22
            shadowVerticalOffset: 2
        }

        ShapePath {
            fillColor: root.fillColor
            strokeColor: root.borderColor
            strokeWidth: 1
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap

            PathSvg {
                path: root.pathData
            }
        }
    }
}
