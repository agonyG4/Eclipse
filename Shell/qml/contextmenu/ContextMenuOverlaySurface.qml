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
    property bool debugEnabled: contextMenuController
        ? contextMenuController.debugEnabled : false
    property bool debugScheduled: false
    property string pendingDebugStage: "settled"
    property string lastDebugSignature: ""

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint
    // This is a fullscreen output-local layer-shell surface. Keep the Qt
    // logical window/content geometry bound to the same output contract that
    // is sent to the compositor by ContextMenuSurfaceBundle.
    width: outputWidth
    height: outputHeight

    function debugGeometry(stage) {
        if (!root.debugEnabled || !root.contextMenuController
                || !root.contextMenuController.hasActivePresentation)
            return

        const view = root.contextMenuController.targetKind === 2 && trayMenuLoader.item
            ? trayMenuLoader.item : menuView
        const value = function(item, name, fallback) {
            if (!item)
                return fallback
            const result = item[name]
            return result === undefined ? fallback : result
        }
        const fields = {
            stage: stage,
            targetKind: root.contextMenuController.targetKind,
            targetIdentity: root.contextMenuController.targetIdentity,
            outputKey: root.contextMenuController.outputKey,
            generation: root.contextMenuController.presentationGeneration,
            anchorKind: root.contextMenuController.anchorKind,
            anchorPointX: root.contextMenuController.anchorPoint.x,
            anchorPointY: root.contextMenuController.anchorPoint.y,
            anchorX: root.contextMenuController.anchorRectangle.x,
            anchorY: root.contextMenuController.anchorRectangle.y,
            anchorWidth: root.contextMenuController.anchorRectangle.width,
            anchorHeight: root.contextMenuController.anchorRectangle.height,
            outputWidth: root.outputWidth,
            outputHeight: root.outputHeight,
            outputOriginX: root.outputOriginX,
            outputOriginY: root.outputOriginY,
            windowWidth: root.width,
            windowHeight: root.height,
            contentWidth: root.contentItem.width,
            contentHeight: root.contentItem.height,
            viewWidth: value(view, "width", 0),
            viewHeight: value(view, "height", 0),
            viewImplicitWidth: value(view, "implicitWidth", 0),
            viewImplicitHeight: value(view, "implicitHeight", 0),
            cardWidth: value(view, "cardWidth", 0),
            cardHeight: value(view, "cardHeight", 0),
            listContentWidth: value(view, "listContentWidth", 0),
            listContentHeight: value(view, "listContentHeight", 0),
            modelRows: value(view, "modelRowCount", 0),
            resolvedWidth: value(view, "width", 0),
            resolvedHeight: value(view, "height", 0),
            resolvedX: value(view, "x", 0),
            resolvedY: value(view, "y", 0),
            loaderWidth: trayMenuLoader.width,
            loaderHeight: trayMenuLoader.height,
            loaderItemWidth: value(trayMenuLoader.item, "width", 0),
            loaderItemHeight: value(trayMenuLoader.item, "height", 0),
            cascadeWidth: value(view, "cascadeWidth", 0),
            cascadeHeight: value(view, "cascadeHeight", 0),
            cascadeX: value(view, "cascadeX", 0),
            cascadeY: value(view, "cascadeY", 0),
            dpr: root.devicePixelRatio,
            effectiveDpr: root.screen ? root.screen.devicePixelRatio : root.devicePixelRatio
        }
        const signature = JSON.stringify(fields)
        if (stage !== "presentation" && signature === root.lastDebugSignature)
            return
        root.lastDebugSignature = signature
        console.log("astrea.context-menu " + signature)
    }

    function scheduleDebugGeometry(stage) {
        if (!root.debugEnabled)
            return
        root.pendingDebugStage = stage
        root.debugScheduled = true
        debugSettlementTimer.restart()
    }

    Timer {
        id: debugSettlementTimer
        // Give QQuickWindow one polish/render turn after output or model
        // changes; the diagnostic must describe settled content geometry,
        // not the transient value observed during binding propagation.
        interval: 16
        repeat: false
        onTriggered: {
            root.debugScheduled = false
            root.debugGeometry(root.pendingDebugStage)
        }
    }

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
        root.scheduleDebugGeometry("settled")
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
            trayMenuLoader.item.focusActiveMenu()
        else if (menuView.visible)
            menuView.forceActiveFocus()
    }

    function syncTrayPresentation() {
        if (!trayMenuLoader.item || !root.contextMenuController
                || root.contextMenuController.targetKind !== 2)
            return
        trayMenuLoader.item.trayService = root.trayService
        trayMenuLoader.item.contextMenuController = root.contextMenuController
        trayMenuLoader.item.contextKey = root.contextMenuController.targetIdentity
        trayMenuLoader.item.contextMenuGeneration =
            root.contextMenuController.presentationGeneration
        trayMenuLoader.item.outputWidth = root.outputWidth
        trayMenuLoader.item.outputHeight = root.outputHeight
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
            root.syncTrayPresentation()
            root.scheduleDebugGeometry("presentation")
            root.scheduleFocusActiveMenu()
        }
    }

    Timer {
        id: focusActiveMenuTimer
        interval: 0
        repeat: false
        onTriggered: root.focusActiveMenu()
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
            root.syncTrayPresentation()
            root.syncMenuGeometry()
            root.focusActiveMenu()
            root.debugGeometry("presentation")
        }
        function onLifecycleChanged() {
            root.syncAnimationForLifecycle()
            if (root.contextMenuController
                    && root.contextMenuController.lifecycle === 1)
                root.scheduleDebugGeometry("presentation")
        }
    }

    Connections {
        // QQuickWindow contentItem geometry can settle one event turn after
        // the Window width/height binding. Reschedule the gated diagnostic at
        // that boundary so its settled record describes the full Qt scene.
        target: root.contentItem
        function onWidthChanged() { root.scheduleDebugGeometry("settled") }
        function onHeightChanged() { root.scheduleDebugGeometry("settled") }
    }

    Component.onCompleted: {
        resetPresentationVisuals()
        syncAnimationForLifecycle()
    }

    function scheduleFocusActiveMenu() {
        focusActiveMenuTimer.restart()
    }

    onVisibleChanged: if (visible) scheduleFocusActiveMenu()
    onActiveChanged: if (active) scheduleFocusActiveMenu()
    onOutputWidthChanged: {
        root.syncMenuGeometry()
        root.scheduleDebugGeometry("settled")
    }
    onOutputHeightChanged: {
        root.syncMenuGeometry()
        root.scheduleDebugGeometry("settled")
    }

}
