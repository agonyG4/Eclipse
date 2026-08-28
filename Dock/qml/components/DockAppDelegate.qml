import QtQuick
import QtQuick.Controls.Basic
import Astrea.Shared as Shared

Item {
    id: root

    required property int index
    required property var model
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
    required property int slotWidth
    required property int slotHeight
    property var dockPanel
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
    property string outputKey: ""
    property bool pointerTarget: false
    property real magnificationScale: 1.0
    property real visualOffsetX: 0
    property real visualOffsetY: 0
    property bool dragging: false
    property real dragScale: 1.0
    property bool dragWasActive: false
    property bool hasLastDragScenePosition: false
    property real lastDragSceneX: 0
    property real lastDragSceneY: 0
    readonly property real visualScale: root.magnificationScale * root.dragScale
    readonly property rect interactionRegion: Qt.rect(interactionTarget.x,
                                                       interactionTarget.y,
                                                       interactionTarget.width,
                                                       interactionTarget.height)
    // 0 = idle, 1 = exclusively grabbed and dragging.
    property int dragLifecycle: 0

    property int iconSize: 48
    signal activated(string desktopFileName)
    signal dragStarted(string desktopFileName)
    signal dragMoved(string desktopFileName, real translationX, real sceneX, real sceneY)
    signal dragFinished(string desktopFileName)
    signal dragCanceled(string desktopFileName)

    function recordDragPointer(sceneX, sceneY) {
        if (!isFinite(sceneX) || !isFinite(sceneY))
            return false
        root.hasLastDragScenePosition = true
        root.lastDragSceneX = sceneX
        root.lastDragSceneY = sceneY
        if (root.dockPanel)
            root.dockPanel.updatePointerFromScene(sceneX, sceneY)
        return true
    }

    function restoreLastDragPointer() {
        if (root.dockPanel && root.hasLastDragScenePosition)
            root.dockPanel.updatePointerFromScene(root.lastDragSceneX,
                                                  root.lastDragSceneY)
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
            root.dockPanel.schedulePointerSemanticRefresh()
        }
    }

    function scheduleInteractionTargetGeometryUpdate() {
        Qt.callLater(root.updateInteractionTargetGeometry)
    }

    function isPointerTarget() {
        return !root.dockPanel
            || root.dockPanel.pointerTargetDesktopFileName === root.desktopFileName
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

    objectName: desktopFileName
    width: slotWidth
    height: slotHeight

    SystemPalette { id: systemPalette }

    Shared.AstreaAppIcon {
        id: appIcon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize
        height: root.iconSize
        scale: root.magnificationScale * root.dragScale
        transformOrigin: Item.Bottom
        iconName: root.iconName
        iconPath: root.iconPath
        iconUrl: root.iconUrl
        appName: root.displayName
        // 2x source sampling remains sharp at the configured 2.0 maximum.
        sourcePixelSize: root.iconSize * 2
        iconRadius: 10
        fallbackRadius: 10
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
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on visualOffsetY {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on magnificationScale {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on dragScale {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }

    Item {
        id: interactionTarget
        objectName: "interactionTarget-" + root.desktopFileName
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
                const bottomCenter = appIcon.mapToItem(root.dockPanel,
                                                        appIcon.width / 2, appIcon.height)
                x = bottomCenter.x - targetSize / 2
                y = bottomCenter.y - targetSize
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
                    root.activated(root.desktopFileName)
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
                    DockController.bottomMargin,
                    Qt.rect(topLeft.x, topLeft.y, root.width, root.height))
                if (root.contextMenuController.debugEnabled) {
                    console.log("astrea.context-menu " + JSON.stringify({
                        stage: "dock-anchor-resolved",
                        targetIdentity: root.desktopFileName,
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
                root.contextMenuController.presentDock(root.desktopFileName,
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
                    root.hasLastDragScenePosition = false
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
                root.dragMoved(root.desktopFileName, activeTranslation.x,
                               scenePosition.x, scenePosition.y)
            }
        }
    }

    ToolTip.visible: root.pointerTarget
    ToolTip.delay: 420
    ToolTip.text: root.runtimeKnown && root.running
        ? qsTr("%1 (%2 windows)").arg(root.displayName).arg(root.windowCount)
        : qsTr("%1").arg(root.displayName)

    /*
     * The feedback rectangle follows the transformed icon, but remains below
     * the exact interaction target so transparent headroom is not clickable.
     */
    Rectangle {
        z: -1
        anchors.horizontalCenter: appIcon.horizontalCenter
        anchors.bottom: appIcon.bottom
        width: appIcon.width
        height: appIcon.height
        scale: appIcon.scale
        transformOrigin: Item.Bottom
        radius: appIcon.iconRadius
        color: "#18FFFFFF"
        visible: tapHandler.pressed || dragHandler.active
    }

    /*
     * The running indicator intentionally stays outside the icon transform.
     */
    Rectangle {
        anchors.horizontalCenter: appIcon.horizontalCenter
        anchors.top: appIcon.bottom
        anchors.topMargin: 2
        width: root.active ? 18 : 8
        height: 3
        radius: 1.5
        color: systemPalette.highlight
        visible: root.runtimeKnown && root.running
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
    Component.onCompleted: root.updateInteractionTargetGeometry()
}
