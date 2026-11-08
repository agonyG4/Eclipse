pragma ComponentBehavior: Bound

import QtQuick
import Astrea.Effects
import Astrea.Shared as Shared

Item {
    id: root

    Shared.ShellMaterialTheme {
        typographySurface: "dock"
        id: shellMaterial
    }
    readonly property real dockRadius: shellMaterial.tokens.value("dockRadius", DockController.cornerRadius)
    readonly property bool lightMaterial: shellMaterial.isLight

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
    // Launch bounce height matches DockAppDelegate.bounceHeight.
    readonly property real bounceHeadroom: Math.max(0,
        Math.round(DockController.iconSize * 0.42) - iconRestingTop)
    readonly property real surfaceHeadroom: Math.max(maxMagnificationHeadroom,
        liftHeadroom, dragHeadroom, bounceHeadroom)
    readonly property int maximumMagnifiedDelegateCount: Math.min(appRepeater.count + minimizedCount,
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
    // Minimized tile reorder. Only the primary-axis component of the pointer
    // reorders; the panel-space pointer is kept for the cross-axis drag that
    // will place a window back on the desktop.
    property string minimizedDragWindowId: ""
    property int minimizedDragSourceIndex: -1
    property int minimizedDragTargetIndex: -1
    property real minimizedDragPointerOriginPrimary: 0
    property real minimizedDragOriginCenterRelativeX: 0
    property real minimizedDragCenterRelativeX: 0
    property point minimizedDragPanelPosition: Qt.point(0, 0)
    property point minimizedDragScenePosition: Qt.point(0, 0)
    property point minimizedDragPointerOriginScene: Qt.point(0, 0)
    // Pulling a minimized tile away from the Dock tears it out: the window
    // grows from the tile to its real size under the pointer, and dropping it
    // restores the window right there, macOS style.
    property bool minimizedTearOut: false
    property point minimizedGhostGrip: Qt.point(0.5, 0.5)
    property rect minimizedGhostTileRect: Qt.rect(0, 0, 1, 1)
    property size minimizedGhostWindowSize: Qt.size(1, 1)
    // Keeps a dropped window's tile hidden until the runtime removes it.
    property string minimizedRestoringWindowId: ""
    // Keeps a tile hidden while its ghost slides back into the slot.
    property string minimizedReturningWindowId: ""
    readonly property real tearOutThreshold: 30
    readonly property real tearOutGrowDistance: 200
    readonly property real tearOutWindowRadius: 10
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
    onMinimizedDragWindowIdChanged: root.scheduleTooltip()

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

    // Auto-hide slides the Dock out past the screen edge and back, like
    // macOS, instead of popping it in and out. revealProgress is 1 at rest.
    property real revealProgress: DockController.revealed ? 1 : 0
    Behavior on revealProgress {
        NumberAnimation {
            id: revealAnimation
            duration: root.animationDuration(DockController.revealed ? 260 : 200)
            easing.type: DockController.revealed ? Easing.OutCubic : Easing.InCubic
        }
    }
    onRevealProgressChanged: {
        if (revealProgress === 0 || revealProgress === 1)
            root.scheduleInputRegionUpdate()
    }
    // Far enough to carry the chrome and its shadow fully past the edge.
    readonly property real revealTravel: (vertical ? dockChrome.width : dockChrome.height)
        + surfaceCrossInset + 16
    readonly property real revealOffset: (1 - revealProgress) * revealTravel
    // The chrome's resting position; input and hit testing always use it.
    readonly property real chromeRestX: vertical
        ? (DockController.position === "left" ? surfaceCrossInset
                                              : width - dockChrome.width - surfaceCrossInset)
        : (width - dockChrome.width) / 2
    readonly property real chromeRestY: vertical
        ? (height - dockChrome.height) / 2
        : height - dockChrome.height - surfaceCrossInset

    Rectangle {
        id: dockChrome
        objectName: "dockChrome"
        x: root.vertical
           ? root.chromeRestX + (DockController.position === "left" ? -root.revealOffset
                                                                     : root.revealOffset)
           : root.chromeRestX
        y: root.vertical ? root.chromeRestY : root.chromeRestY + root.revealOffset
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
        visible: root.revealProgress > 0

        // Magnification tracks the pointer tightly; config and app-count
        // changes glide at the same pace as the icons resizing inside.
        readonly property int resizeDuration: root.animationDuration(root.pointerInside ? 90 : 220)
        Behavior on width {
            NumberAnimation { duration: dockChrome.resizeDuration; easing.type: Easing.OutCubic }
        }
        Behavior on height {
            NumberAnimation { duration: dockChrome.resizeDuration; easing.type: Easing.OutCubic }
        }
        Behavior on radius {
            NumberAnimation { duration: root.animationDuration(220); easing.type: Easing.OutCubic }
        }
        onWidthChanged: root.scheduleInputRegionUpdate()
        onHeightChanged: root.scheduleInputRegionUpdate()
        // A slide moves x/y every frame; the region settles once it ends.
        onXChanged: if (!revealAnimation.running) root.scheduleInputRegionUpdate()
        onYChanged: if (!revealAnimation.running) root.scheduleInputRegionUpdate()

        Grid {
            id: appRow
            columns: root.vertical ? 1 : Math.max(1, appRepeater.count)
            rows: root.vertical ? Math.max(1, appRepeater.count) : 1
            rowSpacing: root.vertical ? DockController.itemSpacing : 0
            columnSpacing: root.vertical ? 0 : DockController.itemSpacing
            Behavior on rowSpacing {
                NumberAnimation { duration: root.animationDuration(220); easing.type: Easing.OutCubic }
            }
            Behavior on columnSpacing {
                NumberAnimation { duration: root.animationDuration(220); easing.type: Easing.OutCubic }
            }
            // Icons joining the Dock pop in. No move transition: hover and
            // reorder geometry read resting slot positions right after a move.
            add: Transition {
                enabled: root.animationDuration(220) > 0
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.animationDuration(220); easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.4; to: 1; duration: root.animationDuration(320); easing.type: Easing.OutBack }
            }
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
            color: shellMaterial.separator
            width: root.vertical ? Math.round(DockController.iconSize * 0.75) : 1
            height: root.vertical ? 1 : Math.round(DockController.iconSize * 0.75)
            property real visualOffset: 0
            transform: Translate {
                x: root.vertical ? 0 : minimizedDivider.visualOffset
                y: root.vertical ? minimizedDivider.visualOffset : 0
            }
            Behavior on visualOffset {
                NumberAnimation { duration: root.animationDuration(110); easing.type: Easing.OutCubic }
            }
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
                id: minimizedRepeater
                model: root.minimizedEntries
                onItemAdded: root.updateHoverEffect()
                onItemRemoved: root.updateHoverEffect()

                // A minimized window is a regular Dock tile, like macOS: the
                // window image fitted into the icon square, badged with its app
                // icon, magnified with its neighbours, with no frame or controls.
                delegate: Item {
                    id: minimizedWindowDelegate
                    required property var modelData
                    objectName: "minimized-window-" + modelData.windowId
                    readonly property string hoverKey: "minimized:" + modelData.windowId
                    readonly property Item tile: minimizedWindowCard
                    readonly property Item windowTile: minimizedWindowTile
                    property bool tornOut: false
                    readonly property bool hiddenForWindowGhost: tornOut
                        || root.minimizedRestoringWindowId === modelData.windowId
                        || root.minimizedReturningWindowId === modelData.windowId
                    opacity: hiddenForWindowGhost ? 0 : 1
                    Behavior on opacity {
                        NumberAnimation { duration: root.animationDuration(120); easing.type: Easing.OutCubic }
                    }
                    property real magnificationScale: 1.0
                    property real visualOffsetX: 0
                    property real visualOffsetY: 0
                    property bool dragging: false
                    property real dragScale: 1.0
                    readonly property real visualScale: magnificationScale * dragScale
                    width: DockController.delegateWidth
                    height: DockController.delegateHeight
                    z: dragging ? 100 : magnificationScale

                    transform: Translate {
                        x: minimizedWindowDelegate.visualOffsetX
                        y: minimizedWindowDelegate.visualOffsetY
                    }
                    // A dragged tile follows the pointer rather than animating to it.
                    Behavior on visualOffsetX {
                        enabled: !minimizedWindowDelegate.dragging
                        NumberAnimation { duration: root.animationDuration(110); easing.type: Easing.OutCubic }
                    }
                    Behavior on visualOffsetY {
                        enabled: !minimizedWindowDelegate.dragging
                        NumberAnimation { duration: root.animationDuration(110); easing.type: Easing.OutCubic }
                    }
                    Behavior on magnificationScale {
                        NumberAnimation { duration: root.animationDuration(110); easing.type: Easing.OutCubic }
                    }
                    Behavior on dragScale {
                        NumberAnimation { duration: root.animationDuration(110); easing.type: Easing.OutCubic }
                    }
                    onMagnificationScaleChanged: root.scheduleInputRegionUpdate()
                    onDragScaleChanged: root.scheduleInputRegionUpdate()
                    onVisualOffsetXChanged: root.scheduleInputRegionUpdate()
                    onVisualOffsetYChanged: root.scheduleInputRegionUpdate()

                    // Same geometry as an app icon; it is also the published
                    // minimize anchor, so the lamp lands exactly in the tile.
                    Item {
                        id: minimizedWindowCard
                        objectName: "minimized-window-card-" + minimizedWindowDelegate.modelData.windowId
                        anchors.centerIn: parent
                        width: DockController.iconSize
                        height: DockController.iconSize
                        scale: minimizedWindowDelegate.visualScale
                        transformOrigin: !root.vertical ? Item.Bottom
                                         : DockController.position === "left" ? Item.Left : Item.Right

                        DockMinimizedWindowTile {
                            id: minimizedWindowTile
                            objectName: "minimized-window-tile-" + minimizedWindowDelegate.modelData.windowId
                            anchors.fill: parent
                            source: root.dockWindowPreviewService
                                ? root.dockWindowPreviewService.imageUrl(
                                      minimizedWindowDelegate.modelData.windowId) : ""
                            sourceSize: root.dockWindowPreviewService
                                ? root.dockWindowPreviewService.imageSize(
                                      minimizedWindowDelegate.modelData.windowId) : Qt.size(0, 0)
                            iconName: minimizedWindowDelegate.modelData.iconName
                            iconPath: minimizedWindowDelegate.modelData.iconPath
                            appName: minimizedWindowDelegate.modelData.displayName
                            maximumPresentationScale: Math.max(1, DockController.magnificationScale)
                            presentationScale: minimizedWindowDelegate.magnificationScale
                            pressed: minimizedWindowTap.pressed
                            // Settles into the slot once the window image lands.
                            opacity: imageReady ? 1 : 0.9
                            scale: imageReady ? 1 : 0.86
                            Behavior on opacity {
                                NumberAnimation { duration: root.animationDuration(220); easing.type: Easing.OutCubic }
                            }
                            Behavior on scale {
                                NumberAnimation { duration: root.animationDuration(260); easing.type: Easing.OutBack; easing.overshoot: 1.2 }
                            }

                            Connections {
                                target: root.dockWindowPreviewService
                                function onImageChanged(windowId) {
                                    if (windowId !== minimizedWindowDelegate.modelData.windowId)
                                        return
                                    minimizedWindowTile.sourceSize =
                                        root.dockWindowPreviewService.imageSize(windowId)
                                    minimizedWindowTile.source =
                                        root.dockWindowPreviewService.imageUrl(windowId)
                                }
                                function onGenerationChanged() {
                                    minimizedWindowTile.sourceSize =
                                        root.dockWindowPreviewService.imageSize(
                                            minimizedWindowDelegate.modelData.windowId)
                                    minimizedWindowTile.source =
                                        root.dockWindowPreviewService.imageUrl(
                                            minimizedWindowDelegate.modelData.windowId)
                                    root.dockWindowPreviewService.requestWindowPreview(
                                        minimizedWindowDelegate.modelData.windowId, false)
                                }
                            }
                        }

                        TapHandler {
                            id: minimizedWindowTap
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.DragThreshold
                            onTapped: DockController.activateWindow(
                                minimizedWindowDelegate.modelData.taskKey,
                                minimizedWindowDelegate.modelData.windowId)
                        }

                        // Window menu for this exact tile; geometry matches the app
                        // icon menu, anchored to the Dock slot.
                        TapHandler {
                            id: minimizedWindowContextTap
                            acceptedButtons: Qt.RightButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: function(eventPoint) {
                                if (!root.contextMenuController || !root.dockSurfaceGeometry
                                        || !root.isMinimizedPointerTargetAt(
                                            eventPoint, minimizedWindowDelegate.hoverKey))
                                    return
                                const topLeft = minimizedWindowDelegate.mapToItem(root, 0, 0)
                                const outputRect = root.dockSurfaceGeometry.outputLocalDelegateRect(
                                    root.outputWidth, root.outputHeight,
                                    Math.round(root.width), Math.round(root.height),
                                    DockController.position, DockController.layerShellEdgeMargin,
                                    Qt.rect(topLeft.x, topLeft.y,
                                            minimizedWindowDelegate.width,
                                            minimizedWindowDelegate.height))
                                root.contextMenuController.presentDockWindow(
                                    minimizedWindowDelegate.modelData.taskKey,
                                    minimizedWindowDelegate.modelData.windowId,
                                    outputRect.x, outputRect.y,
                                    outputRect.width, outputRect.height, root.outputKey)
                            }
                        }

                        DragHandler {
                            // Only one drag at a time: never while an app icon is reordering.
                            enabled: root.draggedDesktopFileName.length === 0
                            target: null
                            acceptedButtons: Qt.LeftButton
                            dragThreshold: 8
                            onGrabChanged: function(transition) {
                                const windowId = minimizedWindowDelegate.modelData.windowId
                                if (transition === PointerDevice.GrabExclusive) {
                                    root.beginMinimizedReorder(windowId,
                                        centroid.scenePosition.x, centroid.scenePosition.y)
                                } else if (transition === PointerDevice.UngrabExclusive) {
                                    root.finishMinimizedReorder(windowId)
                                } else if (transition === PointerDevice.CancelGrabExclusive) {
                                    root.cancelMinimizedReorder(windowId)
                                }
                            }
                            onTranslationChanged: {
                                if (active) {
                                    root.updateMinimizedReorder(
                                        minimizedWindowDelegate.modelData.windowId,
                                        centroid.scenePosition.x, centroid.scenePosition.y)
                                }
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
    // revealable without making transparent surface headroom clickable. It
    // spans the Dock's own length, so pushing the pointer against the edge
    // anywhere under the Dock reveals it, not only near the centre.
    Rectangle {
        id: edgeRevealTarget
        objectName: "edgeRevealTarget"
        width: root.vertical ? 4 : Math.min(root.width, Math.max(96, root.restingWidth))
        height: root.vertical ? Math.min(root.height, Math.max(96, root.restingHeight)) : 4
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
        const thumbnailWidth = DockController.iconSize
        const thumbnailHeight = DockController.iconSize
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

        // While auto-hide floats the Dock off the edge, the gap down to the
        // edge belongs to it: the pointer that revealed it is still there.
        if (DockController.physicalEdgeReveal) {
            const bridge = revealBridgeRect()
            if (point.x >= bridge.x && point.x < bridge.x + bridge.width
                && point.y >= bridge.y && point.y < bridge.y + bridge.height)
                return true
        }

        if (point.x >= root.chromeRestX && point.x < root.chromeRestX + dockChrome.width
            && point.y >= root.chromeRestY && point.y < root.chromeRestY + dockChrome.height)
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

    // The strip between the floating chrome and the physical screen edge,
    // across the chrome's length, in panel coordinates.
    function revealBridgeRect() {
        if (DockController.position === "left")
            return Qt.rect(0, root.chromeRestY, root.chromeRestX, dockChrome.height)
        if (DockController.position === "right") {
            const right = root.chromeRestX + dockChrome.width
            return Qt.rect(right, root.chromeRestY, root.width - right, dockChrome.height)
        }
        const bottom = root.chromeRestY + dockChrome.height
        return Qt.rect(root.chromeRestX, bottom, dockChrome.width, root.height - bottom)
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
        for (var m = 0; m < minimizedRepeater.count; ++m) {
            const tileItem = minimizedRepeater.itemAt(m)
            if (tileItem && tileItem.tile)
                inputInteractionRects.push(windowRect(tileItem.tile.mapToItem(root, 0, 0,
                    tileItem.tile.width, tileItem.tile.height)))
        }
        if (DockController.physicalEdgeReveal)
            inputInteractionRects.push(windowRect(revealBridgeRect()))
        const chromeRect = windowRect(Qt.rect(root.chromeRestX, root.chromeRestY,
                                              dockChrome.width, dockChrome.height))
        inputRegionBridge.update(chromeRect, inputInteractionRects,
                                 appRepeater.count + minimizedRepeater.count
                                 + (DockController.physicalEdgeReveal ? 1 : 0))
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
        if (!targetItem && pointerTargetTaskKey.startsWith("minimized:")) {
            presentMinimizedWindowTooltip()
            return
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

    function presentMinimizedWindowTooltip() {
        if (dockWindowPreviewSurface && !previewHovered)
            dockWindowPreviewSurface.hidePreview()
        previewTaskKey = ""
        for (var index = 0; index < minimizedRepeater.count; ++index) {
            const item = minimizedRepeater.itemAt(index)
            if (!item || item.hoverKey !== pointerTargetTaskKey)
                continue
            const tileRect = item.tile.mapToItem(root, 0, 0, item.tile.width, item.tile.height)
            const outputRegion = dockSurfaceGeometry.outputLocalDelegateRect(
                outputWidth, outputHeight, surfaceWidth, surfaceHeight,
                DockController.position, DockController.layerShellEdgeMargin,
                windowRect(tileRect))
            dockTooltipSurface.showTooltip(item.modelData.title || item.modelData.displayName,
                                           outputRegion, DockController.position)
            return
        }
        dockTooltipSurface.hideTooltip()
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
        return draggedDesktopFileName.length > 0 || minimizedDragWindowId.length > 0
    }

    function isMinimizedPointerTargetAt(eventPoint, hoverKey) {
        updatePointerFromScene(eventPoint.scenePosition.x, eventPoint.scenePosition.y)
        return pointerTargetTaskKey === hoverKey
    }

    // App delegates first, then minimized window tiles: one magnification wave.
    function hoverItemAt(index) {
        return index < appRepeater.count ? appRepeater.itemAt(index)
            : minimizedRepeater.itemAt(index - appRepeater.count)
    }

    function hoverItemKey(item) {
        return item ? (item.hoverKey || item.taskKey) : ""
    }

    function updateHoverEffect() {
        const count = appRepeater.count + minimizedRepeater.count
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
            const item = hoverItemAt(i)
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
        const targetItem = targetIndex >= 0 ? hoverItemAt(targetIndex) : null
        pointerTargetTaskKey = hoverItemKey(targetItem)
        const liftedItem = liftActive && hoveredIndex >= 0 ? hoverItemAt(hoveredIndex) : null
        updateDelegateTransforms(totalExtra, slotPitch,
                                 liftedItem ? (liftedItem.hoverKey || liftedItem.objectName) : "",
                                 computedScales, computedExtras, computedPrefixes)
        scheduleInputRegionUpdate()
        refreshVisibleTooltip()
    }

    // Where a resting slot's item sits while sourceIndex is lifted toward targetIndex.
    function slotPreviewIndex(originalIndex, sourceIndex, targetIndex) {
        if (originalIndex === sourceIndex)
            return targetIndex
        if (sourceIndex < targetIndex
            && originalIndex > sourceIndex && originalIndex <= targetIndex)
            return originalIndex - 1
        if (sourceIndex > targetIndex
            && originalIndex >= targetIndex && originalIndex < sourceIndex)
            return originalIndex + 1
        return originalIndex
    }

    function previewIndexFor(originalIndex) {
        return slotPreviewIndex(originalIndex, draggedSourceIndex, dragTargetIndex)
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

        const appExtra = count > 0 ? (computedPrefixes[count - 1] || 0)
            + (computedExtras[count - 1] || 0) : 0
        minimizedDivider.visualOffset = appExtra - totalExtra / 2
        const minimizedDragging = minimizedDragWindowId.length > 0
        for (var m = 0; m < minimizedRepeater.count; ++m) {
            const tile = minimizedRepeater.itemAt(m)
            if (!tile)
                continue
            const index = count + m
            const tileExtra = computedExtras[index] || 0
            var tileOffset = (computedPrefixes[index] || 0) + tileExtra / 2 - totalExtra / 2
            const lifted = minimizedDragging && m === minimizedDragSourceIndex
            if (minimizedDragging) {
                if (lifted) {
                    const currentCenterRelative = delegateRestingCenterInPanel(tile)
                        - primaryExtent() / 2
                    tileOffset += minimizedDragCenterRelativeX - currentCenterRelative
                } else {
                    tileOffset += (slotPreviewIndex(m, minimizedDragSourceIndex,
                                                    minimizedDragTargetIndex) - m) * slotPitch
                }
            }
            tile.magnificationScale = dragging ? 1 : (computedScales[index] || 1)
            const crossOffsetValue = lifted ? crossOffset(reorderOffsetY)
                : (tile.hoverKey === liftedKey ? crossOffset(liftOffsetY) : 0)
            // Offsets land while the tile still reads as dragged, so no animation
            // runs. A repeater can hand a dragged delegate to another window, and
            // its stale offset must snap back rather than fly to the new slot.
            tile.visualOffsetX = vertical ? crossOffsetValue : tileOffset
            tile.visualOffsetY = vertical ? tileOffset : crossOffsetValue
            tile.dragging = lifted
            tile.dragScale = lifted ? reorderScale : 1
            tile.tornOut = lifted && minimizedTearOut
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

    function recordMinimizedDragPointer(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return false
        const panelPoint = root.mapFromItem(null, sceneX, sceneY)
        if (!isFinite(panelPoint.x) || !isFinite(panelPoint.y))
            return false
        minimizedDragScenePosition = Qt.point(sceneX, sceneY)
        minimizedDragPanelPosition = Qt.point(panelPoint.x, panelPoint.y)
        return true
    }

    function minimizedPrimaryOfPanelPosition() {
        return vertical ? minimizedDragPanelPosition.y : minimizedDragPanelPosition.x
    }

    function minimizedIndexForWindow(windowId) {
        // Read the entries themselves: minimizedCount can still hold the old
        // length while a change handler runs.
        const entries = minimizedEntries || []
        for (var index = 0; index < entries.length; ++index) {
            if (entries[index] && entries[index].windowId === windowId)
                return index
        }
        return -1
    }

    function beginMinimizedReorder(windowId, sceneX, sceneY) {
        if (isReordering() || !recordMinimizedDragPointer(sceneX, sceneY))
            return false
        const source = minimizedIndexForWindow(windowId)
        const sourceItem = source >= 0 ? minimizedRepeater.itemAt(source) : null
        if (!sourceItem)
            return false
        minimizedDragWindowId = windowId
        minimizedDragSourceIndex = source
        minimizedDragTargetIndex = source
        minimizedDragPointerOriginPrimary = minimizedPrimaryOfPanelPosition()
        minimizedDragPointerOriginScene = Qt.point(sceneX, sceneY)
        minimizedGhostGrip = minimizedGripFor(sourceItem.windowTile, sceneX, sceneY)
        minimizedDragOriginCenterRelativeX = delegateRestingCenterInPanel(sourceItem)
            - primaryExtent() / 2
        minimizedDragCenterRelativeX = minimizedDragOriginCenterRelativeX
        updateHoverEffect()
        return true
    }

    function updateMinimizedReorder(windowId, sceneX, sceneY) {
        if (windowId !== minimizedDragWindowId || !recordMinimizedDragPointer(sceneX, sceneY))
            return
        updatePointerFromScene(sceneX, sceneY)
        // Primary axis only: the tile follows the pointer along the dock and its
        // neighbours shift by one slot pitch.
        const unclamped = minimizedDragOriginCenterRelativeX
            + minimizedPrimaryOfPanelPosition() - minimizedDragPointerOriginPrimary
        const firstItem = minimizedRepeater.itemAt(0)
        const lastItem = minimizedRepeater.itemAt(minimizedRepeater.count - 1)
        if (firstItem && lastItem) {
            const firstCenterRelative = delegateRestingCenterInPanel(firstItem)
                - primaryExtent() / 2
            const lastCenterRelative = firstCenterRelative
                + (minimizedRepeater.count - 1) * root.slotPitch
            // The tile stays within the minimized windows: it cannot leave the
            // Dock sideways, and a lone window has no slot to move to.
            minimizedDragCenterRelativeX = Math.max(firstCenterRelative,
                                                    Math.min(lastCenterRelative, unclamped))
            minimizedDragTargetIndex = Math.max(0, Math.min(minimizedCount - 1,
                Math.round((minimizedDragCenterRelativeX - firstCenterRelative)
                           / Math.max(1, root.slotPitch))))
        } else {
            minimizedDragCenterRelativeX = minimizedDragOriginCenterRelativeX
        }
        // Cross-axis: pulling away from the Dock tears the window out.
        const away = minimizedAwayDistance()
        if (!minimizedTearOut && away > tearOutThreshold)
            beginMinimizedTearOut()
        else if (minimizedTearOut && away < tearOutThreshold / 2)
            endMinimizedTearOut()
        if (minimizedTearOut) {
            // Torn out, the window no longer belongs to a slot.
            minimizedDragTargetIndex = minimizedDragSourceIndex
            updateMinimizedGhost()
        }
        updateHoverEffect()
    }

    // How far the pointer has moved from the press toward the desktop.
    function minimizedAwayDistance() {
        const dx = minimizedDragScenePosition.x - minimizedDragPointerOriginScene.x
        const dy = minimizedDragScenePosition.y - minimizedDragPointerOriginScene.y
        if (DockController.position === "left")
            return dx
        if (DockController.position === "right")
            return -dx
        return -dy
    }

    // Where the pointer grips the window image, as fractions of its size.
    function minimizedGripFor(windowTile, sceneX, sceneY) {
        if (!windowTile)
            return Qt.point(0.5, 0.5)
        const paintedWidth = Math.max(1, windowTile.paintedWidth || windowTile.width)
        const paintedHeight = Math.max(1, windowTile.paintedHeight || windowTile.height)
        const local = windowTile.mapFromItem(null, sceneX, sceneY)
        const gripX = (local.x - (windowTile.width - paintedWidth) / 2) / paintedWidth
        const gripY = (local.y - (windowTile.height - paintedHeight) / 2) / paintedHeight
        return Qt.point(isFinite(gripX) ? Math.max(0, Math.min(1, gripX)) : 0.5,
                        isFinite(gripY) ? Math.max(0, Math.min(1, gripY)) : 0.5)
    }

    // Dock window coordinates translate into output-local ones by a constant.
    function windowToOutputOffset() {
        const mapped = dockSurfaceGeometry.outputLocalDelegateRect(
            outputWidth, outputHeight, surfaceWidth, surfaceHeight,
            DockController.position, DockController.layerShellEdgeMargin,
            Qt.rect(0, 0, 100, 100))
        return Qt.point(mapped.x, mapped.y)
    }

    function beginMinimizedTearOut() {
        const sourceItem = minimizedRepeater.itemAt(minimizedDragSourceIndex)
        const service = dockWindowPreviewService
        if (!sourceItem || !service || !dockWindowPreviewSurface || !dockSurfaceGeometry)
            return false
        const thumbnail = service.imageUrl(minimizedDragWindowId)
        const windowTile = sourceItem.windowTile
        if (thumbnail.length === 0 || !windowTile || !windowTile.imageReady)
            return false
        const paintedWidth = Math.max(1, windowTile.paintedWidth)
        const paintedHeight = Math.max(1, windowTile.paintedHeight)
        const left = (windowTile.width - paintedWidth) / 2
        const top = (windowTile.height - paintedHeight) / 2
        const topLeft = windowTile.mapToItem(null, left, top)
        const bottomRight = windowTile.mapToItem(null, left + paintedWidth, top + paintedHeight)
        const offset = windowToOutputOffset()
        minimizedGhostTileRect = Qt.rect(topLeft.x + offset.x, topLeft.y + offset.y,
                                         Math.max(1, bottomRight.x - topLeft.x),
                                         Math.max(1, bottomRight.y - topLeft.y))
        var windowSize = service.windowSize(minimizedDragWindowId)
        if (!windowSize || windowSize.width <= 0 || windowSize.height <= 0) {
            // An older compositor does not report the size: assume a window
            // filling most of the output with the thumbnail's aspect.
            const image = service.imageSize(minimizedDragWindowId)
            const aspect = image.width > 0 && image.height > 0 ? image.width / image.height : 1.6
            const fitWidth = Math.min(outputWidth * 0.6, outputHeight * 0.6 * aspect)
            windowSize = Qt.size(fitWidth, fitWidth / aspect)
        }
        minimizedGhostWindowSize = Qt.size(windowSize.width, windowSize.height)
        minimizedReturningWindowId = ""
        if (dockTooltipSurface)
            dockTooltipSurface.hideTooltip()
        dockWindowPreviewSurface.showGhost(minimizedDragWindowId, thumbnail,
                                           outputWidth, outputHeight,
                                           outputOriginX, outputOriginY)
        minimizedTearOut = true
        return true
    }

    // Pulled back into the Dock: the ghost shrinks into the tile's slot and
    // the tile reappears once it has landed.
    function endMinimizedTearOut() {
        if (!minimizedTearOut)
            return
        minimizedTearOut = false
        retractMinimizedGhost(minimizedDragWindowId)
    }

    function retractMinimizedGhost(windowId) {
        const surface = dockWindowPreviewSurface
        if (!surface || !surface.ghostVisible)
            return
        const tileRect = currentMinimizedTileRect(windowId) || minimizedGhostTileRect
        minimizedReturningWindowId = windowId
        surface.animateGhost(tileRect.x, tileRect.y, tileRect.width, tileRect.height,
                             minimizedGhostRadius(0), 0, animationDuration(200), function() {
            surface.hideGhost()
            if (root.minimizedReturningWindowId === windowId)
                root.minimizedReturningWindowId = ""
        })
    }

    // The tile image's rect in output coordinates, at rest in its slot.
    function currentMinimizedTileRect(windowId) {
        const index = minimizedIndexForWindow(windowId)
        const item = index >= 0 ? minimizedRepeater.itemAt(index) : null
        const windowTile = item ? item.windowTile : null
        if (!windowTile || !dockSurfaceGeometry)
            return null
        const paintedWidth = Math.max(1, windowTile.paintedWidth)
        const paintedHeight = Math.max(1, windowTile.paintedHeight)
        const left = (windowTile.width - paintedWidth) / 2
        const top = (windowTile.height - paintedHeight) / 2
        const topLeft = windowTile.mapToItem(null, left, top)
        const bottomRight = windowTile.mapToItem(null, left + paintedWidth, top + paintedHeight)
        // Remove the drag lift and offset: the tile is about to settle.
        const restX = -item.visualOffsetX
        const restY = -item.visualOffsetY
        const offset = windowToOutputOffset()
        return Qt.rect(topLeft.x + offset.x + restX, topLeft.y + offset.y + restY,
                       Math.max(1, bottomRight.x - topLeft.x),
                       Math.max(1, bottomRight.y - topLeft.y))
    }

    // The ghost's rect for a growth progress: the grip point stays under the pointer.
    function minimizedGhostRect(progress) {
        const tileRect = minimizedGhostTileRect
        const windowSize = minimizedGhostWindowSize
        const width = tileRect.width + (windowSize.width - tileRect.width) * progress
        const height = tileRect.height + (windowSize.height - tileRect.height) * progress
        const offset = windowToOutputOffset()
        const pointerX = minimizedDragScenePosition.x + offset.x
        const pointerY = minimizedDragScenePosition.y + offset.y
        return Qt.rect(pointerX - minimizedGhostGrip.x * width,
                       pointerY - minimizedGhostGrip.y * height, width, height)
    }

    function minimizedGhostRadius(progress) {
        const tileRect = minimizedGhostTileRect
        const tileRadius = Math.max(3, Math.round(Math.min(tileRect.width, tileRect.height) * 0.08))
        return tileRadius + (tearOutWindowRadius - tileRadius) * progress
    }

    // How far through the growth distance the pointer is, linearly.
    function minimizedGhostPull() {
        return Math.max(0, Math.min(1,
            (minimizedAwayDistance() - tearOutThreshold) / tearOutGrowDistance))
    }

    function minimizedGhostProgress() {
        return 1 - Math.pow(1 - minimizedGhostPull(), 3)
    }

    function updateMinimizedGhost() {
        if (!dockWindowPreviewSurface)
            return
        const progress = minimizedGhostProgress()
        const rect = minimizedGhostRect(progress)
        dockWindowPreviewSurface.moveGhost(rect.x, rect.y, rect.width, rect.height,
                                           minimizedGhostRadius(progress), progress)
    }

    // Grows the ghost to the window's size where it was dropped, then has the
    // compositor restore the window exactly underneath before the ghost fades.
    function dropMinimizedTearOut(windowId) {
        const index = minimizedIndexForWindow(windowId)
        const entry = index >= 0 ? minimizedEntries[index] : null
        const surface = dockWindowPreviewSurface
        // Let go barely out of the Dock: it slides back instead of opening a
        // window right on top of the Dock.
        if (!entry || !surface || minimizedGhostPull() < 0.3) {
            retractMinimizedGhost(windowId)
            return
        }
        const full = minimizedGhostRect(1)
        // Keep the title bar reachable on the output.
        const x = Math.max(80 - full.width, Math.min(outputWidth - 80, full.x))
        const y = Math.max(0, Math.min(outputHeight - 48, full.y))
        const taskKey = entry.taskKey
        const originX = outputOriginX
        const originY = outputOriginY
        minimizedRestoringWindowId = windowId
        minimizedRestoreTimeout.restart()
        surface.animateGhost(x, y, full.width, full.height, tearOutWindowRadius, 1,
                             animationDuration(190), function() {
            const restored = DockController.restoreMinimizedWindowAt(
                taskKey, windowId, Math.round(x) + originX, Math.round(y) + originY)
            surface.releaseGhost(restored ? 90 : 0)
            if (!restored)
                root.minimizedRestoringWindowId = ""
        })
    }

    function cancelMinimizedTearOut() {
        if (!minimizedTearOut)
            return
        retractMinimizedGhost(minimizedDragWindowId)
    }

    function finishMinimizedReorder(windowId) {
        if (windowId !== minimizedDragWindowId)
            return
        if (minimizedTearOut) {
            dropMinimizedTearOut(windowId)
            minimizedDragTargetIndex = minimizedDragSourceIndex
        }
        // Commit on the next turn. The move rebuilds the minimized repeater, and
        // the handler reporting this release belongs to one of its delegates.
        Qt.callLater(root.commitMinimizedReorder, windowId)
    }

    function commitMinimizedReorder(windowId) {
        if (windowId !== minimizedDragWindowId)
            return
        const target = minimizedDragTargetIndex
        if (target >= 0 && target !== minimizedDragSourceIndex)
            DockController.moveMinimizedWindow(windowId, target)
        clearMinimizedReorder()
    }

    function cancelMinimizedReorder(windowId) {
        if (windowId !== minimizedDragWindowId)
            return
        cancelMinimizedTearOut()
        clearMinimizedReorder()
    }

    function clearMinimizedReorder() {
        minimizedDragWindowId = ""
        minimizedDragSourceIndex = -1
        minimizedDragTargetIndex = -1
        minimizedDragPointerOriginPrimary = 0
        minimizedDragOriginCenterRelativeX = 0
        minimizedDragCenterRelativeX = 0
        minimizedTearOut = false
        updateHoverEffect()
    }

    Timer {
        id: minimizedRestoreTimeout
        interval: 1500
        onTriggered: root.minimizedRestoringWindowId = ""
    }

    onMinimizedEntriesChanged: {
        if (minimizedRestoringWindowId.length > 0
                && minimizedIndexForWindow(minimizedRestoringWindowId) < 0) {
            minimizedRestoringWindowId = ""
            minimizedRestoreTimeout.stop()
        }
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
