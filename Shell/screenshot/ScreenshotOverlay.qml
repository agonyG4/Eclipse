import QtQuick
import QtQuick.Window

Window {
    id: window

    objectName: "screenshotSelectionOverlay"
    title: "Astrea Screenshot"
    visible: ScreenshotController.visible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY

    property real startX: 0
    property real startY: 0
    property real currentX: 0
    property real currentY: 0
    property bool selecting: false
    readonly property real selectionLeft: Math.min(startX, currentX)
    readonly property real selectionTop: Math.min(startY, currentY)
    readonly property real selectionWidth: Math.abs(currentX - startX)
    readonly property real selectionHeight: Math.abs(currentY - startY)
    readonly property real selectionRight: selectionLeft + selectionWidth
    readonly property real selectionBottom: selectionTop + selectionHeight
    readonly property bool hasSelection: selectionWidth >= 1 || selectionHeight >= 1

    Component.onCompleted: ScreenshotController.setOverlaySize(width, height)
    onWidthChanged: ScreenshotController.setOverlaySize(width, height)
    onHeightChanged: ScreenshotController.setOverlaySize(width, height)
    onActiveChanged: {
        if (active && visible)
            keyboardScope.forceActiveFocus()
    }
    onVisibleChanged: {
        if (visible) {
            startX = 0
            startY = 0
            currentX = 0
            currentY = 0
            selecting = false
            keyboardScope.forceActiveFocus()
        }
    }

    FocusScope {
        id: keyboardScope
        objectName: "keyboardScope"
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: ScreenshotController.cancel()
    }

    Image {
        id: selectionImage
        objectName: "selectionImage"
        width: window.width
        height: window.height
        visible: ScreenshotController.selectionImageVisible
        source: ScreenshotController.imageSource
        fillMode: Image.Stretch
        smooth: false
        asynchronous: false
        cache: false
    }

    Rectangle {
        objectName: "fullDim"
        width: window.width
        height: window.height
        visible: !window.hasSelection
        color: "#66000000"
    }

    Rectangle {
        objectName: "topDim"
        x: 0
        y: 0
        width: window.width
        height: window.selectionTop
        visible: window.hasSelection
        color: "#66000000"
    }
    Rectangle {
        objectName: "bottomDim"
        x: 0
        y: window.selectionBottom
        width: window.width
        height: window.height - y
        visible: window.hasSelection
        color: "#66000000"
    }
    Rectangle {
        objectName: "leftDim"
        x: 0
        y: window.selectionTop
        width: window.selectionLeft
        height: window.selectionHeight
        visible: window.hasSelection
        color: "#66000000"
    }
    Rectangle {
        objectName: "rightDim"
        x: window.selectionRight
        y: window.selectionTop
        width: window.width - x
        height: window.selectionHeight
        visible: window.hasSelection
        color: "#66000000"
    }

    Rectangle {
        objectName: "selectionBorder"
        x: window.selectionLeft
        y: window.selectionTop
        width: window.selectionWidth
        height: window.selectionHeight
        visible: window.hasSelection
        color: "transparent"
        readonly property color selectionBorderColor: "#f5f5f5"
        readonly property int selectionBorderWidth: 1
        border.color: selectionBorderColor
        border.width: selectionBorderWidth
    }

    MouseArea {
        objectName: "selectionMouseArea"
        width: window.width
        height: window.height
        hoverEnabled: true
        cursorShape: Qt.CrossCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                ScreenshotController.cancel()
                return
            }
            window.startX = mouse.x
            window.startY = mouse.y
            window.currentX = mouse.x
            window.currentY = mouse.y
            window.selecting = true
        }
        onPositionChanged: function(mouse) {
            if (!window.selecting)
                return
            window.currentX = mouse.x
            window.currentY = mouse.y
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton || !window.selecting)
                return
            window.currentX = mouse.x
            window.currentY = mouse.y
            window.selecting = false
            ScreenshotController.selection = Qt.rect(window.selectionLeft, window.selectionTop,
                                                      window.selectionWidth, window.selectionHeight)
            ScreenshotController.finishSelection()
        }
    }
}
