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
    readonly property real sideTailArcInset:
        bodyRadius - Math.sqrt(Math.max(0, bodyRadius * bodyRadius
                                            - pointerWidth * pointerWidth / 4))
    readonly property real bodyX: pointerEdge === "left" ? pointerDepth : 0
    readonly property real bodyY: pointerEdge === "top" ? pointerDepth : 0
    readonly property string pathData: buildPathData()
    readonly property var backdropTailRectangles: buildBackdropTailRectangles()

    width: cardWidth + (pointerEdge === "left" || pointerEdge === "right"
                        ? pointerDepth : 0)
    height: bodyHeight + (pointerEdge === "top" || pointerEdge === "bottom"
                          ? pointerDepth : 0)

    function number(value) {
        return Number(value.toFixed(3)).toString()
    }

    function arcTo(x, y, radius) {
        return " A " + number(radius) + " " + number(radius) + " 0 0 1 "
            + number(x) + " " + number(y)
    }

    function buildPathData() {
        const x0 = bodyX
        const y0 = bodyY
        const x1 = bodyX + cardWidth
        const y1 = bodyY + bodyHeight
        const radius = bodyRadius
        const halfPointer = pointerWidth / 2
        const center = pointerCenter

        if (pointerEdge === "bottom") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(x1 - radius)
                + arcTo(x1, y0 + radius, radius)
                + " V " + number(y1 - radius)
                + arcTo(x1 - radius, y1, radius)
                + " H " + number(center + halfPointer)
                + " L " + number(center) + " " + number(y1 + pointerDepth)
                + " L " + number(center - halfPointer) + " " + number(y1)
                + " H " + number(x0 + radius)
                + arcTo(x0, y1 - radius, radius)
                + " V " + number(y0 + radius)
                + arcTo(x0 + radius, y0, radius) + " Z"
        }

        if (pointerEdge === "top") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(center - halfPointer)
                + " L " + number(center) + " " + number(y0 - pointerDepth)
                + " L " + number(center + halfPointer) + " " + number(y0)
                + " H " + number(x1 - radius)
                + arcTo(x1, y0 + radius, radius)
                + " V " + number(y1 - radius)
                + arcTo(x1 - radius, y1, radius)
                + " H " + number(x0 + radius)
                + arcTo(x0, y1 - radius, radius)
                + " V " + number(y0 + radius)
                + arcTo(x0 + radius, y0, radius) + " Z"
        }

        if (pointerEdge === "left") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(x1 - radius)
                + arcTo(x1, y0 + radius, radius)
                + " V " + number(y1 - radius)
                + arcTo(x1 - radius, y1, radius)
                + " H " + number(x0 + radius)
                + arcTo(x0 + sideTailArcInset, center + halfPointer, radius)
                + " L " + number(x0 - pointerDepth) + " " + number(center)
                + " L " + number(x0 + sideTailArcInset) + " "
                + number(center - halfPointer)
                + arcTo(x0 + radius, y0, radius) + " Z"
        }

        return "M " + number(x0 + radius) + " " + number(y0)
            + " H " + number(x1 - radius)
            + arcTo(x1 - sideTailArcInset, center - halfPointer, radius)
            + " L " + number(x1 + pointerDepth) + " " + number(center)
            + " L " + number(x1 - sideTailArcInset) + " "
            + number(center + halfPointer)
            + arcTo(x1 - radius, y1, radius)
            + " H " + number(x0 + radius)
            + arcTo(x0, y1 - radius, radius)
            + " V " + number(y0 + radius)
            + arcTo(x0 + radius, y0, radius) + " Z"
    }

    function buildBackdropTailRectangles() {
        const rectangles = []
        const halfPointer = pointerWidth / 2
        const centerX = bodyX + pointerCenter
        const centerY = bodyY + pointerCenter
        const sliceThickness = 1

        if (pointerEdge === "bottom" || pointerEdge === "top") {
            for (let index = 0; index < pointerDepth; ++index) {
                const width = pointerWidth * (pointerDepth - index - 1) / pointerDepth
                if (width <= 0)
                    continue
                const y = pointerEdge === "bottom"
                    ? bodyY + bodyHeight + index
                    : bodyY - index - sliceThickness
                rectangles.push(Qt.rect(centerX - width / 2, y, width, sliceThickness))
            }
            return rectangles
        }

        const sideTailLength = pointerDepth + sideTailArcInset
        const bodyRight = bodyX + cardWidth
        const baseSliceHeight = pointerWidth * pointerDepth / sideTailLength
        if (pointerEdge === "left") {
            rectangles.push(Qt.rect(bodyX, centerY - baseSliceHeight / 2,
                                    sliceThickness, baseSliceHeight))
            for (let index = 0; index < pointerDepth - 1; ++index) {
                const height = pointerWidth * (pointerDepth - index - 1) / sideTailLength
                if (height <= 0)
                    continue
                rectangles.push(Qt.rect(bodyX - index - sliceThickness,
                                        centerY - height / 2,
                                        sliceThickness, height))
            }
        } else {
            rectangles.push(Qt.rect(bodyRight - sliceThickness,
                                    centerY - baseSliceHeight / 2,
                                    sliceThickness, baseSliceHeight))
            for (let index = 0; index < pointerDepth - 1; ++index) {
                const height = pointerWidth * (pointerDepth - index - 1) / sideTailLength
                if (height <= 0)
                    continue
                rectangles.push(Qt.rect(bodyRight + index, centerY - height / 2,
                                        sliceThickness, height))
            }
        }
        return rectangles
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
