import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Astrea.Shared as Shared

Item {
    id: root
    Shared.Typography { id: typography; surface: "dock" }

    required property int index
    required property var model
    required property string taskKey
    required property string desktopFileName
    required property string displayName
    required property string iconName
    required property string iconPath
    required property string iconUrl
    required property bool resolved
    required property bool launching
    required property bool pinned
    required property bool runtimeKnown
    required property bool running
    required property bool active
    required property int windowCount
    required property int unreadCount
    required property bool unreadCountVisible
    required property int slotWidth
    required property int slotHeight
    property var dockPanel
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
    property string outputKey: ""
    property int outputOriginX: 0
    property int outputOriginY: 0
    property bool pointerTarget: false
    property real magnificationScale: 1.0
    property real visualOffsetX: 0
    property real visualOffsetY: 0
    property bool dragging: false
    property real dragScale: 1.0
    property bool dragWasActive: false
    property bool hasLastDragPanelPosition: false
    property real lastDragPanelX: 0
    property real lastDragPanelY: 0
    readonly property real visualScale: root.magnificationScale * root.dragScale
    readonly property rect interactionRegion: Qt.rect(interactionTarget.x,
                                                       interactionTarget.y,
                                                       interactionTarget.width,
                                                       interactionTarget.height)
    // 0 = idle, 1 = exclusively grabbed and dragging.
    property int dragLifecycle: 0

    property int iconSize: 48
    signal activated(string taskKey)
    signal dragStarted(string desktopFileName)
    signal dragMoved(string desktopFileName, real translationX, real sceneX, real sceneY)
    signal dragFinished(string desktopFileName)
    signal dragCanceled(string desktopFileName)

    function recordDragPointer(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return false
        if (!root.dockPanel)
            return false
        const panelPoint = root.dockPanel.mapFromItem(null, sceneX, sceneY)
        if (!isFinite(panelPoint.x) || !isFinite(panelPoint.y))
            return false
        root.hasLastDragPanelPosition = true
        root.lastDragPanelX = panelPoint.x
        root.lastDragPanelY = panelPoint.y
        root.dockPanel.updatePointerAtPoint(panelPoint.x, panelPoint.y)
        return true
    }

    function restoreLastDragPointer() {
        if (root.dockPanel && root.hasLastDragPanelPosition)
            root.dockPanel.updatePointerAtPoint(root.lastDragPanelX,
                                                root.lastDragPanelY)
    }

    function handleGrabTransition(transition) {
        if (transition === PointerDevice.GrabExclusive) {
            if (root.dragLifecycle !== 0)
                return
            root.dragLifecycle = 1
            root.dragWasActive = true
            root.dragStarted(root.desktopFileName)
        } else if (transition === PointerDevice.UngrabExclusive) {
            if (root.dragLifecycle !== 1)
                return
            root.dragLifecycle = 0
            root.dragFinished(root.desktopFileName)
        } else if (transition === PointerDevice.CancelGrabExclusive) {
            if (root.dragLifecycle !== 1)
                return
            root.dragLifecycle = 0
            root.dragCanceled(root.desktopFileName)
        }
    }

    function updateInteractionTargetGeometry() {
        interactionTarget.updateGeometry()
        if (root.dockPanel) {
            root.dockPanel.scheduleInputRegionUpdate()
            root.dockPanel.refreshTooltipAnchor(root.taskKey)
        }
    }

    function scheduleInteractionTargetGeometryUpdate() {
        Qt.callLater(root.updateInteractionTargetGeometry)
    }

    function isPointerTarget() {
        return !root.dockPanel
            || root.dockPanel.pointerTargetTaskKey === root.taskKey
    }

    function isPointerTargetAt(eventPoint) {
        if (!root.dockPanel)
            return true
        const panelPoint = root.dockPanel.mapFromItem(null,
                                                        eventPoint.scenePosition.x,
                                                        eventPoint.scenePosition.y)
        root.dockPanel.updatePointerAtPoint(panelPoint.x, panelPoint.y)
        return root.isPointerTarget()
    }

    objectName: taskKey
    width: slotWidth
    height: slotHeight

    // Config changes (icon size, slot size) glide instead of snapping.
    readonly property int configDuration: root.dockPanel ? root.dockPanel.animationDuration(220) : 220
    Behavior on iconSize {
        NumberAnimation { duration: root.configDuration; easing.type: Easing.OutCubic }
    }
    Behavior on slotWidth {
        NumberAnimation { duration: root.configDuration; easing.type: Easing.OutCubic }
    }
    Behavior on slotHeight {
        NumberAnimation { duration: root.configDuration; easing.type: Easing.OutCubic }
    }

    // Launch bounce, macOS style: the icon hops off the Dock edge until the
    // app shows a window (or a few hops pass), finishing the hop in flight.
    property real bounceOffset: 0
    property int bounceHops: 0
    // Once the app has shown a window during this launch the bounce ends,
    // even if that window is already gone again.
    property bool bounceSawRunning: false
    readonly property int maximumBounceHops: 6
    readonly property real bounceHeight: Math.round(root.iconSize * 0.42)
    function startLaunchBounce() {
        if (!root.dockPanel || root.dockPanel.animationDuration(300) <= 0)
            return
        root.bounceHops = 0
        root.bounceSawRunning = false
        if (!launchBounce.running)
            launchBounce.start()
    }
    function shouldKeepBouncing() {
        return root.bounceHops < root.maximumBounceHops
            && !root.bounceSawRunning
            && (root.launching || (root.runtimeKnown && !root.running))
    }
    onLaunchingChanged: if (root.launching) root.startLaunchBounce()
    onRunningChanged: if (root.running && launchBounce.running) root.bounceSawRunning = true
    SequentialAnimation {
        id: launchBounce
        NumberAnimation {
            target: root; property: "bounceOffset"
            to: root.bounceHeight
            duration: root.dockPanel ? root.dockPanel.animationDuration(300) : 300
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: root; property: "bounceOffset"
            to: 0
            duration: root.dockPanel ? root.dockPanel.animationDuration(300) : 300
            easing.type: Easing.InQuad
        }
        // A short squash where the icon lands sells the weight.
        PauseAnimation { duration: root.dockPanel ? root.dockPanel.animationDuration(90) : 90 }
        onFinished: {
            root.bounceHops += 1
            if (root.shouldKeepBouncing())
                launchBounce.start()
            else
                root.bounceOffset = 0
        }
    }

    // Press feedback, chosen by pressEffect: darken (macOS), shrink (Windows),
    // frosted (dip and darken over a glass plate) or none.
    readonly property bool iconHeld: tapHandler.pressed && !dragHandler.active
    readonly property real pressStrength: DockController.pressEffectStrength / 100
    readonly property bool pressDarkens: DockController.pressEffect === "darken"
        || DockController.pressEffect === "frosted"
    readonly property bool pressShrinks: DockController.pressEffect === "shrink"
        || DockController.pressEffect === "frosted"
    property real pressScale: root.iconHeld && root.pressShrinks ? 1 - root.pressStrength / 3 : 1.0
    property real pressAmount: root.iconHeld && root.pressDarkens ? 1 : 0
    Behavior on pressAmount {
        NumberAnimation {
            duration: root.dockPanel ? root.dockPanel.animationDuration(root.iconHeld ? 90 : 200) : 120
            easing.type: Easing.OutCubic
        }
    }

    // Drives the frosted plate and its blur area. Compositor blur is
    // all-or-nothing per region, so the area grows and shrinks instead of fading.
    property real frostedAmount: root.iconHeld && DockController.pressEffect === "frosted" ? 1 : 0
    Behavior on frostedAmount {
        NumberAnimation {
            duration: root.dockPanel ? root.dockPanel.animationDuration(root.iconHeld ? 90 : 180) : 120
            easing.type: Easing.OutCubic
        }
    }
    readonly property bool frostedGlassActive: root.frostedAmount > 0.01
        && !!root.dockPanel && root.dockPanel.frostedGlassAvailable
    onFrostedGlassActiveChanged: {
        if (root.dockPanel)
            root.dockPanel.setFrostedPress(root, root.frostedGlassActive)
    }
    Component.onDestruction: {
        if (root.dockPanel)
            root.dockPanel.setFrostedPress(root, false)
    }
    readonly property alias iconVisual: appIcon
    readonly property real plateInset: Math.round(root.iconSize * 0.08)

    // The plate behind the icon: Windows-style hover indicator, dimming while
    // held, and the frosted press effect's glass.
    readonly property real hoverPlateTarget: DockController.hoverIndicator === "plate"
        && root.pointerTarget && !dragHandler.active
        ? DockController.hoverIndicatorStrength / 100 * (root.iconHeld ? 0.6 : 1) : 0
    // With a glass surface the frosted plate is drawn there instead.
    readonly property real frostedPlateTarget: root.iconHeld
        && DockController.pressEffect === "frosted"
        && !(root.dockPanel && root.dockPanel.frostedGlassAvailable) ? 0.16 : 0
    property real plateAlpha: Math.max(root.hoverPlateTarget, root.frostedPlateTarget)
    Behavior on plateAlpha {
        NumberAnimation {
            duration: root.dockPanel
                ? root.dockPanel.animationDuration(root.pointerTarget ? 90 : 180) : 120
            easing.type: Easing.OutCubic
        }
    }
    Behavior on pressScale {
        NumberAnimation {
            duration: root.dockPanel ? root.dockPanel.animationDuration(tapHandler.pressed ? 90 : 200) : 120
            easing.type: Easing.OutCubic
        }
    }

    SystemPalette { id: systemPalette }

    Shared.AstreaAppIcon {
        id: appIcon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        scale: root.magnificationScale * root.dragScale * root.pressScale
        transformOrigin: !root.dockPanel || !root.dockPanel.vertical ? Item.Bottom
                         : DockController.position === "left" ? Item.Left : Item.Right
        iconName: root.iconName
        iconPath: root.iconPath
        iconUrl: root.iconUrl
        appName: root.displayName
        retainWhileLoading: true
        maximumPresentationScale: Math.max(1, DockController.magnificationScale)
        iconRadius: 10
        fallbackRadius: 10
        // The glass layer draws its own copy while the press is on screen.
        opacity: root.dockPanel && root.dockPanel.frostedPressDelegate === root
                 && root.dockPanel.frostedIconPresented ? 0 : 1

        // Only layered while pressed, rendered at the on-screen size so a
        // magnified icon stays sharp.
        layer.enabled: root.pressAmount > 0.001
        layer.textureSize: Qt.size(Math.ceil(width * Math.max(1, scale) * Screen.devicePixelRatio),
                                   Math.ceil(height * Math.max(1, scale) * Screen.devicePixelRatio))
        layer.smooth: true
        layer.effect: MultiEffect {
            brightness: -root.pressStrength * root.pressAmount
        }

        // The bounce lifts away from the Dock's screen edge.
        transform: Translate {
            x: !root.dockPanel || !root.dockPanel.vertical ? 0
               : DockController.position === "left" ? root.bounceOffset : -root.bounceOffset
            y: !root.dockPanel || !root.dockPanel.vertical ? -root.bounceOffset : 0
        }
    }

    transform: Translate {
        x: root.visualOffsetX
        y: root.visualOffsetY
    }
    // The visual topmost icon should also win pointer targeting where scaled
    // icon bounds overlap neighboring resting slots.
    z: root.dragging ? 100 : root.magnificationScale
    Behavior on visualOffsetX {
        enabled: !root.dragging
        NumberAnimation { duration: root.dockPanel ? root.dockPanel.animationDuration(110) : 110; easing.type: Easing.OutCubic }
    }
    Behavior on visualOffsetY {
        NumberAnimation { duration: root.dockPanel ? root.dockPanel.animationDuration(110) : 110; easing.type: Easing.OutCubic }
    }
    Behavior on magnificationScale {
        NumberAnimation { duration: root.dockPanel ? root.dockPanel.animationDuration(110) : 110; easing.type: Easing.OutCubic }
    }
    Behavior on dragScale {
        NumberAnimation { duration: root.dockPanel ? root.dockPanel.animationDuration(110) : 110; easing.type: Easing.OutCubic }
    }

    Item {
        id: interactionTarget
        objectName: "interactionTarget-" + (root.desktopFileName || root.taskKey)
        parent: root.dockPanel || root
        width: root.iconSize * root.visualScale
        height: root.iconSize * root.visualScale
        // This target is reparented to the panel so its exact visual bounds
        // remain interactive even when they extend beyond the delegate slot.
        x: 0
        y: 0
        z: root.dragging ? 1000 : root.magnificationScale + 1

        function updateGeometry() {
            const targetSize = root.iconSize * root.visualScale
            width = targetSize
            height = targetSize
            if (root.dockPanel) {
                if (!root.dockPanel.vertical) {
                    const bottomCenter = appIcon.mapToItem(root.dockPanel,
                                                            appIcon.width / 2, appIcon.height)
                    x = bottomCenter.x - targetSize / 2
                    y = bottomCenter.y - targetSize
                } else if (DockController.position === "left") {
                    const leftCenter = appIcon.mapToItem(root.dockPanel, 0, appIcon.height / 2)
                    x = leftCenter.x
                    y = leftCenter.y - targetSize / 2
                } else {
                    const rightCenter = appIcon.mapToItem(root.dockPanel,
                                                          appIcon.width, appIcon.height / 2)
                    x = rightCenter.x - targetSize
                    y = rightCenter.y - targetSize / 2
                }
            } else {
                x = appIcon.x + appIcon.width / 2 - targetSize / 2
                y = appIcon.y + appIcon.height - targetSize
            }
        }

        TapHandler {
            id: tapHandler
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.DragThreshold
            onPressedChanged: if (pressed) root.dragWasActive = false
            onTapped: function(eventPoint) {
                const wasDrag = root.dragWasActive
                root.dragWasActive = false
                if (!wasDrag && root.isPointerTargetAt(eventPoint))
                    root.activated(root.taskKey)
            }
        }

        TapHandler {
            id: contextTapHandler
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: function(eventPoint) {
                if (!root.isPointerTargetAt(eventPoint) || !root.contextMenuController
                        || !root.dockSurfaceGeometry || !root.dockPanel)
                    return
                const topLeft = root.mapToItem(root.dockPanel, 0, 0)
                const outputRect = root.dockSurfaceGeometry.outputLocalDelegateRect(
                    root.dockPanel.outputWidth, root.dockPanel.outputHeight,
                    Math.round(root.dockPanel.width), Math.round(root.dockPanel.height),
                    DockController.position, DockController.layerShellEdgeMargin,
                    Qt.rect(topLeft.x, topLeft.y, root.width, root.height))
                if (root.contextMenuController.debugEnabled) {
                    console.log("astrea.context-menu " + JSON.stringify({
                        stage: "dock-anchor-resolved",
                        targetIdentity: root.taskKey,
                        outputKey: root.outputKey,
                        outputWidth: root.dockPanel.outputWidth,
                        outputHeight: root.dockPanel.outputHeight,
                        dockWindowWidth: root.dockPanel.width,
                        dockWindowHeight: root.dockPanel.height,
                        delegateX: topLeft.x,
                        delegateY: topLeft.y,
                        delegateWidth: root.width,
                        delegateHeight: root.height,
                        outputX: outputRect.x,
                        outputY: outputRect.y,
                        outputRectWidth: outputRect.width,
                        outputRectHeight: outputRect.height
                    }))
                }
                root.contextMenuController.presentDock(root.taskKey,
                    outputRect.x, outputRect.y, outputRect.width, outputRect.height,
                    root.outputKey)
            }
        }

        DragHandler {
            id: dragHandler
            enabled: root.pinned && (!root.dockPanel || root.dockPanel.draggedDesktopFileName === ""
                                      || root.dockPanel.draggedDesktopFileName === root.desktopFileName)
            target: null
            dragThreshold: 8
            onGrabChanged: function(transition) {
                if (transition === PointerDevice.GrabExclusive) {
                    root.hasLastDragPanelPosition = false
                    const scenePosition = centroid.scenePosition
                    root.recordDragPointer(scenePosition.x, scenePosition.y)
                } else if (transition === PointerDevice.UngrabExclusive
                           || transition === PointerDevice.CancelGrabExclusive) {
                    // Once the handler loses its exclusive grab, centroid can
                    // already be reset. The last active centroid is the
                    // release pointer and remains authoritative for hover.
                    root.restoreLastDragPointer()
                }
                root.handleGrabTransition(transition)
            }
            onTranslationChanged: {
                if (!active)
                    return
                const scenePosition = centroid.scenePosition
                root.recordDragPointer(scenePosition.x, scenePosition.y)
                root.dragMoved(root.desktopFileName,
                               root.dockPanel && root.dockPanel.vertical
                                   ? activeTranslation.y : activeTranslation.x,
                               scenePosition.x, scenePosition.y)
            }
        }
    }

    /*
     * The feedback rectangle follows the transformed icon, but remains below
     * the exact interaction target so transparent headroom is not clickable.
     */
    Rectangle {
        objectName: "iconPlate"
        z: -1
        readonly property real inset: root.plateInset
        x: appIcon.x - inset
        y: appIcon.y - inset
        width: appIcon.width + inset * 2
        height: appIcon.height + inset * 2
        scale: appIcon.scale
        transformOrigin: !root.dockPanel || !root.dockPanel.vertical ? Item.Bottom
                         : DockController.position === "left" ? Item.Left : Item.Right
        radius: appIcon.iconRadius + inset
        // White over a dark Dock, black over a light one, like WinUI's subtle fills.
        readonly property real shade: root.dockPanel && root.dockPanel.lightMaterial ? 0 : 1
        color: Qt.rgba(shade, shade, shade, root.plateAlpha)
        border.width: 1
        border.color: Qt.rgba(shade, shade, shade, root.plateAlpha * 0.8)
        visible: root.plateAlpha > 0.001
        transform: Translate {
            x: !root.dockPanel || !root.dockPanel.vertical ? 0
               : DockController.position === "left" ? root.bounceOffset : -root.bounceOffset
            y: !root.dockPanel || !root.dockPanel.vertical ? -root.bounceOffset : 0
        }
    }

    /*
     * The running indicator intentionally stays outside the icon transform.
     */
    Rectangle {
        id: runningIndicator
        objectName: "runningIndicator"
        function iconEdgePoint() {
            if (root.dockPanel && root.dockPanel.vertical) {
                return DockController.position === "left"
                    ? Qt.point(appIcon.x, appIcon.y + appIcon.height / 2)
                    : Qt.point(appIcon.x + appIcon.width,
                               appIcon.y + appIcon.height / 2)
            }
            return Qt.point(appIcon.x + appIcon.width / 2, appIcon.y + appIcon.height)
        }
        x: !root.dockPanel || !root.dockPanel.vertical
           ? iconEdgePoint().x - width / 2
           : DockController.position === "left"
             ? iconEdgePoint().x - width - 2
             : iconEdgePoint().x + 2
        y: !root.dockPanel || !root.dockPanel.vertical
           ? iconEdgePoint().y + 2
           : iconEdgePoint().y - height / 2
        width: DockController.indicatorStyle === "dot" ? DockController.indicatorSize
              : (!root.dockPanel || !root.dockPanel.vertical
                 ? (root.active ? 18 : 8) : DockController.indicatorSize)
        height: DockController.indicatorStyle === "dot" ? DockController.indicatorSize
               : (root.dockPanel && root.dockPanel.vertical
                  ? (root.active ? 18 : 8) : DockController.indicatorSize)
        radius: DockController.indicatorStyle === "dot" ? width / 2 : Math.min(width, height) / 2
        color: systemPalette.highlight
        // Fades and grows in when an app starts, stretches when it becomes
        // active, and shrinks away when it quits, instead of toggling.
        readonly property bool shown: root.runtimeKnown && root.running
            && DockController.indicatorStyle !== "none"
        visible: opacity > 0
        opacity: shown ? (root.active || DockController.indicatorStyle !== "dot" ? 1 : 0.85) : 0
        scale: shown ? 1 : 0.2
        readonly property int indicatorDuration: root.dockPanel ? root.dockPanel.animationDuration(220) : 220
        Behavior on opacity { NumberAnimation { duration: runningIndicator.indicatorDuration; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: runningIndicator.indicatorDuration; easing.type: Easing.OutBack } }
        Behavior on width { NumberAnimation { duration: runningIndicator.indicatorDuration; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: runningIndicator.indicatorDuration; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: unreadBadge
        parent: appIcon
        objectName: "unreadBadge"
        x: Math.max(0, appIcon.width - width)
        y: 0
        width: unreadCountLabel.implicitWidth + 10
        height: Math.max(18, unreadCountLabel.implicitHeight + 4)
        radius: height / 2
        color: "#d83b48"
        border.color: "#f6f6f6"
        border.width: 1
        z: 10
        visible: root.unreadCountVisible
        Text {
            id: unreadCountLabel
            objectName: "unreadCountLabel"
            anchors.centerIn: parent
            text: root.unreadCount > 99 ? "99+" : String(root.unreadCount)
            color: "white"
            font.family: typography.role("caption").family
            font.pixelSize: typography.role("caption").pixelSize
            font.weight: typography.role("caption").weight
            renderType: Text.NativeRendering
        }
    }

    onIconSizeChanged: scheduleInteractionTargetGeometryUpdate()
    onVisualScaleChanged: scheduleInteractionTargetGeometryUpdate()
    onVisualOffsetXChanged: scheduleInteractionTargetGeometryUpdate()
    onVisualOffsetYChanged: scheduleInteractionTargetGeometryUpdate()
    Connections {
        target: root.dockPanel
        function onWidthChanged() { root.scheduleInteractionTargetGeometryUpdate() }
        function onHeightChanged() { root.scheduleInteractionTargetGeometryUpdate() }
    }
    Connections {
        target: DockController
        function onConfigChanged() { root.scheduleInteractionTargetGeometryUpdate() }
    }
    Component.onCompleted: root.updateInteractionTargetGeometry()
}
