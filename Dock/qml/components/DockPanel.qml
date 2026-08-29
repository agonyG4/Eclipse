pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    readonly property bool vertical: DockController.vertical
    readonly property bool growsPositiveCross: DockController.position === "left"
    readonly property int restingPrimary: (vertical ? appRow.implicitHeight : appRow.implicitWidth)
        + DockController.panelPadding * 2
    readonly property int restingCross: DockController.restingCrossThickness
    readonly property int restingWidth: vertical ? restingCross : restingPrimary
    readonly property int restingHeight: vertical ? restingPrimary : restingCross
    readonly property int contentWidth: restingWidth
    readonly property int contentHeight: restingHeight
    readonly property int surfaceCrossInset: DockController.chromeEdgeInset
    readonly property int slotPitch: (vertical ? DockController.delegateHeight
                                                : DockController.delegateWidth)
        + DockController.itemSpacing
    readonly property string configuredHoverEffect: DockController.hoverEffect
    readonly property real iconRestingTop: DockController.iconRestingTop
    readonly property real liftScale: 1.1
    readonly property real liftOffsetY: 5
    readonly property real reorderScale: 1.06
    readonly property real reorderOffsetY: 8
    readonly property real liftHeadroom: Math.max(0,
        DockController.iconSize * (liftScale - 1) + liftOffsetY - iconRestingTop)
    readonly property real dragHeadroom: Math.max(0,
        DockController.iconSize * (reorderScale - 1) + reorderOffsetY - iconRestingTop)
    readonly property real maxMagnificationHeadroom: Math.max(0,
        DockController.iconSize * (Math.max(1, DockController.magnificationScale) - 1))
    readonly property real surfaceHeadroom: Math.max(maxMagnificationHeadroom,
        liftHeadroom, dragHeadroom)
    readonly property int maximumMagnifiedDelegateCount: Math.min(appRepeater.count,
        2 * Math.ceil(Math.max(1, DockController.magnificationRadius)) + 1)
    readonly property real maximumMagnificationExtraWidth:
        maximumMagnifiedDelegateCount * DockController.iconSize
        * (Math.max(1, DockController.magnificationScale) - 1)
    readonly property real maximumMagnificationExtraPrimary: maximumMagnificationExtraWidth
    readonly property int surfaceWidth: vertical
        ? Math.max(restingWidth + surfaceCrossInset,
                   Math.ceil(restingWidth + surfaceHeadroom + surfaceCrossInset))
        : Math.max(restingWidth, Math.ceil(restingWidth + maximumMagnificationExtraPrimary))
    readonly property int surfaceHeight: vertical
        ? Math.max(restingHeight, Math.ceil(restingHeight + maximumMagnificationExtraPrimary))
        : Math.max(restingHeight + surfaceCrossInset,
                   Math.ceil(restingHeight + surfaceHeadroom + surfaceCrossInset))
    // Kept as a read-only compatibility property. The envelope reserves this
    // headroom structurally instead of resizing as the pointer moves.
    readonly property real visualHeadroom: surfaceHeadroom
    readonly property real animationSpeed: Math.max(0.25, DockController.animationSpeed)
    property real magnificationWidth: 0
    property real magnificationHeight: 0
    // pointerX remains the relative pointer coordinate for the current primary
    // axis. For Bottom this is the historical centered X coordinate.
    property real pointerX: 0
    property bool pointerInside: false
    property string pointerTargetDesktopFileName: ""
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
    property var inputRegionBridge: null
    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property string draggedDesktopFileName: ""
    property int draggedSourceIndex: -1
    property int dragTargetIndex: -1
    // Both values are relative to the panel center on the primary axis. This
    // keeps the drag coordinate invariant while the visual chrome changes.
    property real dragOriginCenterRelativeX: 0
    property real dragCenterRelativeX: 0
    property var magnificationScales: []
    property var extraWidths: []
    property var prefixExtraWidths: []
    property var inputInteractionRects: []

    signal reorderRequested(string desktopFileName, int targetPinIndex)

    width: surfaceWidth
    height: surfaceHeight

    function animationDuration(base) {
        if (!DockController.animationsEnabled)
            return 0
        return Math.max(1, Math.round(base / root.animationSpeed))
    }

    function crossOffset(value) {
        if (!vertical)
            return -value
        return growsPositiveCross ? value : -value
    }

    function primaryExtent() {
        return vertical ? height : width
    }

    Rectangle {
        id: dockChrome
        objectName: "dockChrome"
        x: root.vertical ? (DockController.position === "left" ? root.surfaceCrossInset
                                                                  : parent.width - width - root.surfaceCrossInset)
                         : (parent.width - width) / 2
        y: root.vertical ? (parent.height - height) / 2
                         : parent.height - height - root.surfaceCrossInset
        width: root.vertical ? root.restingWidth : root.restingWidth + root.magnificationWidth
        height: root.vertical ? root.restingHeight + root.magnificationHeight : root.restingHeight
        radius: DockController.cornerRadius
        color: "#80343434"
        border.color: "#33FFFFFF"
        border.width: 1
        visible: DockController.revealed

        Behavior on width {
            NumberAnimation { duration: root.animationDuration(90); easing.type: Easing.OutCubic }
        }
        Behavior on height {
            NumberAnimation { duration: root.animationDuration(90); easing.type: Easing.OutCubic }
        }
        onWidthChanged: root.scheduleInputRegionUpdate()
        onHeightChanged: root.scheduleInputRegionUpdate()
        onXChanged: root.scheduleInputRegionUpdate()
        onYChanged: root.scheduleInputRegionUpdate()

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#10000000"
        }

        Grid {
            id: appRow
            columns: root.vertical ? 1 : Math.max(1, appRepeater.count)
            rows: root.vertical ? Math.max(1, appRepeater.count) : 1
            rowSpacing: root.vertical ? DockController.itemSpacing : 0
            columnSpacing: root.vertical ? 0 : DockController.itemSpacing
            x: root.vertical ? 0 : (dockChrome.width - implicitWidth) / 2
            y: root.vertical ? (dockChrome.height - implicitHeight) / 2
                             : dockChrome.height - implicitHeight - DockController.chromeBottomMargin
            width: root.vertical ? dockChrome.width : implicitWidth

            Repeater {
                id: appRepeater
                model: DockController.appModel
                onCountChanged: root.updateHoverEffect()
                onItemAdded: root.updateHoverEffect()
                onItemRemoved: root.updateHoverEffect()

                delegate: DockAppDelegate {
                    dockPanel: root
                    iconSize: DockController.iconSize
                    slotWidth: DockController.delegateWidth
                    slotHeight: DockController.delegateHeight
                    contextMenuController: root.contextMenuController
                    dockSurfaceGeometry: root.dockSurfaceGeometry
                    outputKey: root.outputKey
                    pinned: model.pinned
                    pointerTarget: dockPanel.pointerTargetDesktopFileName === desktopFileName
                    onActivated: function(key) { DockController.launchByDesktopFileName(key) }
                    onDragStarted: function(key) { root.beginReorder(key) }
                    onDragMoved: function(key, translationX, sceneX, sceneY) {
                        root.updateReorder(key, translationX, sceneX, sceneY)
                    }
                    onDragFinished: function(key) { root.finishReorder(key) }
                    onDragCanceled: function(key) { root.cancelReorder(key) }
                }
            }
        }
    }

    // A mapped, bounded edge target keeps Always/Intelligent auto-hide
    // revealable without making transparent surface headroom clickable.
    Rectangle {
        id: edgeRevealTarget
        objectName: "edgeRevealTarget"
        width: root.vertical ? 4 : Math.min(root.width, 96)
        height: root.vertical ? Math.min(root.height, 96) : 4
        x: root.vertical
           ? (DockController.position === "left" ? 0 : root.width - width)
           : (root.width - width) / 2
        y: root.vertical ? (root.height - height) / 2 : root.height - height
        radius: Math.min(width, height) / 2
        color: "transparent"
        visible: DockController.physicalEdgeReveal && !DockController.revealed
    }

    Connections {
        target: DockController.appModel
        function onRowsMoved(sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow) {
            Qt.callLater(function() {
                // Repeater applies a rowsMoved reconciliation after the model
                // signal. A second event-loop turn reads the live delegate
                // order rather than the pre-move itemAt order.
                Qt.callLater(function() {
                    root.updateHoverEffect()
                    root.updateInputRegion()
                })
            })
        }
    }

    Connections {
        target: root.inputRegionBridge
        function onWindowReady() {
            root.scheduleInputRegionUpdate()
        }
    }

    Connections {
        target: DockController
        function onRevealedChanged() {
            root.updateHoverEffect()
            root.scheduleInputRegionUpdate()
        }
        function onSurfacePlacementChanged() {
            root.updateHoverEffect()
            root.scheduleInputRegionUpdate()
        }
    }

    HoverHandler {
        id: hoverHandler
        objectName: "dockHoverHandler"
        onPointChanged: root.updatePointerAtPoint(point.position.x, point.position.y)
        onHoveredChanged: {
            if (hovered)
                root.updatePointerAtPoint(point.position.x, point.position.y)
            else
                root.setPointerInside(false)
        }
    }

    function setPointerInside(inside) {
        pointerInside = inside
        DockController.setPointerInside(inside)
        if (!inside)
            pointerX = 0
        updateHoverEffect()
    }

    // Keep the one-argument helper as a compatibility surface for tests and
    // non-pointer callers. It represents a primary-axis coordinate.
    function updatePointer(primary) {
        if (!isFinite(primary))
            return
        pointerX = primary - primaryExtent() / 2
        pointerInside = true
        DockController.setPointerInside(true)
        updateHoverEffect()
    }

    function updatePointerAtPoint(x, y) {
        if (!isFinite(x) || !isFinite(y))
            return
        pointerX = (vertical ? y : x) - primaryExtent() / 2
        const interactive = isInteractivePoint(Qt.point(x, y))
        pointerInside = interactive
        DockController.setPointerInside(interactive)
        updateHoverEffect()
    }

    function updatePointerFromScene(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return
        const panelPoint = mapFromItem(null, sceneX, sceneY)
        updatePointerAtPoint(panelPoint.x, panelPoint.y)
    }

    function isInteractivePoint(point) {
        if (!point || !isFinite(point.x) || !isFinite(point.y))
            return false

        if (!DockController.revealed && DockController.physicalEdgeReveal) {
            return point.x >= edgeRevealTarget.x && point.x < edgeRevealTarget.x + edgeRevealTarget.width
                && point.y >= edgeRevealTarget.y && point.y < edgeRevealTarget.y + edgeRevealTarget.height
        }

        if (!DockController.revealed)
            return false

        if (point.x >= dockChrome.x && point.x < dockChrome.x + dockChrome.width
            && point.y >= dockChrome.y && point.y < dockChrome.y + dockChrome.height)
            return true

        for (var i = 0; i < appRepeater.count; ++i) {
            const item = appRepeater.itemAt(i)
            if (!item)
                continue
            // Repeater::itemAt is typed as QQuickItem; the delegate's
            // interactionRegion is intentionally read dynamically here.
            const region = item["interaction" + "Region"]
            if (region && isFinite(region.x) && isFinite(region.y)
                && isFinite(region.width) && isFinite(region.height)
                && point.x >= region.x && point.x < region.x + region.width
                && point.y >= region.y && point.y < region.y + region.height)
                return true
        }
        return false
    }

    function scheduleInputRegionUpdate() {
        if (inputRegionBridge)
            Qt.callLater(root.updateInputRegion)
    }

    function windowRect(panelRect) {
        const topLeft = root.mapToItem(null, panelRect.x, panelRect.y)
        const bottomRight = root.mapToItem(null, panelRect.x + panelRect.width,
                                           panelRect.y + panelRect.height)
        return Qt.rect(topLeft.x, topLeft.y, bottomRight.x - topLeft.x,
                       bottomRight.y - topLeft.y)
    }

    function updateInputRegion() {
        if (!inputRegionBridge)
            return

        inputInteractionRects.length = 0
        if (!DockController.revealed && DockController.physicalEdgeReveal) {
            inputRegionBridge.update(windowRect(Qt.rect(edgeRevealTarget.x, edgeRevealTarget.y,
                                                        edgeRevealTarget.width, edgeRevealTarget.height)),
                                     inputInteractionRects, appRepeater.count)
            return
        }
        if (!DockController.revealed) {
            inputRegionBridge.update(Qt.rect(), inputInteractionRects, appRepeater.count)
            return
        }
        for (var i = 0; i < appRepeater.count; ++i) {
            const item = appRepeater.itemAt(i)
            if (!item)
                continue
            const region = item["interaction" + "Region"]
            if (region && isFinite(region.x) && isFinite(region.y)
                && isFinite(region.width) && isFinite(region.height))
                inputInteractionRects.push(windowRect(region))
        }
        const chromeRect = windowRect(Qt.rect(dockChrome.x, dockChrome.y,
                                              dockChrome.width, dockChrome.height))
        inputRegionBridge.update(chromeRect, inputInteractionRects, appRepeater.count)
    }

    function delegateRestingCenterInPanel(item) {
        if (!item || !item.parent)
            return NaN
        const center = item.parent.mapToItem(root,
                                            item.x + item.width / 2,
                                            item.y + item.height / 2)
        return vertical ? center.y : center.x
    }

    function isReordering() {
        return draggedDesktopFileName.length > 0
    }

    function updateHoverEffect() {
        const count = appRepeater.count
        const magnificationActive = pointerInside && configuredHoverEffect === "magnification"
            && !isReordering()
        const liftActive = pointerInside && configuredHoverEffect === "lift" && !isReordering()
        const slotPitch = Math.max(1, root.slotPitch)
        const radius = Math.max(1, DockController.magnificationRadius) * slotPitch
        const maximumScale = Math.max(1, DockController.magnificationScale)
        var totalExtra = 0
        var maximumExtra = 0
        var hoveredIndex = -1
        var closestIndex = -1
        var closestDistance = Number.POSITIVE_INFINITY
        var computedScales = []
        var computedExtras = []
        var computedPrefixes = []
        const localPointerPrimary = pointerX + primaryExtent() / 2

        for (var i = 0; i < count; ++i) {
            const item = appRepeater.itemAt(i)
            if (!item)
                continue

            const itemCenter = delegateRestingCenterInPanel(item)
            const itemStart = itemCenter - (vertical ? item.height : item.width) / 2
            const itemEnd = itemStart + (vertical ? item.height : item.width)
            const slotHovered = pointerInside && localPointerPrimary >= itemStart
                && localPointerPrimary < itemEnd
            if (slotHovered)
                hoveredIndex = i

            var scale = liftActive && slotHovered ? liftScale : 1
            if (magnificationActive) {
                const center = itemCenter - primaryExtent() / 2
                const distance = Math.abs(pointerX - center)
                const t = Math.min(distance / radius, 1)
                const influence = t < 1 ? 0.5 * (1 + Math.cos(Math.PI * t)) : 0
                scale = 1 + (maximumScale - 1) * influence
                if (distance < closestDistance) {
                    closestDistance = distance
                    closestIndex = i
                }
            }
            const extra = magnificationActive ? DockController.iconSize * (scale - 1) : 0
            computedScales[i] = scale
            computedExtras[i] = extra
            computedPrefixes[i] = totalExtra
            magnificationScales[i] = scale
            extraWidths[i] = extra
            prefixExtraWidths[i] = totalExtra
            totalExtra += extra
            maximumExtra = Math.max(maximumExtra, extra)
        }

        const targetIndex = magnificationActive ? closestIndex : hoveredIndex
        const targetItem = targetIndex >= 0 ? appRepeater.itemAt(targetIndex) : null
        pointerTargetDesktopFileName = targetItem ? targetItem.objectName : ""
        magnificationWidth = !vertical && magnificationActive ? totalExtra : 0
        magnificationHeight = vertical && magnificationActive ? totalExtra : 0
        const liftedItem = liftActive && hoveredIndex >= 0 ? appRepeater.itemAt(hoveredIndex) : null
        updateDelegateTransforms(totalExtra, slotPitch,
                                 liftedItem ? liftedItem.objectName : "",
                                 computedScales, computedExtras, computedPrefixes)
        scheduleInputRegionUpdate()
    }

    function previewIndexFor(originalIndex) {
        if (originalIndex === draggedSourceIndex)
            return dragTargetIndex
        if (draggedSourceIndex < dragTargetIndex
            && originalIndex > draggedSourceIndex && originalIndex <= dragTargetIndex)
            return originalIndex - 1
        if (draggedSourceIndex > dragTargetIndex
            && originalIndex >= dragTargetIndex && originalIndex < draggedSourceIndex)
            return originalIndex + 1
        return originalIndex
    }

    function updateDelegateTransforms(totalExtra, slotPitch, liftedKey,
                                      computedScales, computedExtras, computedPrefixes) {
        const count = appRepeater.count
        const dragging = isReordering()
        for (var i = 0; i < count; ++i) {
            const item = appRepeater.itemAt(i)
            if (!item)
                continue

            const extra = computedExtras[i] || 0
            var offset = (computedPrefixes[i] || 0) + extra / 2 - totalExtra / 2
            if (dragging) {
                if (i === draggedSourceIndex) {
                    const currentCenterRelative = delegateRestingCenterInPanel(item)
                        - primaryExtent() / 2
                    offset += dragCenterRelativeX - currentCenterRelative
                } else {
                    offset += (previewIndexFor(i) - i) * slotPitch
                }
            }
            item.magnificationScale = dragging ? 1
                : (computedScales[i] || 1)
            item.visualOffsetX = vertical ? 0 : offset
            item.visualOffsetY = vertical ? offset
                : (dragging && i === draggedSourceIndex ? crossOffset(reorderOffsetY)
                   : (!dragging && item.objectName === liftedKey ? crossOffset(liftOffsetY) : 0))
            if (vertical && dragging && i === draggedSourceIndex)
                item.visualOffsetX = crossOffset(reorderOffsetY)
            else if (vertical)
                item.visualOffsetX = item.objectName === liftedKey ? crossOffset(liftOffsetY) : 0
            item.dragScale = dragging && i === draggedSourceIndex ? reorderScale : 1
            item.dragging = i === draggedSourceIndex
        }
    }

    function beginReorder(key) {
        if (isReordering())
            return

        var source = -1
        for (var i = 0; i < Math.min(DockController.pinCount, appRepeater.count); ++i) {
            const item = appRepeater.itemAt(i)
            if (item && item.objectName === key) {
                source = i
                break
            }
        }
        if (source < 0)
            return

        const sourceItem = appRepeater.itemAt(source)
        const visualCenter = sourceItem.mapToItem(root, sourceItem.width / 2,
                                                  sourceItem.height / 2)
        draggedDesktopFileName = key
        draggedSourceIndex = source
        dragTargetIndex = source
        dragOriginCenterRelativeX = (vertical ? visualCenter.y : visualCenter.x) - primaryExtent() / 2
        dragCenterRelativeX = dragOriginCenterRelativeX
        updateHoverEffect()
    }

    function updateReorder(key, translationX, sceneX, sceneY) {
        if (key !== draggedDesktopFileName)
            return

        updatePointerFromScene(sceneX, sceneY)
        dragCenterRelativeX = dragOriginCenterRelativeX + translationX
        const firstItem = appRepeater.itemAt(0)
        if (!firstItem)
            return
        const firstCenterRelative = delegateRestingCenterInPanel(firstItem) - primaryExtent() / 2
        dragTargetIndex = Math.max(0, Math.min(DockController.pinCount - 1,
                                               Math.round((dragCenterRelativeX
                                                           - firstCenterRelative) / Math.max(1, root.slotPitch))))
        updateHoverEffect()
    }

    function finishReorder(key) {
        if (key !== draggedDesktopFileName)
            return
        const target = dragTargetIndex
        const source = draggedSourceIndex
        draggedDesktopFileName = ""
        draggedSourceIndex = -1
        dragTargetIndex = -1
        dragOriginCenterRelativeX = 0
        dragCenterRelativeX = 0
        pointerTargetDesktopFileName = ""
        updateHoverEffect()
        if (source >= 0 && target >= 0 && source !== target)
            reorderRequested(key, target)
    }

    function cancelReorder(key) {
        if (key !== draggedDesktopFileName)
            return
        draggedDesktopFileName = ""
        draggedSourceIndex = -1
        dragTargetIndex = -1
        dragOriginCenterRelativeX = 0
        dragCenterRelativeX = 0
        updateHoverEffect()
    }

    Component.onCompleted: {
        updateHoverEffect()
        scheduleInputRegionUpdate()
    }
    onWidthChanged: updateHoverEffect()
    onHeightChanged: updateHoverEffect()
    onConfiguredHoverEffectChanged: updateHoverEffect()
}
