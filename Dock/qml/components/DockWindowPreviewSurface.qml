import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Astrea.Effects
import Astrea.Shared as Shared

Window {
    id: root
    objectName: "dockWindowPreviewSurface"

    property bool previewVisible: false
    property bool pointerHovered: false
    property string taskKey: ""
    property rect anchorRect: Qt.rect(0, 0, 0, 0)
    property string dockPosition: "bottom"
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
    property string outputKey: ""
    property var entries: []
    property var dockPanel: null
    property var previewService: null
    property var dockSurfaceGeometry: null
    property var inputRegionBridge: null
    property int edgeInset: 12
    property int dockGap: 10

    // A minimized window dragged out of the Dock, drawn over the desktop in
    // output-local coordinates while the Dock keeps the pointer grab. It grows
    // from the tile to the window's real size, and on release the compositor
    // restores the window exactly underneath it before it fades away.
    property bool ghostVisible: false
    property string ghostWindowId: ""
    property string ghostThumbnail: ""
    property string ghostDetail: ""
    property real ghostX: 0
    property real ghostY: 0
    property real ghostWidth: 1
    property real ghostHeight: 1
    property real ghostRadius: 8
    // 0 at the Dock, 1 fully pulled out: deepens the shadow like a lifted window.
    property real ghostLift: 0
    property var ghostFinished: null

    Shared.ShellMaterialTheme {
        typographySurface: "dock"
        id: material
        objectName: "dockWindowPreviewMaterial"
    }

    readonly property color accent: material.tokens.value("accent", "#007aff")
    readonly property real panelRadius: material.tokens.value("dockPreviewRadius", 22)
    // Read through root: RectangularShadow's own `material` shadows the id.
    readonly property real thumbnailRadius: material.radiusMedium
    readonly property int panelPadding: 8
    readonly property int cardSpacing: 4
    readonly property int cardPadding: 8
    readonly property int thumbnailWidth: 200
    readonly property int thumbnailHeight: 124
    readonly property int titleHeight: Math.max(22, Math.ceil(
        material.typography.role("label").pixelSize * material.typography.role("label").lineHeight) + 4)
    readonly property int cardWidth: thumbnailWidth + 2 * cardPadding
    readonly property int cardHeight: thumbnailHeight + titleHeight + 3 * cardPadding
    readonly property int columns: Math.max(1, Math.floor(
        (Math.max(1, outputWidth - 2 * edgeInset - 2 * panelPadding) + cardSpacing)
        / (cardWidth + cardSpacing)))
    readonly property int rowCount: Math.ceil(entries.length / columns)
    readonly property int naturalHeight: rowCount * (cardHeight + cardSpacing)
        - cardSpacing + 2 * panelPadding
    readonly property int surfaceWidth: Math.min(Math.max(1, outputWidth - 2 * edgeInset),
        Math.min(entries.length, columns) * (cardWidth + cardSpacing) - cardSpacing
        + 2 * panelPadding)
    readonly property int surfaceHeight: Math.min(Math.max(1, outputHeight - 2 * edgeInset),
                                                  naturalHeight)
    readonly property int preferredX: dockPosition === "left"
        ? anchorRect.x + anchorRect.width + dockGap
        : dockPosition === "right" ? anchorRect.x - surfaceWidth - dockGap
                                    : anchorRect.x + anchorRect.width / 2 - surfaceWidth / 2
    readonly property int clampedX: Math.max(edgeInset,
        Math.min(outputWidth - edgeInset - surfaceWidth, preferredX))
    readonly property int preferredY: dockPosition === "bottom"
        ? anchorRect.y - surfaceHeight - dockGap
        : anchorRect.y + anchorRect.height / 2 - surfaceHeight / 2
    readonly property int clampedY: Math.max(edgeInset,
        Math.min(outputHeight - edgeInset - surfaceHeight, preferredY))
    readonly property int panelTransformOrigin: dockPosition === "left" ? Item.Left
        : dockPosition === "right" ? Item.Right : Item.Bottom

    visible: previewVisible || ghostVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus
        | Qt.WindowStaysOnTopHint
    width: Math.max(1, outputWidth)
    height: Math.max(1, outputHeight)

    function updateInputRegion() {
        if (!inputRegionBridge)
            return
        if (previewVisible)
            inputRegionBridge.update(Qt.rect(clampedX, clampedY, surfaceWidth, surfaceHeight))
        else
            inputRegionBridge.suspend()
    }

    onClampedXChanged: updateInputRegion()
    onClampedYChanged: updateInputRegion()
    onSurfaceWidthChanged: updateInputRegion()
    onSurfaceHeightChanged: updateInputRegion()

    // Cards cascade in only when the popup opens; cards rebuilt while it is
    // already showing (a title changed) appear in place.
    property bool cardsSettled: false
    Timer {
        id: cardsSettledTimer
        interval: 450
        onTriggered: root.cardsSettled = true
    }
    onPreviewVisibleChanged: {
        Qt.callLater(updateInputRegion)
        cardsSettled = false
        if (previewVisible)
            cardsSettledTimer.restart()
        else
            cardsSettledTimer.stop()
    }

    function sameEntries(left, right) {
        if (left.length !== right.length)
            return false
        for (var i = 0; i < left.length; ++i) {
            if (left[i].windowId !== right[i].windowId || left[i].title !== right[i].title)
                return false
        }
        return true
    }

    function showPreview(key, windowEntries, visualAnchor, position, widthIn, heightIn,
                         originX, originY) {
        const nextEntries = windowEntries || []
        // Runtime snapshots re-announce the same windows on every focus or
        // title change. Keep the cards (and their images) instead of
        // rebuilding them, which flashed the app icon and replayed the entrance.
        const unchanged = previewVisible && key === taskKey && sameEntries(entries, nextEntries)
        taskKey = key
        if (!unchanged)
            entries = nextEntries
        anchorRect = visualAnchor
        dockPosition = position
        outputWidth = Math.max(1, widthIn)
        outputHeight = Math.max(1, heightIn)
        outputOriginX = originX || 0
        outputOriginY = originY || 0
        previewVisible = entries.length >= 2
        if (unchanged)
            return
        if (previewVisible) {
            if (previewService)
                previewService.setPopupWindowIds(entries.map(function(entry) {
                    return entry.windowId
                }))
            refreshImages()
        } else if (previewService) {
            previewService.setPopupWindowIds([])
        }
    }

    function showGhost(windowId, thumbnail, widthIn, heightIn, originX, originY) {
        ghostSettleGuard.stop()
        ghostHideGuard.stop()
        ghostAnimation.stop()
        ghostFade.stop()
        if (previewVisible)
            hidePreview()
        outputWidth = Math.max(1, widthIn)
        outputHeight = Math.max(1, heightIn)
        outputOriginX = originX || 0
        outputOriginY = originY || 0
        ghostWindowId = windowId
        ghostThumbnail = thumbnail || ""
        ghostDetail = previewService ? previewService.detailImageUrl(windowId) : ""
        ghostItem.opacity = 1
        ghostFinished = null
        ghostVisible = true
        if (previewService)
            previewService.requestDetailImage(windowId)
        Qt.callLater(updateInputRegion)
    }

    function moveGhost(x, y, w, h, radius, lift) {
        if (!ghostVisible || ghostAnimation.running)
            return
        ghostX = x
        ghostY = y
        ghostWidth = Math.max(1, w)
        ghostHeight = Math.max(1, h)
        ghostRadius = radius
        ghostLift = lift
    }

    // Animates the ghost to a rect, then runs `finished` (if any).
    function animateGhost(x, y, w, h, radius, lift, duration, finished) {
        if (!ghostVisible)
            return
        ghostFinished = finished || null
        ghostXAnimation.to = x
        ghostYAnimation.to = y
        ghostWidthAnimation.to = Math.max(1, w)
        ghostHeightAnimation.to = Math.max(1, h)
        ghostRadiusAnimation.to = radius
        ghostLiftAnimation.to = lift
        ghostAnimation.duration = duration
        ghostAnimation.restart()
        // Animations advance with rendered frames; a timer guarantees the
        // ghost still lands and its follow-up runs if frames stall.
        ghostSettleGuard.interval = duration + 150
        ghostSettleGuard.restart()
    }

    function finishGhostAnimation() {
        ghostSettleGuard.stop()
        const finished = ghostFinished
        ghostFinished = null
        if (finished)
            finished()
    }

    // The window is now on screen underneath: let the ghost dissolve into it.
    function releaseGhost(delay) {
        if (!ghostVisible)
            return
        ghostFadeDelay.duration = Math.max(0, delay || 0)
        ghostFade.restart()
        ghostHideGuard.interval = ghostFadeDelay.duration + 150 + 200
        ghostHideGuard.restart()
    }

    function hideGhost() {
        ghostSettleGuard.stop()
        ghostHideGuard.stop()
        ghostAnimation.stop()
        ghostFade.stop()
        ghostVisible = false
        ghostWindowId = ""
        ghostThumbnail = ""
        ghostDetail = ""
        ghostFinished = null
        if (previewService)
            previewService.releaseDetailImage()
    }

    function hidePreview() {
        if (previewService)
            previewService.setPopupWindowIds([])
        previewVisible = false
        pointerHovered = false
        taskKey = ""
        entries = []
        if (dockPanel)
            dockPanel.setPreviewHovered(false)
    }

    function refreshImages() {
        if (!previewVisible || !previewService)
            return
        for (var i = 0; i < entries.length; ++i)
            previewService.requestWindowPreview(entries[i].windowId, true)
    }

    Timer {
        interval: 2200
        repeat: true
        running: root.previewVisible
        onTriggered: root.refreshImages()
    }

    HoverHandler {
        objectName: "previewSurfaceHover"
        onHoveredChanged: {
            root.pointerHovered = hovered
            if (root.dockPanel)
                root.dockPanel.setPreviewHovered(hovered)
        }
    }

    BackdropEffectRegions {
        objectName: "previewSurfaceBackdropRegions"
        anchors.fill: parent
        enabled: material.isFrosted && root.previewVisible

        BackdropRegion {
            item: panelBackground
            enabled: panelBackground.visible
            radius: panelBackground.radius
        }
    }

    // Same material as the Dock chrome and Alt+Tab: shell background with the
    // shell border, frosted by the compositor when the frosted style is active.
    Rectangle {
        id: panelBackground
        objectName: "previewSurfaceCardBackground"
        x: root.clampedX
        y: root.clampedY
        width: root.surfaceWidth
        height: root.surfaceHeight
        radius: root.panelRadius
        color: material.background
        // macOS-style double edge: a dark outer hairline separates the glass
        // from busy content, a light inner one catches the light.
        border.color: Qt.rgba(0, 0, 0, material.isLight ? 0.12 : 0.45)
        border.width: 1
        visible: root.previewVisible
        transformOrigin: root.panelTransformOrigin
        scale: root.previewVisible ? 1 : 0.94
        opacity: root.previewVisible ? 1 : 0
        Behavior on scale {
            NumberAnimation { duration: material.tokens.value("animationSubtle", 130) + 60; easing.type: Easing.OutBack; easing.overshoot: 0.9 }
        }
        Behavior on opacity {
            NumberAnimation { duration: material.tokens.value("animationInstant", 90) }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            color: material.surface
            border.width: 1
            border.color: material.border
        }
    }

    Flickable {
        id: previewScroll
        x: root.clampedX
        y: root.clampedY
        width: root.surfaceWidth
        height: root.surfaceHeight
        contentWidth: width
        contentHeight: root.naturalHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height
        transformOrigin: root.panelTransformOrigin
        scale: panelBackground.scale
        opacity: panelBackground.opacity

        Grid {
            x: root.panelPadding
            y: root.panelPadding
            columns: root.columns
            rowSpacing: root.cardSpacing
            columnSpacing: root.cardSpacing

            Repeater {
                model: root.entries
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    objectName: "window-preview-card-" + modelData.windowId
                    width: root.cardWidth
                    height: root.cardHeight
                    readonly property bool hovered: cardHover.hovered
                    // Cards cascade in after the panel, left to right.
                    opacity: 0
                    transform: Translate { id: cardLift; y: 6 }
                    ParallelAnimation {
                        id: cardEntrance
                        SequentialAnimation {
                            PauseAnimation { duration: 40 + card.index * 35 }
                            ParallelAnimation {
                                NumberAnimation { target: card; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                NumberAnimation { target: cardLift; property: "y"; to: 0; duration: 240; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                    property size imageSize: Qt.size(0, 0)
                    readonly property real fitScale: imageSize.width > 0 && imageSize.height > 0
                        ? Math.min(root.thumbnailWidth / imageSize.width,
                                   root.thumbnailHeight / imageSize.height, 1) : 0
                    readonly property bool imageReady: fitScale > 0 && previewImage.ready

                    function refreshImage() {
                        if (!root.previewService)
                            return
                        imageSize = root.previewService.imageSize(modelData.windowId)
                        previewImage.source = root.previewService.imageUrl(modelData.windowId)
                    }

                    // Alt+Tab's selection treatment: accent tint and accent edge.
                    Rectangle {
                        objectName: "window-preview-highlight-" + card.modelData.windowId
                        anchors.fill: parent
                        radius: root.panelRadius - root.panelPadding
                        color: card.hovered
                            ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.25)
                            : "transparent"
                        border.width: 1
                        border.color: card.hovered
                            ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.40)
                            : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: material.tokens.value("animationInstant", 90) }
                        }
                    }

                    Item {
                        id: thumbnailArea
                        x: root.cardPadding
                        y: root.cardPadding
                        width: root.thumbnailWidth
                        height: root.thumbnailHeight

                        // Rounded, bordered thumbnail drawn like Astrea's
                        // screenshot visual: soft shadow, mask, hairline edge.
                        // Crisp at any scale: the AstreaAppIcon path decodes at the
                        // presented size and rounds in its shader; the shadow
                        // is analytic, so the hover lift never resamples.
                        Item {
                            id: previewFrame
                            anchors.centerIn: parent
                            width: Math.max(1, Math.round(card.imageSize.width * card.fitScale))
                            height: Math.max(1, Math.round(card.imageSize.height * card.fitScale))
                            visible: card.imageReady
                            scale: card.hovered ? 1.035 : 1
                            Behavior on scale {
                                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                            }

                            RectangularShadow {
                                anchors.fill: parent
                                radius: root.thumbnailRadius
                                blur: card.hovered ? 16 : 10
                                offset.y: card.hovered ? 5 : 3
                                color: Qt.rgba(0, 0, 0, card.hovered ? 0.5 : 0.35)
                                Behavior on blur { NumberAnimation { duration: 160 } }
                            }

                            Shared.AstreaAppIcon {
                                id: previewImage
                                objectName: "window-preview-image-" + card.modelData.windowId
                                property string source: ""
                                anchors.fill: parent
                                iconUrl: source
                                iconSize: Math.max(width, height)
                                maximumPresentationScale: 1.05
                                iconRadius: material.radiusMedium
                                showFallbackText: false
                                appearanceOverride: "default"
                                // Captures are already in memory; a synchronous swap
                                // replaces the image in place instead of flashing the
                                // app icon while each periodic refresh decodes.
                                asynchronous: false
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: material.radiusMedium
                                color: "transparent"
                                border.width: 1
                                border.color: Qt.rgba(1, 1, 1, material.isLight ? 0.35 : 0.16)
                                antialiasing: true
                            }
                        }

                        Shared.AstreaAppIcon {
                            anchors.centerIn: parent
                            width: 48
                            height: 48
                            iconName: card.modelData.iconName || ""
                            iconPath: card.modelData.iconPath || ""
                            appName: card.modelData.displayName || ""
                            iconRadius: 10
                            fallbackRadius: 10
                            visible: !card.imageReady
                        }
                    }

                    Row {
                        x: root.cardPadding
                        y: thumbnailArea.y + thumbnailArea.height + root.cardPadding
                        width: root.thumbnailWidth
                        height: root.titleHeight
                        spacing: 6

                        Shared.AstreaAppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            iconName: card.modelData.iconName || ""
                            iconPath: card.modelData.iconPath || ""
                            appName: card.modelData.displayName || ""
                            iconRadius: 4
                            fallbackRadius: 4
                        }

                        Text {
                            objectName: "window-preview-title-" + card.modelData.windowId
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 22
                            text: card.modelData.title || card.modelData.displayName
                            textFormat: Text.PlainText
                            color: material.textActive
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            font: material.typography.font("label")
                            lineHeight: material.typography.role("label").lineHeight
                            lineHeightMode: Text.ProportionalHeight
                            renderType: Text.NativeRendering
                        }
                    }

                    Rectangle {
                        id: closeButton
                        objectName: "window-preview-close-" + card.modelData.windowId
                        x: thumbnailArea.x + thumbnailArea.width - width / 2 - 2
                        y: thumbnailArea.y - height / 2 + 2
                        width: 20
                        height: 20
                        radius: 10
                        color: closeHover.hovered ? material.hover : material.background
                        border.color: material.border
                        border.width: 1
                        visible: card.hovered
                        z: 2

                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: material.textActive
                            font.family: material.fontFamily
                            font.pixelSize: 14
                        }
                        HoverHandler { id: closeHover }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: DockController.closeWindow(card.modelData.taskKey,
                                                                 card.modelData.windowId)
                        }
                    }

                    HoverHandler { id: cardHover }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: function(eventPoint) {
                            const closePoint = closeButton.mapFromItem(card,
                                eventPoint.position.x, eventPoint.position.y)
                            if (closeButton.visible && closeButton.contains(closePoint))
                                return
                            if (DockController.activateWindow(card.modelData.taskKey,
                                                              card.modelData.windowId))
                                root.hidePreview()
                        }
                    }

                    Connections {
                        target: root.previewService
                        function onImageChanged(windowId) {
                            if (windowId === card.modelData.windowId)
                                card.refreshImage()
                        }
                        function onGenerationChanged() {
                            card.refreshImage()
                            root.previewService.requestWindowPreview(card.modelData.windowId,
                                                                     false)
                        }
                    }

                    Component.onCompleted: {
                        if (root.cardsSettled) {
                            card.opacity = 1
                            cardLift.y = 0
                        } else {
                            cardEntrance.start()
                        }
                        refreshImage()
                        if (root.previewService)
                            root.previewService.requestWindowPreview(modelData.windowId, false)
                    }
                }
            }
        }
    }

    ParallelAnimation {
        id: ghostAnimation
        property int duration: 200
        NumberAnimation { id: ghostXAnimation; target: root; property: "ghostX"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        NumberAnimation { id: ghostYAnimation; target: root; property: "ghostY"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        NumberAnimation { id: ghostWidthAnimation; target: root; property: "ghostWidth"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        NumberAnimation { id: ghostHeightAnimation; target: root; property: "ghostHeight"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        NumberAnimation { id: ghostRadiusAnimation; target: root; property: "ghostRadius"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        NumberAnimation { id: ghostLiftAnimation; target: root; property: "ghostLift"; duration: ghostAnimation.duration; easing.type: Easing.OutCubic }
        onFinished: root.finishGhostAnimation()
    }

    Timer {
        id: ghostSettleGuard
        onTriggered: {
            if (!root.ghostFinished)
                return
            ghostAnimation.stop()
            root.ghostX = ghostXAnimation.to
            root.ghostY = ghostYAnimation.to
            root.ghostWidth = ghostWidthAnimation.to
            root.ghostHeight = ghostHeightAnimation.to
            root.ghostRadius = ghostRadiusAnimation.to
            root.ghostLift = ghostLiftAnimation.to
            root.finishGhostAnimation()
        }
    }

    Timer {
        id: ghostHideGuard
        onTriggered: root.hideGhost()
    }

    SequentialAnimation {
        id: ghostFade
        PauseAnimation { id: ghostFadeDelay; duration: 0 }
        NumberAnimation { target: ghostItem; property: "opacity"; to: 0; duration: 150; easing.type: Easing.OutQuad }
        // Deferred: hideGhost stops this very animation.
        ScriptAction { script: Qt.callLater(root.hideGhost) }
    }

    Connections {
        target: root.previewService
        function onDetailImageChanged(windowId) {
            if (root.ghostVisible && windowId === root.ghostWindowId)
                root.ghostDetail = root.previewService.detailImageUrl(windowId)
        }
    }

    Item {
        id: ghostItem
        objectName: "dockWindowGhost"
        x: root.ghostX
        y: root.ghostY
        width: root.ghostWidth
        height: root.ghostHeight
        visible: root.ghostVisible

        RectangularShadow {
            anchors.fill: parent
            visible: ghostThumbnailImage.status === Image.Ready
                || ghostDetailImage.status === Image.Ready
            radius: root.ghostRadius
            blur: 6 + 30 * root.ghostLift
            offset.y: 2 + 14 * root.ghostLift
            color: Qt.rgba(0, 0, 0, 0.32 + 0.14 * root.ghostLift)
        }

        // Full-resolution decodes, sampled with mipmaps while the ghost grows;
        // the rounded-corner shader draws them, so the images stay hidden.
        Image {
            id: ghostThumbnailImage
            anchors.fill: parent
            source: root.ghostThumbnail
            asynchronous: false
            cache: false
            smooth: true
            mipmap: true
            visible: false
        }

        Image {
            id: ghostDetailImage
            anchors.fill: parent
            source: root.ghostDetail
            asynchronous: false
            cache: false
            smooth: true
            mipmap: true
            visible: false
        }

        ShaderEffect {
            anchors.fill: parent
            visible: ghostThumbnailImage.status === Image.Ready
            property variant source: ghostThumbnailImage
            property real roundedRadius: root.ghostRadius / Math.max(1, Math.min(width, height))
            property real presentationMode: 0
            property color tintColor: "#0a84ff"
            supportsAtlasTextures: false
            blending: true
            fragmentShader: "qrc:/shaders/rounded_icon.frag.qsb"
        }

        // The sharp capture fades in over the thumbnail as soon as it lands.
        ShaderEffect {
            anchors.fill: parent
            visible: ghostDetailImage.status === Image.Ready
            opacity: visible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
            property variant source: ghostDetailImage
            property real roundedRadius: root.ghostRadius / Math.max(1, Math.min(width, height))
            property real presentationMode: 0
            property color tintColor: "#0a84ff"
            supportsAtlasTextures: false
            blending: true
            fragmentShader: "qrc:/shaders/rounded_icon.frag.qsb"
        }
    }
}
