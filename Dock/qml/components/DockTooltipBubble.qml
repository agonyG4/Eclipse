import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window

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
    readonly property real renderPadding: 1
    readonly property real effectiveDevicePixelRatio: {
        var value = Screen.devicePixelRatio
        if (!isFinite(value) || value <= 0)
            value = 1
        return Math.max(0.25, value)
    }
    readonly property real minimumRasterScale: 2.0
    readonly property real rasterScale: Math.max(minimumRasterScale,
                                                 effectiveDevicePixelRatio)
    readonly property int rasterTextureWidth: Math.ceil(width * rasterScale)
    readonly property int rasterTextureHeight: Math.ceil(height * rasterScale)
    readonly property int pointerWidth: DockSurfaceGeometry.tooltipPointerWidth
    readonly property int pointerDepth: DockSurfaceGeometry.tooltipPointerDepth
    readonly property real bodyRadius: bodyHeight / 2
    readonly property real sideTailArcInset:
        bodyRadius - Math.sqrt(Math.max(0, bodyRadius * bodyRadius
                                            - pointerWidth * pointerWidth / 4))
    readonly property real bodyX:
        renderPadding + (pointerEdge === "left" ? pointerDepth : 0)
    readonly property real bodyY:
        renderPadding + (pointerEdge === "top" ? pointerDepth : 0)
    readonly property string pathData: buildPathData()
    readonly property var backdropBodyRectangles: buildBackdropBodyRectangles()
    readonly property var backdropTailRectangles: buildBackdropTailRectangles()
    readonly property var backdropRectangles:
        backdropBodyRectangles.concat(backdropTailRectangles)

    width: cardWidth + (pointerEdge === "left" || pointerEdge === "right"
                        ? pointerDepth : 0) + 2 * renderPadding
    height: bodyHeight + (pointerEdge === "top" || pointerEdge === "bottom"
                          ? pointerDepth : 0) + 2 * renderPadding

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

    function buildBackdropBodyRectangles() {
        const rectangles = []
        const radius = bodyRadius
        const centerY = bodyY + radius

        for (let row = 0; row < Math.ceil(bodyHeight); ++row) {
            const y = bodyY + row
            const sampleY = y + 0.5
            const dy = Math.abs(sampleY - centerY)
            const radiusSquared = radius * radius
            if (dy > radius)
                continue
            const horizontalReach = Math.sqrt(Math.max(0, radiusSquared - dy * dy))
            const leftExtent = bodyX + radius - horizontalReach
            const rightExtent = bodyX + cardWidth - radius + horizontalReach
            const firstPixel = Math.ceil(leftExtent - 0.5)
            const lastPixel = Math.floor(rightExtent - 0.5)
            if (lastPixel >= firstPixel)
                rectangles.push(Qt.rect(firstPixel, y, lastPixel - firstPixel + 1, 1))
        }
        return rectangles
    }

    function buildBackdropTailRectangles() {
        const rectangles = []
        const halfPointer = pointerWidth / 2
        const centerX = bodyX + pointerCenter
        const centerY = bodyY + pointerCenter
        const sliceThickness = 1

        if (pointerEdge === "bottom" || pointerEdge === "top") {
            for (let index = 0; index < pointerDepth; ++index) {
                const y = pointerEdge === "bottom"
                    ? bodyY + bodyHeight + index
                    : bodyY - index - sliceThickness
                const sampleY = y + 0.5
                const baseY = pointerEdge === "bottom"
                    ? bodyY + bodyHeight : bodyY
                const distanceFromBase = Math.abs(sampleY - baseY)
                const halfWidth = halfPointer
                    * Math.max(0, 1 - distanceFromBase / pointerDepth)
                if (halfWidth <= 0)
                    continue
                const leftExtent = centerX - halfWidth
                const rightExtent = centerX + halfWidth
                const firstPixel = Math.ceil(leftExtent - 0.5)
                const lastPixel = Math.floor(rightExtent - 0.5)
                if (lastPixel < firstPixel)
                    continue
                rectangles.push(Qt.rect(firstPixel, y,
                                        lastPixel - firstPixel + 1, sliceThickness))
            }
            return rectangles
        }

        const sideTailLength = pointerDepth + sideTailArcInset
        const bodyRight = bodyX + cardWidth
        const appendSideSlice = function(x, leftTail) {
            const sampleX = x + 0.5
            const distanceFromTip = leftTail
                ? sampleX - (bodyX - pointerDepth)
                : bodyRight + pointerDepth - sampleX
            const height = pointerWidth * Math.max(
                0, Math.min(1, distanceFromTip / sideTailLength))
            if (height <= 0)
                return
            const topExtent = centerY - height / 2
            const bottomExtent = centerY + height / 2
            const firstPixel = Math.ceil(topExtent - 0.5)
            const lastPixel = Math.floor(bottomExtent - 0.5)
            if (lastPixel >= firstPixel)
                rectangles.push(Qt.rect(x, firstPixel, sliceThickness,
                                        lastPixel - firstPixel + 1))
        }
        if (pointerEdge === "left") {
            for (let index = 0; index < pointerDepth; ++index)
                appendSideSlice(bodyX - index, true)
        } else {
            for (let index = 0; index < pointerDepth; ++index)
                appendSideSlice(bodyRight - sliceThickness + index, false)
        }
        return rectangles
    }

    Shape {
        id: bubbleShape
        objectName: "dockTooltipShape"
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        layer.enabled: root.visible
        layer.textureSize: Qt.size(root.rasterTextureWidth, root.rasterTextureHeight)
        layer.smooth: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: root.shadowColor
            shadowBlur: 0.22
            shadowVerticalOffset: 2
        }

        ShapePath {
            objectName: "dockTooltipStroke"
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
