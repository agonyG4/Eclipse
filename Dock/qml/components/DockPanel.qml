pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    readonly property int restingWidth: appRow.implicitWidth + DockController.panelPadding * 2
    readonly property int restingHeight: DockController.restingHeight
    readonly property int contentWidth: restingWidth
    readonly property int contentHeight: restingHeight
    readonly property int slotPitch: DockController.delegateWidth + DockController.itemSpacing
    property real magnificationWidth: 0
    property real magnificationHeight: 0
    // Keep the pointer in the panel's centered coordinate system. The window
    // may resize around this center while magnification is active.
    property real pointerX: 0
    property bool pointerInside: false
    property string pointerTargetDesktopFileName: ""
    property string draggedDesktopFileName: ""
    property int draggedSourceIndex: -1
    property int dragTargetIndex: -1
    property real dragOriginCenterX: 0
    property real dragCenterX: 0
    property var magnificationScales: []
    property var extraWidths: []
    property var prefixExtraWidths: []
    property var delegateKeys: []

    signal reorderRequested(string desktopFileName, int targetPinIndex)

    width: restingWidth + magnificationWidth
    height: restingHeight + magnificationHeight

    Behavior on width {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }
    Behavior on height {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
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
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            spacing: DockController.itemSpacing

            Repeater {
                id: appRepeater
                model: DockController.appModel
                onCountChanged: root.updateMagnification()

                delegate: DockAppDelegate {
                    dockPanel: root
                    iconSize: DockController.iconSize
                    slotWidth: DockController.delegateWidth
                    slotHeight: DockController.delegateHeight
                    pinned: model.pinned
                    pointerTarget: dockPanel.pointerTargetDesktopFileName === desktopFileName
                    onActivated: DockController.launchByDesktopFileName(desktopFileName)
                    onDragStarted: function(key) { root.beginReorder(key) }
                    onDragMoved: function(key, translationX) {
                        root.updateReorder(key, translationX)
                    }
                    onDragFinished: function(key) { root.finishReorder(key) }
                    onDragCanceled: function(key) { root.cancelReorder(key) }
                }
            }
        }
    }

    HoverHandler {
        id: hoverHandler
        onPointChanged: root.updatePointer(point.position.x)
        onHoveredChanged: root.setPointerInside(hovered)
    }

    function setPointerInside(inside) {
        pointerInside = inside
        if (!inside)
            pointerX = 0
        updateMagnification()
    }

    function updatePointer(x) {
        pointerX = x - width / 2
        pointerInside = true
        updateMagnification()
    }

    function isReordering() {
        return draggedDesktopFileName.length > 0
    }

    function updateMagnification() {
        var count = appRepeater.count
        var active = pointerInside && DockController.magnificationEnabled && !isReordering()
        var slotPitch = Math.max(1, root.slotPitch)
        var radius = Math.max(1, DockController.magnificationRadius) * slotPitch
        var maximumScale = Math.max(1, DockController.magnificationScale)
        var totalExtra = 0
        var maximumExtra = 0
        var closestIndex = -1
        var closestDistance = Number.POSITIVE_INFINITY

        for (var i = 0; i < count; ++i) {
            var item = appRepeater.itemAt(i)
            if (!item)
                continue

            delegateKeys[i] = item.objectName
            var center = appRow.x + item.x + item.width / 2 - width / 2
            var distance = Math.abs(pointerX - center)
            var t = active ? Math.min(distance / radius, 1) : 1
            var influence = t < 1 ? 0.5 * (1 + Math.cos(Math.PI * t)) : 0
            var scale = 1 + (maximumScale - 1) * influence * (active ? 1 : 0)
            var extra = DockController.iconSize * (scale - 1)
            magnificationScales[i] = scale
            extraWidths[i] = extra
            prefixExtraWidths[i] = totalExtra
            totalExtra += extra
            maximumExtra = Math.max(maximumExtra, extra)

            if (active && distance < closestDistance) {
                closestDistance = distance
                closestIndex = i
            }
        }

        pointerTargetDesktopFileName = closestIndex >= 0 ? delegateKeys[closestIndex] : ""
        magnificationWidth = active ? totalExtra : 0
        magnificationHeight = active ? maximumExtra : 0
        updateDelegateTransforms(totalExtra, slotPitch)
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

    function updateDelegateTransforms(totalExtra, slotPitch) {
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
                    var originalCenter = appRow.x + item.x + item.width / 2
                    offset += dragCenterX - originalCenter
                } else {
                    offset += (previewIndexFor(i) - i) * slotPitch
                }
            }
            item.magnificationScale = dragging ? 1 : (magnificationScales[i] || 1)
            item.visualOffsetX = offset
            item.dragging = i === draggedSourceIndex
        }
    }

    function beginReorder(key) {
        if (isReordering())
            return

        var source = -1
        for (var i = 0; i < Math.min(DockController.pinCount, appRepeater.count); ++i) {
            var item = appRepeater.itemAt(i)
            if (item && delegateKeys[i] === key) {
                source = i
                break
            }
        }
        if (source < 0)
            return

        var sourceItem = appRepeater.itemAt(source)
        draggedDesktopFileName = key
        draggedSourceIndex = source
        dragTargetIndex = source
        dragOriginCenterX = appRow.x + sourceItem.x + sourceItem.width / 2
        dragCenterX = dragOriginCenterX
        updateMagnification()
    }

    function updateReorder(key, translationX) {
        if (key !== draggedDesktopFileName)
            return

        dragCenterX = dragOriginCenterX + translationX
        var firstItem = appRepeater.itemAt(0)
        if (!firstItem)
            return
        var slotPitch = Math.max(1, root.slotPitch)
        var firstCenter = appRow.x + firstItem.x + firstItem.width / 2
        dragTargetIndex = Math.max(0, Math.min(DockController.pinCount - 1,
                                               Math.round((dragCenterX - firstCenter) / slotPitch)))
        updateMagnification()
    }

    function finishReorder(key) {
        if (key !== draggedDesktopFileName)
            return
        var target = dragTargetIndex
        var source = draggedSourceIndex
        draggedDesktopFileName = ""
        draggedSourceIndex = -1
        dragTargetIndex = -1
        dragOriginCenterX = 0
        dragCenterX = 0
        pointerTargetDesktopFileName = ""
        updateMagnification()
        if (source >= 0 && target >= 0 && source !== target)
            reorderRequested(key, target)
    }

    function cancelReorder(key) {
        if (key !== draggedDesktopFileName)
            return
        draggedDesktopFileName = ""
        draggedSourceIndex = -1
        dragTargetIndex = -1
        dragOriginCenterX = 0
        dragCenterX = 0
        updateMagnification()
    }

    Component.onCompleted: updateMagnification()
    onWidthChanged: if (pointerInside) updateMagnification()
}
