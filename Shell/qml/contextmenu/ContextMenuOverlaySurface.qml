import QtQuick
import QtQuick.Window

Window {
    id: root

    property var contextMenuController: null
    property string outputKey: ""
    property int outputWidth: width
    property int outputHeight: height
    property int outputOriginX: 0
    property int outputOriginY: 0
    property int menuWidth: 280
    property var trayService: null

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint

    function syncMenuGeometry() {
        if (!menuView || !root.contextMenuController)
            return
        menuView.width = Math.min(root.menuWidth, Math.max(1, root.outputWidth))
        menuView.height = Math.min(menuView.implicitHeight, Math.max(1, root.outputHeight))
        const position = root.contextMenuController.menuPosition(root.outputWidth,
                                                                  root.outputHeight,
                                                                  menuView.width,
                                                                  menuView.height)
        menuView.x = position.x
        menuView.y = position.y
    }

    function resetPresentationVisuals() {
        menuView.opacity = 0
        menuView.scale = 0.96
        trayMenuLoader.opacity = 0
        trayMenuLoader.scale = 0.96
    }

    function focusActiveMenu() {
        if (!root.visible)
            return
        if (trayMenuLoader.item)
            trayMenuLoader.item.forceActiveFocus()
        else if (menuView.visible)
            menuView.forceActiveFocus()
    }

    function syncAnimationForLifecycle() {
        const lifecycle = root.contextMenuController
            ? root.contextMenuController.lifecycle : 0
        if (lifecycle === 1) {
            exitAnimation.stop()
            menuView.enabled = true
            trayMenuLoader.enabled = true
            outsideShield.enabled = true
            enterAnimation.restart()
        } else if (lifecycle === 2) {
            enterAnimation.stop()
            menuView.enabled = false
            trayMenuLoader.enabled = false
            outsideShield.enabled = false
            exitAnimation.restart()
        } else {
            enterAnimation.stop()
            exitAnimation.stop()
            menuView.enabled = false
            trayMenuLoader.enabled = false
            outsideShield.enabled = false
            resetPresentationVisuals()
        }
    }

    MouseArea {
        id: outsideShield
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: root.contextMenuController
            && root.contextMenuController.lifecycle === 1
        z: 0
        onPressed: function(mouse) {
            if (root.contextMenuController)
                root.contextMenuController.close()
            mouse.accepted = true
        }
    }

    ContextMenuView {
        id: menuView
        objectName: "contextMenuView"
        parent: root.contentItem
        z: 1
        visible: !root.contextMenuController
            || root.contextMenuController.targetKind !== 2
        width: Math.min(root.menuWidth, Math.max(1, root.outputWidth))
        height: Math.min(implicitHeight, Math.max(1, root.outputHeight))
        menuModel: root.contextMenuController ? root.contextMenuController.model : null
        contextMenuController: root.contextMenuController
        presentationGeneration: root.contextMenuController
            ? root.contextMenuController.presentationGeneration : 0
        outputWidth: root.outputWidth
        outputHeight: root.outputHeight
        transformOrigin: Item.TopLeft
        opacity: 0
        scale: 0.96
        enabled: root.contextMenuController
            && root.contextMenuController.lifecycle === 1
        onImplicitHeightChanged: root.syncMenuGeometry()
        Component.onCompleted: root.syncMenuGeometry()
    }

    Loader {
        id: trayMenuLoader
        objectName: "trayMenuLoader"
        parent: root.contentItem
        z: 1
        active: root.contextMenuController
            && root.contextMenuController.targetKind === 2
        source: active ? "qrc:/qt/qml/Astrea/Shell/Bar/qml/TrayContextMenu.qml" : ""
        transformOrigin: Item.TopLeft
        opacity: 0
        scale: 0.96
        enabled: root.contextMenuController
            && root.contextMenuController.lifecycle === 1
        width: item ? Math.min(item.implicitWidth, Math.max(1, root.outputWidth)) : 0
        height: item ? Math.min(item.implicitHeight, Math.max(1, root.outputHeight)) : 0
        x: root.contextMenuController && item
           ? root.contextMenuController.menuPosition(root.outputWidth, root.outputHeight,
                                                      width, height).x : 0
        y: root.contextMenuController && item
           ? root.contextMenuController.menuPosition(root.outputWidth, root.outputHeight,
                                                      width, height).y : 0
        onLoaded: {
            item.trayService = root.trayService
            item.contextMenuController = root.contextMenuController
            item.contextMenuGeneration = root.contextMenuController.presentationGeneration
            item.contextKey = root.contextMenuController.targetIdentity
            item.outputWidth = root.outputWidth
            item.outputHeight = root.outputHeight
            Qt.callLater(root.focusActiveMenu)
        }
    }

    ParallelAnimation {
        id: enterAnimation
        NumberAnimation {
            targets: [menuView, trayMenuLoader]
            property: "opacity"
            from: 0
            to: 1
            duration: 120
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            targets: [menuView, trayMenuLoader]
            property: "scale"
            from: 0.96
            to: 1
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    ParallelAnimation {
        id: exitAnimation
        NumberAnimation {
            targets: [menuView, trayMenuLoader]
            property: "opacity"
            from: 1
            to: 0
            duration: 100
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            targets: [menuView, trayMenuLoader]
            property: "scale"
            from: 1
            to: 0.96
            duration: 100
            easing.type: Easing.InCubic
        }
        onFinished: {
            if (root.contextMenuController
                && root.contextMenuController.lifecycle === 2)
                root.contextMenuController.completeClose()
        }
    }

    Connections {
        target: root.contextMenuController
        function onPresentationChanged() {
            root.syncMenuGeometry()
            root.focusActiveMenu()
        }
        function onLifecycleChanged() {
            root.syncAnimationForLifecycle()
        }
    }

    Component.onCompleted: {
        resetPresentationVisuals()
        syncAnimationForLifecycle()
    }

    onVisibleChanged: if (visible) Qt.callLater(focusActiveMenu)
    onActiveChanged: if (active) Qt.callLater(focusActiveMenu)

}
