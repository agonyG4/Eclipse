import QtQuick
import QtQuick.Window

Window {
    id: root

    objectName: "screenshotPreviewSurface"
    title: "Astrea Screenshot Preview"
    visible: ScreenshotController.previewVisible
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: Screen.width > 0 ? Screen.width : 1920
    height: Screen.height > 0 ? Screen.height : 1080
    x: Screen.virtualX
    y: Screen.virtualY

    property real transitionProgress: 0.0
    property rect transitionSourceRect: Qt.rect(0, 0, 0, 0)
    property real backdropOpacity: 0.34
    property bool closing: false
    property rect targetRect: {
        const imageWidth = ScreenshotController.imageWidth
        const imageHeight = ScreenshotController.imageHeight
        const maxWidth = width * 0.78
        const maxHeight = height * 0.78
        if (imageWidth <= 0 || imageHeight <= 0)
            return Qt.rect((width - maxWidth) / 2, (height - maxHeight) / 2,
                           maxWidth, maxHeight)
        const aspectRatio = imageWidth / imageHeight
        let targetWidth = maxWidth
        let targetHeight = targetWidth / aspectRatio
        if (targetHeight > maxHeight) {
            targetHeight = maxHeight
            targetWidth = targetHeight * aspectRatio
        }
        return Qt.rect((width - targetWidth) / 2, (height - targetHeight) / 2,
                       targetWidth, targetHeight)
    }
    property rect cardRect: Qt.rect(
        transitionSourceRect.x
            + (targetRect.x - transitionSourceRect.x) * transitionProgress,
        transitionSourceRect.y
            + (targetRect.y - transitionSourceRect.y) * transitionProgress,
        transitionSourceRect.width
            + (targetRect.width - transitionSourceRect.width) * transitionProgress,
        transitionSourceRect.height
            + (targetRect.height - transitionSourceRect.height) * transitionProgress)

    function requestClose() {
        if (!root.visible || root.closing)
            return
        openAnimation.stop()
        root.closing = true
        closeAnimation.from = root.transitionProgress
        closeAnimation.restart()
    }

    onVisibleChanged: {
        if (visible) {
            closing = false
            transitionSourceRect = ScreenshotController.thumbnailRect
            transitionProgress = 0.0
            keyboardScope.forceActiveFocus()
            Qt.callLater(function() {
                if (root.visible && !root.closing)
                    openAnimation.restart()
            })
        } else {
            openAnimation.stop()
            closeAnimation.stop()
            closing = false
            transitionProgress = 0.0
        }
    }
    onActiveChanged: {
        if (active && visible)
            keyboardScope.forceActiveFocus()
    }

    FocusScope {
        id: keyboardScope
        objectName: "keyboardScope"
        width: root.width
        height: root.height
        focus: true
        Keys.onEscapePressed: root.requestClose()
    }

    Rectangle {
        id: previewBackdrop
        objectName: "previewBackdrop"
        width: root.width
        height: root.height
        color: "#000000"
        opacity: root.transitionProgress * root.backdropOpacity

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: root.requestClose()
        }
    }

    Rectangle {
        id: previewShadow
        objectName: "previewShadow"
        x: root.cardRect.x
        y: root.cardRect.y + 8
        width: root.cardRect.width
        height: root.cardRect.height
        radius: 10 + root.transitionProgress * 3
        color: "#55000000"
        opacity: 0.55 + root.transitionProgress * 0.2
        z: 1
    }

    Rectangle {
        id: previewCard
        objectName: "previewCard"
        x: root.cardRect.x
        y: root.cardRect.y
        width: root.cardRect.width
        height: root.cardRect.height
        radius: 10 + root.transitionProgress * 3
        color: "transparent"
        border.color: "#b3ffffff"
        border.width: 1
        clip: true
        z: 2

        Image {
            id: previewImage
            objectName: "previewImage"
            anchors.fill: parent
            source: ScreenshotController.imageSource
            fillMode: Image.Stretch
            asynchronous: false
            cache: false
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: function(mouse) { mouse.accepted = true }
        }
    }

    NumberAnimation {
        id: openAnimation
        target: root
        property: "transitionProgress"
        from: 0.0
        to: 1.0
        duration: 250
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: closeAnimation
        target: root
        property: "transitionProgress"
        to: 0.0
        duration: 220
        easing.type: Easing.OutCubic
        onFinished: {
            if (root.closing) {
                root.closing = false
                ScreenshotController.closePreview()
            }
        }
    }
}
