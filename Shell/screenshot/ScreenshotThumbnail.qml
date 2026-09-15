import QtQuick
import QtQuick.Window

Window {
    id: root

    objectName: "screenshotThumbnailSurface"
    title: "Astrea Screenshot"
    visible: ScreenshotController.thumbnailVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY

    property real dragThreshold: 8
    property real dismissThreshold: 96
    property bool held: false
    property bool dragging: false
    property bool hovered: false
    property bool dismissing: false
    property bool entering: false
    property bool fileDragArmed: false
    property bool nativeFileDragging: false
    property bool nativeDragConsumedPress: false
    property bool nativeDragTestMode: false
    property int nativeDragStartCount: 0
    property point dragStart: Qt.point(0, 0)
    property point nativeDragHotSpot: Qt.point(0, 0)
    property point slotPosition: Qt.point(width - card.width - 20,
                                          height - card.height - 20)
    property real aspectRatio: ScreenshotController.imageHeight > 0
                                ? ScreenshotController.imageWidth / ScreenshotController.imageHeight
                                : 1.5
    readonly property real nativeDragDistance: Math.max(
        dragThreshold, Qt.styleHints.startDragDistance || dragThreshold)
    readonly property var nativeDragMimeData: card.Drag.mimeData
    readonly property int nativeDragSupportedActions: card.Drag.supportedActions
    readonly property int nativeDragProposedAction: card.Drag.proposedAction

    function decideGesture(dx, dy) {
        if (dx >= dismissThreshold)
            return "dismiss"
        if (Math.abs(dx) >= dragThreshold || Math.abs(dy) >= dragThreshold)
            return "return"
        return "click"
    }

    function updateGeometry() {
        const scaledWidth = card.width * card.scale
        const scaledHeight = card.height * card.scale
        const inputRect = Qt.rect(card.x - (scaledWidth - card.width) / 2,
                                  card.y - (scaledHeight - card.height) / 2,
                                  scaledWidth, scaledHeight)
        if (ScreenshotInputRegion)
            ScreenshotInputRegion.update(inputRect)
        ScreenshotController.thumbnailRect = inputRect
    }

    function restoreSlot() {
        card.x = slotPosition.x
        card.y = slotPosition.y
    }

    function pointerPosition(mouse) {
        return cardMouseArea.mapToItem(null, mouse.x, mouse.y)
    }

    function resetPressState() {
        fileDragArmTimer.stop()
        held = false
        dragging = false
        fileDragArmed = false
    }

    function startNativeFileDrag() {
        if (!held || !fileDragArmed || nativeFileDragging
                || !ScreenshotController.resultUri)
            return

        nativeFileDragging = true
        nativeDragConsumedPress = true
        fileDragArmTimer.stop()
        held = false
        dragging = false
        fileDragArmed = false
        if (ScreenshotInputRegion)
            ScreenshotInputRegion.suspend()
        nativeDragStartCount += 1
        if (nativeDragTestMode)
            return

        card.Drag.active = true
        card.Drag.startDrag(Qt.CopyAction)
        card.Drag.active = false
    }

    function finishNativeFileDrag(dropAction) {
        nativeFileDragging = false
        dragging = false
        fileDragArmed = false
        if (dropAction === Qt.CopyAction) {
            ScreenshotController.dismissThumbnail()
            return true
        }

        restoreSlot()
        updateGeometry()
        return true
    }

    function dismiss() {
        if (dismissing)
            return
        dismissing = true
        dismissAnimation.restart()
    }

    onVisibleChanged: {
        if (visible) {
            entering = true
            dismissing = false
            nativeFileDragging = false
            nativeDragConsumedPress = false
            resetPressState()
            restoreSlot()
            updateGeometry()
            Qt.callLater(function() {
                if (root.visible)
                    root.entering = false
            })
        }
    }

    Item {
        id: card
        objectName: "thumbnailCard"
        width: Math.min(240, Math.max(1, 160 * root.aspectRatio))
        height: Math.min(160, Math.max(1, 240 / root.aspectRatio))
        x: root.slotPosition.x
        y: root.slotPosition.y
        scale: root.entering ? 0.94
                             : (root.nativeFileDragging ? 1.0
                                : (root.fileDragArmed ? 1.05
                                   : (root.held || root.dragging ? 1.03 : 1.0)))
        opacity: root.nativeFileDragging ? 0 : (root.entering ? 0 : 1)

        Behavior on x {
            enabled: !root.dragging && !root.dismissing
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        Behavior on y {
            enabled: !root.dragging && !root.dismissing
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        onXChanged: root.updateGeometry()
        onYChanged: root.updateGeometry()
        onScaleChanged: root.updateGeometry()

        Rectangle {
            x: 2
            y: 4
            width: parent.width
            height: parent.height
            radius: 11
            color: "#40000000"
            z: -1
        }
        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "#b315171c"
            border.color: "#b3ffffff"
            border.width: 1
        }
        Image {
            anchors.fill: parent
            anchors.margins: 3
            source: ScreenshotController.imageSource
            fillMode: Image.PreserveAspectFit
            asynchronous: false
            cache: false
        }

        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction
        Drag.proposedAction: Qt.CopyAction
        Drag.mimeData: ({
            "text/uri-list": ScreenshotController.resultUri,
            "text/plain": ScreenshotController.resultPath
        })
        Drag.imageSource: ScreenshotController.imageSource
        Drag.imageSourceSize: Qt.size(Math.max(1, Math.round(width)),
                                      Math.max(1, Math.round(height)))
        Drag.hotSpot: root.nativeDragHotSpot

        Drag.onDragFinished: function(dropAction) {
            root.finishNativeFileDrag(dropAction)
        }

        MouseArea {
            id: cardMouseArea
            objectName: "thumbnailMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onEntered: root.hovered = true
            onExited: root.hovered = false
            onPressed: function(mouse) {
                root.nativeDragConsumedPress = false
                root.held = true
                root.dragging = false
                root.fileDragArmed = false
                root.dragStart = root.pointerPosition(mouse)
                root.nativeDragHotSpot = Qt.point(mouse.x, mouse.y)
                fileDragArmTimer.restart()
            }
            onPositionChanged: function(mouse) {
                if (!root.held)
                    return
                const current = root.pointerPosition(mouse)
                const dx = current.x - root.dragStart.x
                const dy = current.y - root.dragStart.y
                if (root.fileDragArmed
                        && Math.sqrt(dx * dx + dy * dy) >= root.nativeDragDistance) {
                    root.startNativeFileDrag()
                    return
                }
                if (!root.dragging && Math.sqrt(dx * dx + dy * dy) >= root.dragThreshold)
                    root.dragging = true
                if (root.dragging) {
                    card.x = root.slotPosition.x + dx
                    card.y = root.slotPosition.y + dy
                    root.updateGeometry()
                }
            }
            onReleased: function(mouse) {
                if (root.nativeDragConsumedPress) {
                    root.nativeDragConsumedPress = false
                    root.resetPressState()
                    return
                }
                if (!root.held)
                    return
                const current = root.pointerPosition(mouse)
                const dx = current.x - root.dragStart.x
                const dy = current.y - root.dragStart.y
                const decision = root.decideGesture(dx, dy)
                root.resetPressState()
                if (decision === "click") {
                    ScreenshotController.openPreview()
                } else if (decision === "dismiss") {
                    root.dismiss()
                } else {
                    root.dragging = false
                    root.restoreSlot()
                    root.updateGeometry()
                }
            }
            onCanceled: {
                if (root.nativeDragConsumedPress) {
                    root.nativeDragConsumedPress = false
                    root.resetPressState()
                    return
                }
                root.resetPressState()
            }
        }
    }

    Timer {
        id: fileDragArmTimer
        objectName: "fileDragArmTimer"
        interval: 220
        repeat: false
        onTriggered: {
            if (root.held && !root.dragging && !root.nativeFileDragging)
                root.fileDragArmed = true
        }
    }

    Timer {
        id: idleTimer
        objectName: "idleTimer"
        interval: 5000
        repeat: false
        running: root.visible && !root.hovered && !root.held && !root.dragging
                 && !root.fileDragArmed && !root.nativeFileDragging
                 && !root.dismissing && !root.entering
        onTriggered: ScreenshotController.dismissThumbnail()
    }

    ParallelAnimation {
        id: dismissAnimation
        NumberAnimation {
            target: card
            property: "x"
            to: root.width + 120
            duration: 220
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: card
            property: "opacity"
            to: 0
            duration: 220
            easing.type: Easing.OutCubic
        }
        onStopped: {
            root.dismissing = false
            root.dragging = false
            root.restoreSlot()
            ScreenshotController.dismissThumbnail()
        }
    }

    onWidthChanged: root.updateGeometry()
    onHeightChanged: root.updateGeometry()
}
