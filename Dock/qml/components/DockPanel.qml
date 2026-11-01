pragma ComponentBehavior: Bound

import QtQuick
import Astrea.Effects
import Astrea.Shared as Shared

Item {
    id: root

    Shared.ShellMaterialTheme { id: shellMaterial }
    readonly property real dockRadius: shellMaterial.tokens.value("dockRadius", DockController.cornerRadius)

    readonly property bool vertical: DockController.vertical
    readonly property bool growsPositiveCross: DockController.position === "left"
    readonly property var minimizedEntries: DockController.minimizedThumbnailsEnabled
        ? DockController.minimizedWindowEntries : []
    readonly property int minimizedCount: minimizedEntries ? minimizedEntries.length : 0
    readonly property int previewSectionGap: minimizedCount > 0 ? 20 : 0
    readonly property int previewPrimaryExtent: minimizedCount *
        (vertical ? DockController.delegateHeight : DockController.delegateWidth)
        + Math.max(0, minimizedCount - 1) * DockController.itemSpacing
        + previewSectionGap
    readonly property int appPrimaryExtent: vertical ? appRow.implicitHeight : appRow.implicitWidth
    readonly property int restingPrimary: appPrimaryExtent + previewPrimaryExtent
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
    // pointerX remains the relative pointer coordinate for the current primary
    // axis. For Bottom this is the historical centered X coordinate.
    property real pointerX: 0
    property bool pointerInside: false
    property bool previewHovered: false
    property string previewTaskKey: ""
    property string pointerTargetTaskKey: ""
    property int tooltipLifecycleEpoch: 0
    property int tooltipRefreshQueuedEpoch: -1
    property string tooltipRefreshQueuedTaskKey: ""
    readonly property string pointerTargetDesktopFileName:
        pointerTargetTaskKey.startsWith("desktop:")
            ? pointerTargetTaskKey.substring("desktop:".length) : pointerTargetTaskKey
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
    property var dockTooltipSurface: null
    property var dockWindowPreviewSurface: null
    property var dockWindowPreviewService: null
    property var inputRegionBridge: null
    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
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
    property real magnificationExtraPrimary: 0
    property var inputInteractionRects: []

    signal reorderRequested(string desktopFileName, int targetPinIndex)

    function isTooltipEligible() {
        return pointerInside && pointerTargetTaskKey.length > 0
            && DockController.visible && DockController.revealed
            && !isReordering()
            && (!contextMenuController || !contextMenuController.hasActivePresentation)
    }

    width: surfaceWidth
    height: surfaceHeight

    onPointerInsideChanged: root.scheduleTooltip()
    onPointerTargetTaskKeyChanged: root.scheduleTooltip()
    onDraggedDesktopFileNameChanged: root.scheduleTooltip()

    Timer {
        id: tooltipDelay
        objectName: "dockTooltipDelay"
        interval: 420
        repeat: false
        onTriggered: root.presentTooltipForCurrentTarget()
    }

    Timer {
        id: previewDismissDelay
        objectName: "dockPreviewDismissDelay"
        interval: 180
        repeat: false
        onTriggered: {
            if (root.pointerInside || root.previewHovered)
                return
            if (root.dockWindowPreviewSurface)
                root.dockWindowPreviewSurface.hidePreview()
            root.previewTaskKey = ""
            DockController.setPointerInside(false)
        }
    }

    function animationDuration(base) {
        if (!DockController.animationsEnabled)
            return 0
        const themedDuration = shellMaterial.tokens.value("animationFast", undefined)
        if (themedDuration !== undefined)
            return Math.max(0, Math.round(base * themedDuration / 150 / root.animationSpeed))
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
        width: root.vertical
            ? root.restingWidth
            : root.restingWidth + root.magnificationExtraPrimary
        height: root.vertical
            ? root.restingHeight + root.magnificationExtraPrimary
            : root.restingHeight
        radius: root.dockRadius
        color: shellMaterial.background
        border.color: shellMaterial.border
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

        Grid {
            id: appRow
            columns: root.vertical ? 1 : Math.max(1, appRepeater.count)
            rows: root.vertical ? Math.max(1, appRepeater.count) : 1
            rowSpacing: root.vertical ? DockController.itemSpacing : 0
            columnSpacing: root.vertical ? 0 : DockController.itemSpacing
            x: root.vertical ? (dockChrome.width - implicitWidth) / 2
                             : (dockChrome.width - implicitWidth - root.previewPrimaryExtent) / 2
            y: root.vertical ? (dockChrome.height - implicitHeight
                                - root.previewPrimaryExtent) / 2
                             : dockChrome.height - implicitHeight - DockController.chromeBottomMargin
            width: root.vertical ? dockChrome.width : implicitWidth

            Repeater {
                id: appRepeater
                model: DockController.appModel
                onCountChanged: root.updateHoverEffect()
                onItemAdded: {
                    root.updateHoverEffect()
                    root.scheduleMinimizeAnchorPublication()
                    root.reconcileWindowPreviewCache()
                }
                onItemRemoved: function(index, item) {
                    if (item && item.taskKey)
                        DockController.clearMinimizeAnchor(item.taskKey)
                    root.updateHoverEffect()
                    root.scheduleMinimizeAnchorPublication()
                    root.reconcileWindowPreviewCache()
                }

                delegate: DockAppDelegate {
                    dockPanel: root
                    iconSize: DockController.iconSize
                    slotWidth: DockController.delegateWidth
                    slotHeight: DockController.delegateHeight
                    contextMenuController: root.contextMenuController
                    dockSurfaceGeometry: root.dockSurfaceGeometry
                    outputKey: root.outputKey
                    outputOriginX: root.outputOriginX
                    outputOriginY: root.outputOriginY
                    unreadCount: model.unreadCount
                    unreadCountVisible: model.unreadCountVisible
                    pinned: model.pinned
                    pointerTarget: dockPanel.pointerTargetTaskKey === taskKey
                    onActivated: function(key) { DockController.activateOrLaunchTask(key) }
                    onDragStarted: function(key) { root.beginReorder(key) }
                    onDragMoved: function(key, translationX, sceneX, sceneY) {
                        root.updateReorder(key, translationX, sceneX, sceneY)
                    }
                    onDragFinished: function(key) { root.finishReorder(key) }
                    onDragCanceled: function(key) { root.cancelReorder(key) }
                }
            }
        }

        Rectangle {
            id: minimizedDivider
            objectName: "minimizedWindowDivider"
            color: shellMaterial.border
            width: root.vertical ? 20 : 1
            height: root.vertical ? 1 : 20
            x: root.vertical ? (dockChrome.width - width) / 2
                             : appRow.x + appRow.implicitWidth + 9
            y: root.vertical ? appRow.y + appRow.implicitHeight + 9
                             : appRow.y + (appRow.implicitHeight - height) / 2
            visible: root.minimizedCount > 0
        }

        Grid {
            id: minimizedWindowGrid
            objectName: "minimizedWindowGrid"
            columns: root.vertical ? 1 : root.minimizedCount
            rows: root.vertical ? root.minimizedCount : 1
            rowSpacing: DockController.itemSpacing
            columnSpacing: DockController.itemSpacing
            visible: root.minimizedCount > 0
            width: root.vertical ? DockController.delegateWidth : implicitWidth
            height: root.vertical ? implicitHeight : DockController.delegateHeight
            x: root.vertical ? (dockChrome.width - width) / 2
                             : minimizedDivider.x + minimizedDivider.width + 9
            y: root.vertical ? minimizedDivider.y + minimizedDivider.height + 9
                             : appRow.y + (appRow.implicitHeight - height) / 2

            Repeater {
                model: root.minimizedEntries
                delegate: Item {
                    id: minimizedWindowDelegate
                    required property var modelData
                    objectName: "minimized-window-" + modelData.windowId
                    width: DockController.delegateWidth
                    height: DockController.delegateHeight

                    // The card keeps the fixed slot size because it is the published
                    // minimize anchor; the visible frame hugs the fitted window image.
                    Item {
                        id: minimizedWindowCard
                        objectName: "minimized-window-card-" + minimizedWindowDelegate.modelData.windowId
                        anchors.centerIn: parent
                        width: Math.min(68, parent.width - 4)
                        height: Math.min(46, parent.height - 4)

                        Rectangle {
                            id: minimizedWindowFrame
                            anchors.centerIn: parent
                            width: minimizedWindowImage.visible
                                ? Math.ceil(minimizedWindowImage.paintedWidth) + 2 : parent.width
                            height: minimizedWindowImage.visible
                                ? Math.ceil(minimizedWindowImage.paintedHeight) + 2 : parent.height
                            radius: 6
                            color: shellMaterial.tooltipSurface
                            border.color: shellMaterial.tooltipBorder
                        }

                        Image {
                            id: minimizedWindowImage
                            objectName: "minimized-window-image-" + minimizedWindowDelegate.modelData.windowId
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            source: root.dockWindowPreviewService
                                ? root.dockWindowPreviewService.imageUrl(modelData.windowId) : ""
                            visible: status === Image.Ready
                            asynchronous: true
                            // Every capture gets a new revision URL, so caching by URL
                            // only keeps snapshot-driven delegate rebuilds from flashing.
                            cache: true
                            Connections {
                                target: root.dockWindowPreviewService
                                function onImageChanged(windowId) {
                                    if (windowId === minimizedWindowDelegate.modelData.windowId)
                                        minimizedWindowImage.source =
                                            root.dockWindowPreviewService.imageUrl(windowId)
                                }
                                function onGenerationChanged() {
                                    minimizedWindowImage.source =
                                        root.dockWindowPreviewService.imageUrl(
                                            minimizedWindowDelegate.modelData.windowId)
                                    root.dockWindowPreviewService.requestWindowPreview(
                                        minimizedWindowDelegate.modelData.windowId, false)
                                }
                            }
                        }
                        Shared.AstreaAppIcon {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            iconName: minimizedWindowDelegate.modelData.iconName
                            iconPath: minimizedWindowDelegate.modelData.iconPath
                            appName: minimizedWindowDelegate.modelData.displayName
                            visible: !minimizedWindowImage.visible
                        }

                        // macOS badges minimized windows with their app icon.
                        Shared.AstreaAppIcon {
                            objectName: "minimized-window-badge-" + minimizedWindowDelegate.modelData.windowId
                            width: 18
                            height: 18
                            x: minimizedWindowFrame.x + minimizedWindowFrame.width - width + 4
                            y: minimizedWindowFrame.y + minimizedWindowFrame.height - height + 4
                            iconName: minimizedWindowDelegate.modelData.iconName
                            iconPath: minimizedWindowDelegate.modelData.iconPath
                            appName: minimizedWindowDelegate.modelData.displayName
                            visible: minimizedWindowImage.visible
                        }

                        Rectangle {
                            id: minimizedWindowClose
                            objectName: "minimized-window-close-" + minimizedWindowDelegate.modelData.windowId
                            x: minimizedWindowFrame.x + minimizedWindowFrame.width - width
                            y: minimizedWindowFrame.y
                            width: 16
                            height: 16
                            radius: 8
                            color: "#c91b1d25"
                            visible: minimizedWindowHover.hovered

                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: "white"
                                font.pixelSize: 13
                            }
                            TapHandler {
                                id: closeTap
                                acceptedButtons: Qt.LeftButton
                                onTapped: DockController.closeWindow(
                                    minimizedWindowDelegate.modelData.taskKey,
                                    minimizedWindowDelegate.modelData.windowId)
                            }
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: function(eventPoint) {
                            const closePoint = minimizedWindowClose.mapFromItem(
                                minimizedWindowDelegate, eventPoint.position.x,
                                eventPoint.position.y)
                            if (minimizedWindowClose.visible
                                && minimizedWindowClose.contains(closePoint))
                                return
                            DockController.activateWindow(
                                minimizedWindowDelegate.modelData.taskKey,
                                minimizedWindowDelegate.modelData.windowId)
                        }
                    }
                    HoverHandler {
                        id: minimizedWindowHover
                        onHoveredChanged: {
                            if (hovered && root.dockTooltipSurface && root.dockSurfaceGeometry) {
                                const targetRect = root.windowRect(Qt.rect(
                                    dockChrome.x + minimizedWindowGrid.x + minimizedWindowDelegate.x,
                                    dockChrome.y + minimizedWindowGrid.y + minimizedWindowDelegate.y,
                                    minimizedWindowDelegate.width,
                                    minimizedWindowDelegate.height))
                                const outputRect = root.dockSurfaceGeometry.outputLocalDelegateRect(
                                    root.outputWidth, root.outputHeight, root.surfaceWidth,
                                    root.surfaceHeight, DockController.position,
                                    DockController.layerShellEdgeMargin, targetRect)
                                root.dockTooltipSurface.showTooltip(
                                    minimizedWindowDelegate.modelData.title ||
                                        minimizedWindowDelegate.modelData.displayName,
                                    outputRect, DockController.position)
                            } else if (!hovered && root.dockTooltipSurface
                                       && !root.pointerInside) {
                                root.dockTooltipSurface.hideTooltip()
                            }
                        }
                    }
                    Component.onCompleted: {
                        if (root.dockWindowPreviewService)
                            root.dockWindowPreviewService.requestWindowPreview(
                                modelData.windowId, false)
                    }
                }
            }
        }
    }

    BackdropEffectRegions {
        id: backdropRegions
        anchors.fill: parent
        enabled: shellMaterial.isFrosted

        BackdropRegion {
            item: dockChrome
            enabled: dockChrome.visible
            radius: dockChrome.radius
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
                    root.scheduleMinimizeAnchorPublication()
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
        function onConfigChanged() {
            if (!DockController.windowPreviewsEnabled && root.dockWindowPreviewSurface) {
                root.dockWindowPreviewSurface.hidePreview()
                root.previewTaskKey = ""
            }
            root.scheduleInputRegionUpdate()
            Qt.callLater(root.reconcileWindowPreviewCache)
            root.scheduleMinimizeAnchorPublication()
            root.scheduleTooltip()
        }
        function onModelChanged() {
            root.scheduleMinimizeAnchorPublication()
            root.refreshVisibleTooltip()
            Qt.callLater(root.reconcileWindowPreviewCache)
        }
        function onVisibleChanged() { root.scheduleTooltip() }
        function onRevealedChanged() {
            root.updateHoverEffect()
            root.scheduleInputRegionUpdate()
            root.scheduleTooltip()
        }
        function onSurfacePlacementChanged() {
            root.updateHoverEffect()
            root.scheduleInputRegionUpdate()
            root.scheduleTooltip()
        }
    }

    Connections {
        target: root.contextMenuController
        function onPresentationChanged() { root.scheduleTooltip() }
    }

    onOutputKeyChanged: root.scheduleMinimizeAnchorPublication()
    onOutputWidthChanged: {
        root.scheduleMinimizeAnchorPublication()
        root.refreshVisibleTooltip()
    }
    onOutputHeightChanged: {
        root.scheduleMinimizeAnchorPublication()
        root.refreshVisibleTooltip()
    }
    onOutputOriginXChanged: root.scheduleMinimizeAnchorPublication()
    onOutputOriginYChanged: root.scheduleMinimizeAnchorPublication()

    function scheduleMinimizeAnchorPublication() {
        Qt.callLater(root.publishMinimizeAnchors)
    }

    function thumbnailAnchorRectForSlot(slotIndex, thumbnailCount) {
        const previewExtent = thumbnailCount > 0
            ? thumbnailCount * (root.vertical ? DockController.delegateHeight
                                               : DockController.delegateWidth)
                + Math.max(0, thumbnailCount - 1) * DockController.itemSpacing + 20
            : 0
        const primaryExtent = root.appPrimaryExtent + previewExtent
            + DockController.panelPadding * 2
        const restingWidth = root.vertical ? root.restingCross : primaryExtent
        const restingHeight = root.vertical ? primaryExtent : root.restingCross
        const maximumMagnificationExtra = root.maximumMagnificationExtraPrimary
        const surfaceWidth = root.vertical
            ? Math.max(restingWidth + root.surfaceCrossInset,
                       Math.ceil(restingWidth + root.surfaceHeadroom + root.surfaceCrossInset))
            : Math.max(restingWidth, Math.ceil(restingWidth + maximumMagnificationExtra))
        const surfaceHeight = root.vertical
            ? Math.max(restingHeight, Math.ceil(restingHeight + maximumMagnificationExtra))
            : Math.max(restingHeight + root.surfaceCrossInset,
                       Math.ceil(restingHeight + root.surfaceHeadroom + root.surfaceCrossInset))

        const chromeX = root.vertical
            ? (DockController.position === "left" ? root.surfaceCrossInset
                : surfaceWidth - restingWidth - root.surfaceCrossInset)
            : (surfaceWidth - restingWidth) / 2
        const chromeY = root.vertical
            ? (surfaceHeight - restingHeight) / 2
            : surfaceHeight - restingHeight - root.surfaceCrossInset
        const appRowX = root.vertical
            ? (restingWidth - DockController.delegateWidth) / 2
            : (restingWidth - root.appPrimaryExtent - previewExtent) / 2
        const appRowY = root.vertical
            ? (restingHeight - root.appPrimaryExtent - previewExtent) / 2
            : restingHeight - DockController.delegateHeight - DockController.chromeBottomMargin
        const gridX = root.vertical
            ? (restingWidth - DockController.delegateWidth) / 2
            : appRowX + root.appPrimaryExtent + 19
        const gridY = root.vertical
            ? appRowY + root.appPrimaryExtent + 19 : appRowY
        const delegateX = root.vertical ? gridX : gridX + slotIndex * root.slotPitch
        const delegateY = root.vertical ? gridY + slotIndex * root.slotPitch : gridY
        const thumbnailWidth = Math.min(68, DockController.delegateWidth - 4)
        const thumbnailHeight = Math.min(46, DockController.delegateHeight - 4)
        const thumbnailRect = Qt.rect(
            chromeX + delegateX + (DockController.delegateWidth - thumbnailWidth) / 2,
            chromeY + delegateY + (DockController.delegateHeight - thumbnailHeight) / 2,
            thumbnailWidth, thumbnailHeight)
        const outputLocal = root.dockSurfaceGeometry.outputLocalDelegateRect(
            root.outputWidth, root.outputHeight, surfaceWidth, surfaceHeight,
            DockController.position, DockController.layerShellEdgeMargin, thumbnailRect)
        return Qt.rect(outputLocal.x + root.outputOriginX,
                       outputLocal.y + root.outputOriginY,
                       outputLocal.width, outputLocal.height)
    }

    function publishMinimizeAnchors() {
        if (!root.dockSurfaceGeometry || root.outputKey === "")
            return
        for (var index = 0; index < appRepeater.count; ++index) {
            const item = appRepeater.itemAt(index)
            if (!item)
                continue
            const iconRect = root.dockSurfaceGeometry.restingIconRectInGlobal(
                root.outputWidth, root.outputHeight, root.surfaceWidth, root.surfaceHeight,
                root.outputOriginX, root.outputOriginY, DockController.position,
                DockController.layerShellEdgeMargin, root.surfaceCrossInset, DockController.iconSize,
                DockController.delegateWidth, DockController.delegateHeight,
                DockController.itemSpacing, DockController.panelPadding, index, appRepeater.count)
            const rect = Qt.rect(iconRect.x - (root.vertical ? 0 : root.previewPrimaryExtent / 2),
                iconRect.y - (root.vertical ? root.previewPrimaryExtent / 2 : 0),
                iconRect.width, iconRect.height)
            DockController.setMinimizeAnchor(item.taskKey, rect)

            if (!DockController.minimizedThumbnailsEnabled)
                continue
            const windows = DockController.windowPreviewEntries(item.taskKey)
            for (var windowIndex = 0; windowIndex < windows.length; ++windowIndex) {
                const windowEntry = windows[windowIndex]
                var slotIndex = root.minimizedCount
                for (var minimizedIndex = 0; minimizedIndex < root.minimizedCount;
                     ++minimizedIndex) {
                    if (root.minimizedEntries[minimizedIndex].windowId === windowEntry.windowId) {
                        slotIndex = minimizedIndex
                        break
                    }
                }
                const thumbnailCount = windowEntry.minimized
                    ? root.minimizedCount : root.minimizedCount + 1
                DockController.setWindowMinimizeAnchor(
                    item.taskKey, windowEntry.windowId,
                    root.thumbnailAnchorRectForSlot(slotIndex, thumbnailCount))
            }
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
        DockController.setPointerInside(inside || previewHovered
            || (dockWindowPreviewSurface && dockWindowPreviewSurface.previewVisible))
        if (!inside)
            pointerX = 0
        if (inside) {
            previewDismissDelay.stop()
        } else if (previewHovered) {
            previewDismissDelay.stop()
        } else if (dockWindowPreviewSurface && dockWindowPreviewSurface.previewVisible) {
            previewDismissDelay.start()
        }
        updateHoverEffect()
    }

    function setPreviewHovered(hovered) {
        previewHovered = hovered
        DockController.setPointerInside(pointerInside || previewHovered
            || (dockWindowPreviewSurface && dockWindowPreviewSurface.previewVisible))
        if (hovered) {
            previewDismissDelay.stop()
        } else if (!pointerInside && dockWindowPreviewSurface
                   && dockWindowPreviewSurface.previewVisible) {
            previewDismissDelay.start()
        }
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

    function scheduleTooltip() {
        ++tooltipLifecycleEpoch
        tooltipDelay.stop()
        if (dockTooltipSurface)
            dockTooltipSurface.hideTooltip()
        if (dockWindowPreviewSurface && dockWindowPreviewSurface.previewVisible) {
            if (isTooltipEligible())
                tooltipDelay.start()
            else if (!previewHovered)
                previewDismissDelay.start()
            return
        }
        if (isTooltipEligible())
            tooltipDelay.start()
    }

    function refreshTooltipAnchor(taskKey) {
        if (taskKey !== pointerTargetTaskKey)
            return
        refreshVisibleTooltip()
    }

    function refreshVisibleTooltip() {
        if (!dockTooltipSurface || !dockTooltipSurface.tooltipVisible || !isTooltipEligible())
            return

        const refreshEpoch = tooltipLifecycleEpoch
        const refreshTaskKey = pointerTargetTaskKey
        if (tooltipRefreshQueuedEpoch === refreshEpoch
            && tooltipRefreshQueuedTaskKey === refreshTaskKey)
            return

        tooltipRefreshQueuedEpoch = refreshEpoch
        tooltipRefreshQueuedTaskKey = refreshTaskKey
        Qt.callLater(function() {
            if (root.tooltipRefreshQueuedEpoch === refreshEpoch
                && root.tooltipRefreshQueuedTaskKey === refreshTaskKey) {
                root.tooltipRefreshQueuedEpoch = -1
                root.tooltipRefreshQueuedTaskKey = ""
            }

            if (root.tooltipLifecycleEpoch !== refreshEpoch
                || root.pointerTargetTaskKey !== refreshTaskKey
                || !root.dockTooltipSurface
                || !root.dockTooltipSurface.tooltipVisible
                || !root.isTooltipEligible())
                return

            root.presentTooltipForCurrentTarget()
        })
    }

    function presentTooltipForCurrentTarget() {
        if (!isTooltipEligible() || !dockTooltipSurface || !dockSurfaceGeometry) {
            if (dockTooltipSurface)
                dockTooltipSurface.hideTooltip()
            if (dockWindowPreviewSurface && !previewHovered)
                dockWindowPreviewSurface.hidePreview()
            return
        }

        var targetItem = null
        for (var i = 0; i < appRepeater.count; ++i) {
            const item = appRepeater.itemAt(i)
            if (item && item.taskKey === pointerTargetTaskKey) {
                targetItem = item
                break
            }
        }
        if (!targetItem) {
            dockTooltipSurface.hideTooltip()
            return
        }

        const region = targetItem.interactionRegion
        if (!region || !isFinite(region.x) || !isFinite(region.y)
            || !isFinite(region.width) || !isFinite(region.height)
            || region.width <= 0 || region.height <= 0) {
            dockTooltipSurface.hideTooltip()
            return
        }

        const windowRegion = windowRect(region)
        const outputRegion = dockSurfaceGeometry.outputLocalDelegateRect(
            outputWidth, outputHeight, surfaceWidth, surfaceHeight,
            DockController.position, DockController.layerShellEdgeMargin, windowRegion)
        const windows = DockController.windowPreviewEntries(targetItem.taskKey)
        if (DockController.windowPreviewsEnabled && windows.length >= 2 && dockWindowPreviewSurface) {
            previewDismissDelay.stop()
            previewTaskKey = targetItem.taskKey
            dockTooltipSurface.hideTooltip()
            dockWindowPreviewSurface.showPreview(targetItem.taskKey, windows, outputRegion,
                                                  DockController.position,
                                                  outputWidth, outputHeight,
                                                  outputOriginX, outputOriginY)
            return
        }
        if (dockWindowPreviewSurface)
            dockWindowPreviewSurface.hidePreview()
        previewTaskKey = ""
        dockTooltipSurface.showTooltip(targetItem.displayName, outputRegion,
                                       DockController.position)
    }

    function reconcileWindowPreviewCache() {
        if (!dockWindowPreviewService)
            return
        var ids = []
        for (var row = 0; row < appRepeater.count; ++row) {
            const item = appRepeater.itemAt(row)
            if (!item)
                continue
            const windows = DockController.windowPreviewEntries(item.taskKey)
            for (var index = 0; index < windows.length; ++index)
                ids.push(windows[index].windowId)
        }
        dockWindowPreviewService.reconcileLiveWindowIds(ids)
        var minimizedIds = []
        for (var minIndex = 0; minIndex < minimizedCount; ++minIndex)
            minimizedIds.push(minimizedEntries[minIndex].windowId)
        dockWindowPreviewService.setMinimizedWindowIds(minimizedIds)
        if (dockWindowPreviewSurface && dockWindowPreviewSurface.previewVisible) {
            const liveEntries = DockController.windowPreviewEntries(previewTaskKey)
            if (!DockController.windowPreviewsEnabled || liveEntries.length < 2) {
                dockWindowPreviewSurface.hidePreview()
                previewTaskKey = ""
            } else {
                dockWindowPreviewSurface.showPreview(previewTaskKey, liveEntries,
                    dockWindowPreviewSurface.anchorRect, DockController.position,
                    outputWidth, outputHeight, outputOriginX, outputOriginY)
            }
        }
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

        magnificationExtraPrimary = magnificationActive ? totalExtra : 0
        const targetIndex = magnificationActive ? closestIndex : hoveredIndex
        const targetItem = targetIndex >= 0 ? appRepeater.itemAt(targetIndex) : null
        pointerTargetTaskKey = targetItem ? targetItem.taskKey : ""
        const liftedItem = liftActive && hoveredIndex >= 0 ? appRepeater.itemAt(hoveredIndex) : null
        updateDelegateTransforms(totalExtra, slotPitch,
                                 liftedItem ? liftedItem.objectName : "",
                                 computedScales, computedExtras, computedPrefixes)
        scheduleInputRegionUpdate()
        refreshVisibleTooltip()
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
            if (item && item.desktopFileName === key) {
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
        pointerTargetTaskKey = ""
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
        reconcileWindowPreviewCache()
    }
    onWidthChanged: {
        updateHoverEffect()
        scheduleMinimizeAnchorPublication()
    }
    onHeightChanged: {
        updateHoverEffect()
        scheduleMinimizeAnchorPublication()
    }
    onConfiguredHoverEffectChanged: updateHoverEffect()
}
