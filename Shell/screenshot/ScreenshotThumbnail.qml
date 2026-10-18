import QtQuick
import QtQuick.Window
import "./" as ScreenshotComponents

Window {
    id: root

    objectName: "screenshotThumbnailSurface"
    title: "Astrea Screenshot"
    property bool surfaceReady: false
    visible: surfaceReady && ScreenshotController.thumbnailVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY

    property var configuration: ScreenshotController.configuration
    readonly property bool demo: ScreenshotController.demonstrationActive
    readonly property bool demoStopping: ScreenshotController.demonstrationStopping
    property string lifecycleState: "hidden"
    property real lifecycleProgress: 1.0
    property int animationToken: 0
    property int activeAnimationToken: 0
    property string activeImageSource: ""
    property string activeAnimationStyle: "scale-fade"
    property string activeAnimationEasing: "out-cubic"
    property int activeAnimationDurationMs: 0
    property real idleRemainingMs: configuration.displayDurationMs
    property int appliedDisplayDurationMs: configuration.displayDurationMs
    property double idleDeadlineMs: 0
    property bool held: false
    property bool hovered: false
    property bool nativeFileDragging: false
    property bool pendingCaptureRefresh: false
    property bool nativeDragConsumedPress: false
    property bool nativeDragTestMode: false
    property int nativeDragStartCount: 0
    property point dragStart: Qt.point(0, 0)
    property point nativeDragHotSpot: Qt.point(0, 0)
    property var nativeDragSnapshot: null
    property var nativeDragPayload: ({})

    readonly property bool entering: lifecycleState === "entering"
    readonly property bool dismissing: lifecycleState === "dismissing"
    readonly property real aspectRatio: ScreenshotController.imageHeight > 0
        ? ScreenshotController.imageWidth / ScreenshotController.imageHeight : 1.5
    readonly property real dragThreshold: 8
    readonly property real nativeDragDistance: Math.max(
        dragThreshold, Qt.styleHints.startDragDistance || dragThreshold)
    readonly property var nativeDragMimeData: nativeFileDragging
        ? nativeDragPayload : ({
            "text/uri-list": ScreenshotController.resultUri + "\r\n",
            "text/plain": ScreenshotController.resultPath,
            "image/png": ScreenshotController.imagePngData
        })
    readonly property int nativeDragSupportedActions: card.Drag.supportedActions
    readonly property int nativeDragProposedAction: card.Drag.proposedAction
    readonly property bool nativeDragUsesManualStart: card.Drag.dragType === Drag.None
    readonly property real frameWidth: {
        if (nativeDragSnapshot)
            return nativeDragSnapshot.frameWidth
        const boxWidth = effectiveSetting("thumbnailWidth")
        const boxHeight = effectiveSetting("thumbnailHeight")
        if (ScreenshotController.imageWidth <= 0 || ScreenshotController.imageHeight <= 0)
            return boxWidth
        return Math.max(1, Math.min(boxWidth,
            boxHeight * ScreenshotController.imageWidth / ScreenshotController.imageHeight))
    }
    readonly property real frameHeight: {
        if (nativeDragSnapshot)
            return nativeDragSnapshot.frameHeight
        const boxWidth = effectiveSetting("thumbnailWidth")
        const boxHeight = effectiveSetting("thumbnailHeight")
        if (ScreenshotController.imageWidth <= 0 || ScreenshotController.imageHeight <= 0)
            return boxHeight
        return Math.max(1, Math.min(boxHeight,
            boxWidth * ScreenshotController.imageHeight / ScreenshotController.imageWidth))
    }
    readonly property point slotPosition: {
        const margin = effectiveSetting("edgeMargin")
        const position = effectiveSetting("position")
        const left = position === "bottom-left" || position === "top-left"
        const top = position === "top-left" || position === "top-right"
        return Qt.point(left ? margin : width - margin - card.width,
                        top ? margin : height - margin - card.height)
    }
    readonly property real entryOffsetX: activeAnimationStyle === "slide-fade"
        ? ((effectiveSetting("position").endsWith("right") ? 1 : -1)
           * (card.width + effectiveSetting("edgeMargin"))) : 0
    readonly property real entryOffsetY: activeAnimationStyle === "slide-fade"
        ? ((effectiveSetting("position").startsWith("top") ? -1 : 1)
           * (card.height + effectiveSetting("edgeMargin"))) : 0
    // Exits travel horizontally toward the configured screen edge; the
    // entry direction can retain its existing diagonal placement.
    readonly property real exitOffsetX: activeAnimationStyle === "slide-fade"
        ? ((effectiveSetting("position").endsWith("right") ? 1 : -1)
           * (card.width + effectiveSetting("edgeMargin"))) : 0
    readonly property real transitionOffsetX: entering
        ? entryOffsetX * (1 - lifecycleProgress)
        : (dismissing ? exitOffsetX * lifecycleProgress : 0)
    readonly property real transitionOffsetY: entering
        ? entryOffsetY * (1 - lifecycleProgress) : 0
    readonly property real presentationOpacity: entering ? lifecycleProgress
        : (dismissing ? 1 - lifecycleProgress : (lifecycleState === "visible" ? 1 : 0))
    readonly property real presentationScale: activeAnimationStyle === "scale-fade"
        ? (entering ? 0.92 + 0.08 * lifecycleProgress
                    : (dismissing ? 1.0 - 0.04 * lifecycleProgress : 1.0))
        : 1.0

    function effectiveSetting(name) {
        return nativeDragSnapshot ? nativeDragSnapshot[name] : configuration[name]
    }

    function easingValue(name) {
        if (name === "linear") return Easing.Linear
        if (name === "in-quad") return Easing.InQuad
        if (name === "in-out-cubic") return Easing.InOutCubic
        return Easing.OutCubic
    }

    function updateGeometry() {
        if (!visible || nativeFileDragging)
            return
        const scale = card.scale
        const x = card.x + (card.width * (1 - scale) / 2)
        const y = card.y + (card.height * (1 - scale) / 2)
        const inputRect = Qt.rect(x, y, card.width * scale, card.height * scale)
        if (ScreenshotInputRegion)
            ScreenshotInputRegion.update(inputRect)
        ScreenshotController.thumbnailRect = inputRect
    }

    function pointerPosition(mouse) {
        return cardMouseArea.mapToItem(null, mouse.x, mouse.y)
    }

    function resetPressState() {
        held = false
        resumeIdleCountdown()
    }

    function idleIsSuspended() {
        return demo || hovered || held || nativeFileDragging || !visible
            || lifecycleState !== "visible" || ScreenshotController.previewVisible
    }

    function pauseIdleCountdown() {
        if (idleTimer.running) {
            idleRemainingMs = Math.max(0, idleDeadlineMs - Date.now())
            idleTimer.stop()
        }
        if (dismissing)
            cancelDismissalForInteraction()
    }

    function resumeIdleCountdown() {
        if (idleIsSuspended()) {
            idleTimer.stop()
            return
        }
        if (idleTimer.running)
            return
        if (idleRemainingMs <= 0)
            idleRemainingMs = configuration.displayDurationMs
        idleDeadlineMs = Date.now() + idleRemainingMs
        idleTimer.interval = Math.max(1, Math.ceil(idleRemainingMs))
        idleTimer.start()
    }

    function startIdleCountdown() {
        idleRemainingMs = configuration.displayDurationMs
        appliedDisplayDurationMs = configuration.displayDurationMs
        idleTimer.stop()
        resumeIdleCountdown()
    }

    function startEntry() {
        ++animationToken
        activeAnimationToken = animationToken
        activeImageSource = ScreenshotController.imageSource
        activeAnimationStyle = configuration.entryAnimation
        activeAnimationEasing = configuration.entryEasing
        activeAnimationDurationMs = configuration.entryDurationMs
        lifecycleState = "entering"
        lifecycleProgress = 0
        idleTimer.stop()
        idleRemainingMs = configuration.displayDurationMs
        updateGeometry()

        if (!configuration.animationsEnabled || activeAnimationDurationMs <= 0) {
            lifecycleProgress = 1
            lifecycleState = "visible"
            updateGeometry()
            resumeIdleCountdown()
            return
        }
        lifecycleAnimation.duration = activeAnimationDurationMs
        lifecycleAnimation.easing.type = easingValue(activeAnimationEasing)
        lifecycleAnimation.restart()
    }

    function beginDismissal(force) {
        if (!visible || (demo && !demoStopping) || dismissing || nativeFileDragging || lifecycleState === "hidden")
            return
        if (!force && (held || hovered)) {
            idleRemainingMs = configuration.displayDurationMs
            return
        }

        idleTimer.stop()
        ++animationToken
        activeAnimationToken = animationToken
        activeImageSource = ScreenshotController.imageSource
        activeAnimationStyle = configuration.exitAnimation
        activeAnimationEasing = configuration.exitEasing
        activeAnimationDurationMs = configuration.exitDurationMs
        lifecycleState = "dismissing"
        lifecycleProgress = 0

        if (!configuration.animationsEnabled || activeAnimationDurationMs <= 0) {
            completeDismissal(animationToken, activeImageSource)
            return
        }
        lifecycleAnimation.duration = activeAnimationDurationMs
        lifecycleAnimation.easing.type = easingValue(activeAnimationEasing)
        lifecycleAnimation.restart()
    }

    function completeDismissal(token, imageSource) {
        if (token !== animationToken || token !== activeAnimationToken
                || lifecycleState !== "dismissing" || imageSource !== ScreenshotController.imageSource
                || !visible || nativeFileDragging)
            return
        lifecycleState = "hidden"
        lifecycleProgress = 1
        if (demoStopping) ScreenshotController.finishDemonstrationStop()
        else ScreenshotController.dismissThumbnail()
    }

    function finishLifecycleAnimation() {
        const token = activeAnimationToken
        const imageSource = activeImageSource
        if (token !== animationToken || imageSource !== ScreenshotController.imageSource)
            return
        if (lifecycleState === "entering") {
            lifecycleProgress = 1
            lifecycleState = "visible"
            updateGeometry()
            startIdleCountdown()
        } else if (lifecycleState === "dismissing") {
            completeDismissal(token, imageSource)
        }
    }

    function cancelDismissalForInteraction() {
        if (!dismissing)
            return
        ++animationToken
        lifecycleAnimation.stop()
        lifecycleState = "visible"
        lifecycleProgress = 1
        idleRemainingMs = configuration.displayDurationMs
        updateGeometry()
    }

    function startNativeFileDrag() {
        if (demo || !configuration.dragEnabled || !held || nativeFileDragging
                || !ScreenshotController.resultUri || lifecycleState !== "visible")
            return

        nativeDragSnapshot = ({
            cornerRadius: configuration.cornerRadius,
            borderEnabled: configuration.borderEnabled,
            borderWidth: configuration.borderWidth,
            borderColor: configuration.borderColor,
            shadowEnabled: configuration.shadowEnabled,
            shadowStrength: configuration.shadowStrength,
            thumbnailWidth: configuration.thumbnailWidth,
            thumbnailHeight: configuration.thumbnailHeight,
            position: configuration.position,
            edgeMargin: configuration.edgeMargin,
            frameWidth: card.width,
            frameHeight: card.height
        })
        nativeDragPayload = ({
            "text/uri-list": ScreenshotController.resultUri + "\r\n",
            "text/plain": ScreenshotController.resultPath,
            "image/png": ScreenshotController.imagePngData
        })
        nativeDragSource = ScreenshotController.imageSource
        nativeFileDragging = true
        nativeDragConsumedPress = true
        held = false
        idleTimer.stop()
        ScreenshotController.setThumbnailDragActive(true)
        nativeDragStartCount += 1
        if (nativeDragTestMode)
            return

        card.Drag.active = true
        card.Drag.startDrag(Qt.CopyAction)
    }

    property url nativeDragSource: ""

    function finishNativeFileDrag(dropAction) {
        if (!nativeFileDragging)
            return true
        const refreshForNewCapture = pendingCaptureRefresh
        nativeFileDragging = false
        ScreenshotController.setThumbnailDragActive(false)
        nativeDragSnapshot = null
        nativeDragPayload = ({})
        nativeDragSource = ""
        pendingCaptureRefresh = false
        updateGeometry()
        if (refreshForNewCapture && ScreenshotController.thumbnailVisible
                && configuration.showThumbnail) {
            startEntry()
            return true
        }
        if (dropAction === Qt.CopyAction) {
            // An accepted native drop hands the screenshot to the target app.
            // The dragged image must not fly away a second time afterward.
            idleTimer.stop()
            ++animationToken
            lifecycleAnimation.stop()
            lifecycleState = "hidden"
            lifecycleProgress = 1
            ScreenshotController.dismissThumbnail()
            return true
        }
        if (!configuration.showThumbnail) {
            beginDismissal(true)
            return true
        }
        resumeIdleCountdown()
        return true
    }

    function activateClickAction() {
        if (demo) {
            ScreenshotController.openPreview()
            return
        }
        if (configuration.clickAction === "copy") {
            ScreenshotController.copyScreenshot()
            beginDismissal(true)
        } else if (configuration.clickAction === "dismiss") {
            beginDismissal(true)
        } else {
            ScreenshotController.openPreview()
        }
    }

    onVisibleChanged: {
        if (visible) {
            nativeFileDragging = false
            nativeDragConsumedPress = false
            held = false
            hovered = false
            startEntry()
            updateGeometry()
        } else {
            ++animationToken
            lifecycleAnimation.stop()
            idleTimer.stop()
            lifecycleProgress = 1
            lifecycleState = "hidden"
            if (!nativeFileDragging && ScreenshotInputRegion)
                ScreenshotInputRegion.suspend()
        }
    }

    onHoveredChanged: {
        if (hovered)
            pauseIdleCountdown()
        else
            resumeIdleCountdown()
    }
    onHeldChanged: {
        if (held)
            pauseIdleCountdown()
        else
            resumeIdleCountdown()
    }

    onWidthChanged: updateGeometry()
    onHeightChanged: updateGeometry()
    Item {
        id: card
        objectName: "thumbnailCard"
        width: root.frameWidth
        height: root.frameHeight
        x: root.slotPosition.x + root.transitionOffsetX
        y: root.slotPosition.y + root.transitionOffsetY
        scale: root.presentationScale
        opacity: root.nativeFileDragging ? 0 : root.presentationOpacity

        onXChanged: root.updateGeometry()
        onYChanged: root.updateGeometry()
        onScaleChanged: root.updateGeometry()
        onWidthChanged: root.updateGeometry()
        onHeightChanged: root.updateGeometry()

        ScreenshotComponents.ScreenshotVisual {
            id: screenshotVisual
            objectName: "thumbnailVisual"
            x: -shadowPadding
            y: -shadowPadding
            imageWidth: card.width
            imageHeight: card.height
            source: root.nativeDragSnapshot ? root.nativeDragSource
                                           : ScreenshotController.imageSource
            cornerRadius: root.effectiveSetting("cornerRadius")
            borderEnabled: root.effectiveSetting("borderEnabled")
            borderWidth: root.effectiveSetting("borderWidth")
            borderColor: root.effectiveSetting("borderColor")
            shadowEnabled: root.effectiveSetting("shadowEnabled")
            shadowStrength: root.effectiveSetting("shadowStrength")
        }

        Drag.dragType: Drag.None
        Drag.supportedActions: Qt.CopyAction
        Drag.proposedAction: Qt.CopyAction
        Drag.mimeData: root.nativeDragPayload
        Drag.imageSource: root.nativeDragSource || ScreenshotController.imageSource
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
            onEntered: {
                root.hovered = true
                root.pauseIdleCountdown()
            }
            onExited: {
                root.hovered = false
                root.resumeIdleCountdown()
            }
            onPressed: function(mouse) {
                root.nativeDragConsumedPress = false
                root.held = true
                root.pauseIdleCountdown()
                root.dragStart = root.pointerPosition(mouse)
                root.nativeDragHotSpot = Qt.point(mouse.x, mouse.y)
            }
            onPositionChanged: function(mouse) {
                if (root.demo || !root.held || !root.configuration.dragEnabled)
                    return
                const current = root.pointerPosition(mouse)
                const dx = current.x - root.dragStart.x
                const dy = current.y - root.dragStart.y
                if (Math.sqrt(dx * dx + dy * dy) >= root.nativeDragDistance)
                    root.startNativeFileDrag()
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
                const distance = Math.sqrt(dx * dx + dy * dy)
                root.resetPressState()
                if (distance < root.nativeDragDistance)
                    root.activateClickAction()
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
        id: idleTimer
        objectName: "idleTimer"
        interval: Math.max(1, Math.ceil(root.idleRemainingMs))
        repeat: false
        onTriggered: root.beginDismissal()
    }

    Connections {
        target: ScreenshotController
        function onDemonstrationStoppingChanged() {
            if (root.demoStopping) root.beginDismissal(true)
        }
    }

    Connections {
        target: ScreenshotController
        function onImageChanged() {
            if (!root.visible)
                return
            if (root.nativeFileDragging) {
                root.pendingCaptureRefresh = true
                return
            }
            root.pendingCaptureRefresh = false
            root.startEntry()
        }
    }

    Connections {
        target: root.configuration
        function onConfigurationChanged() {
            if (!root.nativeFileDragging && !root.held && !root.hovered)
                root.updateGeometry()
            if (root.appliedDisplayDurationMs !== root.configuration.displayDurationMs) {
                root.appliedDisplayDurationMs = root.configuration.displayDurationMs
                root.idleRemainingMs = root.configuration.displayDurationMs
                if (root.lifecycleState === "visible" && !root.idleIsSuspended())
                    root.startIdleCountdown()
            }
        }
    }

    NumberAnimation {
        id: lifecycleAnimation
        target: root
        property: "lifecycleProgress"
        from: 0
        to: 1
        duration: 220
        easing.type: Easing.OutCubic
        onFinished: root.finishLifecycleAnimation()
    }
}
