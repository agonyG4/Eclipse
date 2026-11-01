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

    Shared.ShellMaterialTheme {
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
    readonly property int titleHeight: 22
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

    visible: previewVisible
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
    onPreviewVisibleChanged: Qt.callLater(updateInputRegion)

    function showPreview(key, windowEntries, visualAnchor, position, widthIn, heightIn,
                         originX, originY) {
        taskKey = key
        entries = windowEntries || []
        anchorRect = visualAnchor
        dockPosition = position
        outputWidth = Math.max(1, widthIn)
        outputHeight = Math.max(1, heightIn)
        outputOriginX = originX || 0
        outputOriginY = originY || 0
        previewVisible = entries.length >= 2
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
                            font.family: material.fontFamily
                            font.pixelSize: material.fontSizeCaption + 1
                            font.weight: Font.Medium
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
                        cardEntrance.start()
                        refreshImage()
                        if (root.previewService)
                            root.previewService.requestWindowPreview(modelData.windowId, false)
                    }
                }
            }
        }
    }
}
