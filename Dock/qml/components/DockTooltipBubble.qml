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
    readonly property rect bodyRect: Qt.rect(bodyX, bodyY, cardWidth, bodyHeight)
    readonly property real tailCenter:
        (pointerEdge === "left" || pointerEdge === "right" ? bodyY : bodyX) + pointerCenter
    readonly property point tailTip: pointerEdge === "bottom"
        ? Qt.point(tailCenter, bodyY + bodyHeight + pointerDepth)
        : pointerEdge === "top"
            ? Qt.point(tailCenter, bodyY - pointerDepth)
            : pointerEdge === "left"
                ? Qt.point(bodyX - pointerDepth, tailCenter)
                : Qt.point(bodyX + cardWidth + pointerDepth, tailCenter)
    readonly property point tailBaseA: pointerEdge === "bottom"
        ? Qt.point(tailCenter - pointerWidth / 2, bodyY + bodyHeight)
        : pointerEdge === "top"
            ? Qt.point(tailCenter - pointerWidth / 2, bodyY)
            : pointerEdge === "left"
                ? Qt.point(bodyX + sideTailArcInset, tailCenter - pointerWidth / 2)
                : Qt.point(bodyX + cardWidth - sideTailArcInset,
                           tailCenter - pointerWidth / 2)
    readonly property point tailBaseB: pointerEdge === "bottom"
        ? Qt.point(tailCenter + pointerWidth / 2, bodyY + bodyHeight)
        : pointerEdge === "top"
            ? Qt.point(tailCenter + pointerWidth / 2, bodyY)
            : pointerEdge === "left"
                ? Qt.point(bodyX + sideTailArcInset, tailCenter + pointerWidth / 2)
                : Qt.point(bodyX + cardWidth - sideTailArcInset,
                           tailCenter + pointerWidth / 2)
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
        const x0 = bodyRect.x
        const y0 = bodyRect.y
        const x1 = bodyRect.x + bodyRect.width
        const y1 = bodyRect.y + bodyRect.height
        const radius = bodyRadius

        if (pointerEdge === "bottom") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(x1 - radius)
                + arcTo(x1, y0 + radius, radius)
                + " V " + number(y1 - radius)
                + arcTo(x1 - radius, y1, radius)
                + " H " + number(tailBaseB.x)
                + " L " + number(tailTip.x) + " " + number(tailTip.y)
                + " L " + number(tailBaseA.x) + " " + number(tailBaseA.y)
                + " H " + number(x0 + radius)
                + arcTo(x0, y1 - radius, radius)
                + " V " + number(y0 + radius)
                + arcTo(x0 + radius, y0, radius) + " Z"
        }

        if (pointerEdge === "top") {
            return "M " + number(x0 + radius) + " " + number(y0)
                + " H " + number(tailBaseA.x)
                + " L " + number(tailTip.x) + " " + number(tailTip.y)
                + " L " + number(tailBaseB.x) + " " + number(tailBaseB.y)
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
                + arcTo(tailBaseB.x, tailBaseB.y, radius)
                + " L " + number(tailTip.x) + " " + number(tailTip.y)
                + " L " + number(tailBaseA.x) + " " + number(tailBaseA.y)
                + arcTo(x0 + radius, y0, radius) + " Z"
        }

        return "M " + number(x0 + radius) + " " + number(y0)
            + " H " + number(x1 - radius)
            + arcTo(tailBaseA.x, tailBaseA.y, radius)
            + " L " + number(tailTip.x) + " " + number(tailTip.y)
            + " L " + number(tailBaseB.x) + " " + number(tailBaseB.y)
            + arcTo(x1 - radius, y1, radius)
            + " H " + number(x0 + radius)
            + arcTo(x0, y1 - radius, radius)
            + " V " + number(y0 + radius)
            + arcTo(x0 + radius, y0, radius) + " Z"
    }

    function buildBackdropBodyRectangles() {
        const rectangles = []
        const radius = bodyRadius
        const centerY = bodyRect.y + radius

        for (let row = 0; row < Math.ceil(bodyRect.height); ++row) {
            const y = bodyRect.y + row
            const sampleY = y + 0.5
            const dy = Math.abs(sampleY - centerY)
            const radiusSquared = radius * radius
            if (dy > radius)
                continue
            const horizontalReach = Math.sqrt(Math.max(0, radiusSquared - dy * dy))
            const leftExtent = bodyRect.x + radius - horizontalReach
            const rightExtent = bodyRect.x + bodyRect.width - radius + horizontalReach
            const firstPixel = Math.ceil(leftExtent - 0.5)
            const lastPixel = Math.floor(rightExtent - 0.5)
            if (lastPixel >= firstPixel)
                rectangles.push(Qt.rect(firstPixel, y, lastPixel - firstPixel + 1, 1))
        }
        return rectangles
    }

    function buildBackdropTailRectangles() {
        const rectangles = []
        const points = [tailBaseA, tailBaseB, tailTip]
        const horizontal = pointerEdge === "bottom" || pointerEdge === "top"
        const primary = points.map(function(point) { return horizontal ? point.y : point.x })
        const secondary = points.map(function(point) { return horizontal ? point.x : point.y })
        let first = Math.floor(Math.min(primary[0], primary[1], primary[2]))
        let end = Math.ceil(Math.max(primary[0], primary[1], primary[2]))
        if (pointerEdge === "left")
            end = Math.min(end, Math.floor(bodyRect.x) + 1)
        else if (pointerEdge === "right")
            first = Math.max(first, Math.ceil(bodyRect.x + bodyRect.width) - 1)
        const edges = [[0, 1], [1, 2], [2, 0]]

        for (let slice = first; slice < end; ++slice) {
            const sample = slice + 0.5
            const intersections = []
            for (let index = 0; index < edges.length; ++index) {
                const p = edges[index][0]
                const q = edges[index][1]
                const pPrimary = primary[p]
                const qPrimary = primary[q]
                if ((pPrimary <= sample && sample < qPrimary)
                    || (qPrimary <= sample && sample < pPrimary)) {
                    const fraction = (sample - pPrimary) / (qPrimary - pPrimary)
                    intersections.push(secondary[p]
                                       + fraction * (secondary[q] - secondary[p]))
                }
            }
            if (intersections.length < 2)
                continue
            const low = Math.min(intersections[0], intersections[1])
            const high = Math.max(intersections[0], intersections[1])
            const firstPixel = Math.ceil(low - 0.5)
            const lastPixel = Math.floor(high - 0.5)
            if (lastPixel < firstPixel)
                continue
            rectangles.push(horizontal
                ? Qt.rect(firstPixel, slice, lastPixel - firstPixel + 1, 1)
                : Qt.rect(slice, firstPixel, 1, lastPixel - firstPixel + 1))
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
