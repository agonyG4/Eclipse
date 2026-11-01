import QtQuick
import QtQuick.Window
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
    property int cardWidth: 196
    property int cardHeight: 156
    property int cardSpacing: 8
    readonly property int columns: Math.max(1, Math.floor(
        (Math.max(1, outputWidth - 2 * edgeInset - 20) + cardSpacing)
        / (cardWidth + cardSpacing)))
    readonly property int rowCount: Math.ceil(entries.length / columns)
    readonly property int naturalHeight: rowCount * (cardHeight + cardSpacing)
        - cardSpacing + 20
    readonly property int surfaceWidth: Math.min(Math.max(1, outputWidth - 2 * edgeInset),
        Math.min(entries.length, columns) * (cardWidth + cardSpacing) - cardSpacing + 20)
    readonly property int surfaceHeight: Math.min(Math.max(1, outputHeight - 2 * edgeInset),
                                                  naturalHeight)
    readonly property int preferredX: dockPosition === "left"
        ? anchorRect.x + anchorRect.width + 10
        : dockPosition === "right" ? anchorRect.x - surfaceWidth - 10
                                    : anchorRect.x + anchorRect.width / 2 - surfaceWidth / 2
    readonly property int clampedX: Math.max(edgeInset,
        Math.min(outputWidth - edgeInset - surfaceWidth,
        preferredX))
    readonly property int preferredY: dockPosition === "bottom"
        ? anchorRect.y - surfaceHeight - 10
        : anchorRect.y + anchorRect.height / 2 - surfaceHeight / 2
    readonly property int clampedY: Math.max(edgeInset,
        Math.min(outputHeight - edgeInset - surfaceHeight, preferredY))

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

    Rectangle {
        objectName: "previewSurfaceCardBackground"
        x: root.clampedX
        y: root.clampedY
        width: root.surfaceWidth
        height: root.surfaceHeight
        radius: 12
        color: "#ed1b1d25"
        border.color: "#704f5668"
        clip: true
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

        Grid {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
            columns: root.columns
            rowSpacing: root.cardSpacing
            columnSpacing: root.cardSpacing

            Repeater {
                model: root.entries
                delegate: Rectangle {
                id: card
                required property var modelData
                objectName: "window-preview-card-" + modelData.windowId
                width: root.cardWidth
                height: root.cardHeight
                radius: 9
                color: "#ff252833"
                border.color: "#665f6474"
                clip: true

                Shared.AstreaAppIcon {
                    anchors.centerIn: previewImage
                    width: 36
                    height: 36
                    iconName: card.modelData.iconName || ""
                    iconPath: card.modelData.iconPath || ""
                    appName: card.modelData.displayName || ""
                    visible: !previewImage.visible
                }

                Image {
                    id: previewImage
                    objectName: "window-preview-image-" + card.modelData.windowId
                    x: 6
                    y: 6
                    width: parent.width - 12
                    height: parent.height - 38
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    source: root.previewService
                        ? root.previewService.imageUrl(card.modelData.windowId) : ""
                    visible: status === Image.Ready
                    Component.onCompleted: {
                        if (root.previewService)
                            root.previewService.requestWindowPreview(card.modelData.windowId, false)
                    }
                    Connections {
                        target: root.previewService
                        function onImageChanged(windowId) {
                            if (windowId === card.modelData.windowId)
                                previewImage.source = root.previewService.imageUrl(windowId)
                        }
                        function onGenerationChanged() {
                            previewImage.source = root.previewService.imageUrl(
                                card.modelData.windowId)
                            root.previewService.requestWindowPreview(
                                card.modelData.windowId, false)
                        }
                    }
                }

                Text {
                    objectName: "window-preview-title-" + card.modelData.windowId
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    height: 26
                    text: card.modelData.title || card.modelData.displayName
                    color: "#fff"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    maximumLineCount: 1
                    font.pixelSize: 12
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 7
                    width: 20
                    height: 20
                    radius: 10
                    color: "#c91b1d25"
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: "white"
                        font.pixelSize: 15
                    }
                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: DockController.closeWindow(card.modelData.taskKey,
                                                             card.modelData.windowId)
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: function(eventPoint) {
                        if (eventPoint.position.x >= card.width - 27
                            && eventPoint.position.y <= 27)
                            return
                        if (DockController.activateWindow(card.modelData.taskKey,
                                                          card.modelData.windowId))
                            root.hidePreview()
                    }
                }
                }
            }
        }
    }
}
