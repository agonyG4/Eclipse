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
    readonly property real visualHeadroom: Math.max(magnificationHeight,
        pointerInside && configuredHoverEffect === "lift" && !isReordering()
            ? liftHeadroom : 0,
        isReordering() ? dragHeadroom : 0)
    property real magnificationWidth: 0
    property real magnificationHeight: 0
    // Keep the pointer in the panel's centered coordinate system. The window
    // may resize around this center while magnification is active.
    property real pointerX: 0
    property bool pointerInside: false
    property string pointerTargetDesktopFileName: ""
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
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

    signal reorderRequested(string desktopFileName, int targetPinIndex)

    width: restingWidth + magnificationWidth
    height: restingHeight + visualHeadroom

    Behavior on width {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }
    Behavior on height {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }

    Rectangle {
        id: dockChrome
        objectName: "dockChrome"
        anchors.bottom: parent.bottom
        width: parent.width
        height: root.restingHeight
        radius: 23
        color: "#80343434"
        border.color: "#33FFFFFF"
        border.width: 1

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
            Qt.callLater(root.updateHoverEffect)
        }
    }

    HoverHandler {
        id: hoverHandler
        objectName: "dockHoverHandler"
        onPointChanged: root.updatePointer(point.position.x)
        onHoveredChanged: root.setPointerInside(hovered)
    }

    function setPointerInside(inside) {
        pointerInside = inside
        if (!inside)
            pointerX = 0
        updateHoverEffect()
    }

    function updatePointer(x) {
        pointerX = x - width / 2
        pointerInside = true
        updateHoverEffect()
    }

    function updatePointerFromScene(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return
        var panelPoint = mapFromItem(null, sceneX, sceneY)
        pointerX = panelPoint.x - width / 2
        pointerInside = panelPoint.x >= 0 && panelPoint.x <= width
            && panelPoint.y >= 0 && panelPoint.y <= height
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

            var itemLeft = appRow.x + item.x
            var itemRight = itemLeft + item.width
            var slotHovered = pointerInside && localPointerX >= itemLeft
                && localPointerX < itemRight
            if (slotHovered)
                hoveredIndex = i

            var scale = liftActive && slotHovered ? 1.1 : 1
            if (magnificationActive) {
                var center = itemLeft + item.width / 2 - width / 2
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
                    var currentCenterRelative = appRow.x + item.x + item.width / 2 - width / 2
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
        var firstCenterRelative = appRow.x + firstItem.x + firstItem.width / 2 - width / 2
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

    Component.onCompleted: updateHoverEffect()
    onWidthChanged: updateHoverEffect()
    onHeightChanged: updateHoverEffect()
    onConfiguredHoverEffectChanged: updateHoverEffect()
}
