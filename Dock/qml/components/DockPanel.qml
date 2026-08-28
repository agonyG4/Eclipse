pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    readonly property int restingWidth: appRow.implicitWidth + DockController.panelPadding * 2
    readonly property int restingHeight: DockController.restingHeight
    readonly property int contentWidth: restingWidth
    readonly property int contentHeight: restingHeight
    readonly property int slotPitch: DockController.delegateWidth + DockController.itemSpacing
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
    readonly property int surfaceWidth: Math.max(restingWidth,
        Math.ceil(restingWidth + maximumMagnificationExtraWidth))
    readonly property int surfaceHeight: Math.max(restingHeight,
        Math.ceil(restingHeight + surfaceHeadroom))
    // Kept as a read-only compatibility property. The envelope reserves this
    // headroom structurally instead of resizing as the pointer moves.
    readonly property real visualHeadroom: surfaceHeadroom
    property real magnificationWidth: 0
    property real magnificationHeight: 0
    // Keep the pointer in the panel's centered coordinate system. The panel
    // and its QQuickWindow remain fixed during hover.
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
    // Both values are relative to the panel center. This keeps the drag
    // coordinate invariant while a centered visual surface changes width.
    property real dragOriginCenterRelativeX: 0
    property real dragCenterRelativeX: 0
    property var magnificationScales: []
    property var extraWidths: []
    property var prefixExtraWidths: []
    property var inputInteractionRects: []

    signal reorderRequested(string desktopFileName, int targetPinIndex)

    width: surfaceWidth
    height: surfaceHeight

    Rectangle {
        id: dockChrome
        objectName: "dockChrome"
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.restingWidth + root.magnificationWidth
        height: root.restingHeight
        radius: 23
        color: "#80343434"
        border.color: "#33FFFFFF"
        border.width: 1

        Behavior on width {
            NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
        }
        onWidthChanged: root.scheduleInputRegionUpdate()
        onXChanged: root.scheduleInputRegionUpdate()

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#10000000"
        }

        Row {
            id: appRow
            anchors.horizontalCenter: dockChrome.horizontalCenter
            anchors.bottom: dockChrome.bottom
            anchors.bottomMargin: DockController.chromeBottomMargin
            spacing: DockController.itemSpacing

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
        if (!inside) {
            pointerX = 0
        }
        updateHoverEffect()
    }

    // Keep the one-argument helper as a small compatibility surface for tests
    // and non-pointer callers. Real pointer events use the semantic
    // two-coordinate helper below so transparent visual headroom remains
    // outside the Dock boundary.
    function updatePointer(x) {
        if (!isFinite(x))
            return
        pointerX = x - width / 2
        pointerInside = true
        updateHoverEffect()
    }

    function updatePointerAtPoint(x, y) {
        if (!isFinite(x) || !isFinite(y))
            return
        pointerX = x - width / 2
        pointerInside = isInteractivePoint(Qt.point(x, y))
        updateHoverEffect()
    }

    function updatePointerFromScene(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return
        var panelPoint = mapFromItem(null, sceneX, sceneY)
        updatePointerAtPoint(panelPoint.x, panelPoint.y)
    }

    function isInteractivePoint(point) {
        if (!point || !isFinite(point.x) || !isFinite(point.y))
            return false

        const chromeLeft = dockChrome.x
        const chromeTop = dockChrome.y
        if (point.x >= chromeLeft && point.x < chromeLeft + dockChrome.width
            && point.y >= chromeTop && point.y < chromeTop + dockChrome.height)
            return true

        for (var i = 0; i < appRepeater.count; ++i) {
            var item = appRepeater.itemAt(i)
            if (!item)
                continue
            // Repeater::itemAt is typed as QQuickItem; the delegate's
            // interactionRegion is intentionally read dynamically here.
            var region = item["interaction" + "Region"]
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
        for (var i = 0; i < appRepeater.count; ++i) {
            var item = appRepeater.itemAt(i)
            if (!item)
                continue
            var region = item["interaction" + "Region"]
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
        return center.x
    }

    function isReordering() {
        return draggedDesktopFileName.length > 0
    }

    function updateHoverEffect() {
        var count = appRepeater.count
        var magnificationActive = pointerInside && configuredHoverEffect === "magnification"
            && !isReordering()
        var liftActive = pointerInside && configuredHoverEffect === "lift" && !isReordering()
        var slotPitch = Math.max(1, root.slotPitch)
        var radius = Math.max(1, DockController.magnificationRadius) * slotPitch
        var maximumScale = Math.max(1, DockController.magnificationScale)
        var totalExtra = 0
        var maximumExtra = 0
        var hoveredIndex = -1
        var closestIndex = -1
        var closestDistance = Number.POSITIVE_INFINITY
        var localPointerX = pointerX + width / 2

        for (var i = 0; i < count; ++i) {
            var item = appRepeater.itemAt(i)
            if (!item)
                continue

            var itemCenter = delegateRestingCenterInPanel(item)
            var itemLeft = itemCenter - item.width / 2
            var itemRight = itemLeft + item.width
            var slotHovered = pointerInside && localPointerX >= itemLeft
                && localPointerX < itemRight
            if (slotHovered)
                hoveredIndex = i

            var scale = liftActive && slotHovered ? 1.1 : 1
            if (magnificationActive) {
                var center = itemCenter - width / 2
                var distance = Math.abs(pointerX - center)
                var t = Math.min(distance / radius, 1)
                var influence = t < 1 ? 0.5 * (1 + Math.cos(Math.PI * t)) : 0
                scale = 1 + (maximumScale - 1) * influence
                if (distance < closestDistance) {
                    closestDistance = distance
                    closestIndex = i
                }
            }
            var extra = magnificationActive ? DockController.iconSize * (scale - 1) : 0
            magnificationScales[i] = scale
            extraWidths[i] = extra
            prefixExtraWidths[i] = totalExtra
            totalExtra += extra
            maximumExtra = Math.max(maximumExtra, extra)

        }

        var targetIndex = magnificationActive ? closestIndex : hoveredIndex
        var targetItem = targetIndex >= 0 ? appRepeater.itemAt(targetIndex) : null
        pointerTargetDesktopFileName = targetItem ? targetItem.objectName : ""
        magnificationWidth = magnificationActive ? totalExtra : 0
        magnificationHeight = magnificationActive ? maximumExtra : 0
        var liftedItem = liftActive && hoveredIndex >= 0 ? appRepeater.itemAt(hoveredIndex) : null
        updateDelegateTransforms(totalExtra, slotPitch,
                                 liftedItem ? liftedItem.objectName : "")
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

    function updateDelegateTransforms(totalExtra, slotPitch, liftedKey) {
        var count = appRepeater.count
        var dragging = isReordering()
        for (var i = 0; i < count; ++i) {
            var item = appRepeater.itemAt(i)
            if (!item)
                continue

            var extra = extraWidths[i] || 0
            var offset = (prefixExtraWidths[i] || 0) + extra / 2 - totalExtra / 2
            if (dragging) {
                if (i === draggedSourceIndex) {
                    var currentCenterRelative = delegateRestingCenterInPanel(item) - width / 2
                    offset += dragCenterRelativeX - currentCenterRelative
                } else {
                    offset += (previewIndexFor(i) - i) * slotPitch
                }
            }
            item.magnificationScale = dragging ? 1 : (magnificationScales[i] || 1)
            item.visualOffsetX = offset
            item.visualOffsetY = dragging && i === draggedSourceIndex ? -reorderOffsetY
                : (!dragging && item.objectName === liftedKey ? -liftOffsetY : 0)
            item.dragScale = dragging && i === draggedSourceIndex ? reorderScale : 1
            item.dragging = i === draggedSourceIndex
        }
    }

    function beginReorder(key) {
        if (isReordering())
            return

        var source = -1
        for (var i = 0; i < Math.min(DockController.pinCount, appRepeater.count); ++i) {
            var item = appRepeater.itemAt(i)
            if (item && item.objectName === key) {
                source = i
                break
            }
        }
        if (source < 0)
            return

        var sourceItem = appRepeater.itemAt(source)
        var visualCenter = sourceItem.mapToItem(root, sourceItem.width / 2,
                                               sourceItem.height / 2)
        draggedDesktopFileName = key
        draggedSourceIndex = source
        dragTargetIndex = source
        dragOriginCenterRelativeX = visualCenter.x - width / 2
        dragCenterRelativeX = dragOriginCenterRelativeX
        updateHoverEffect()
    }

    function updateReorder(key, translationX, sceneX, sceneY) {
        if (key !== draggedDesktopFileName)
            return

        updatePointerFromScene(sceneX, sceneY)
        dragCenterRelativeX = dragOriginCenterRelativeX + translationX
        var firstItem = appRepeater.itemAt(0)
        if (!firstItem)
            return
        var slotPitch = Math.max(1, root.slotPitch)
        var firstCenterRelative = delegateRestingCenterInPanel(firstItem) - width / 2
        dragTargetIndex = Math.max(0, Math.min(DockController.pinCount - 1,
                                               Math.round((dragCenterRelativeX
                                                           - firstCenterRelative) / slotPitch)))
        updateHoverEffect()
    }

    function finishReorder(key) {
        if (key !== draggedDesktopFileName)
            return
        var target = dragTargetIndex
        var source = draggedSourceIndex
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
