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

    MouseArea {
        id: outsideShield
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 0
        onPressed: function(mouse) {
            if (root.contextMenuController)
                root.contextMenuController.close()
            mouse.accepted = true
        }
    }

    ContextMenuView {
        id: menuView
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
        onImplicitHeightChanged: root.syncMenuGeometry()
        Component.onCompleted: {
            forceActiveFocus()
            root.syncMenuGeometry()
        }
    }

    Loader {
        id: trayMenuLoader
        parent: root.contentItem
        z: 1
        active: root.contextMenuController
            && root.contextMenuController.targetKind === 2
        source: active ? "qrc:/qt/qml/Astrea/Shell/Bar/qml/TrayContextMenu.qml" : ""
        width: item ? Math.min(item.implicitWidth, Math.max(1, root.outputWidth))
                    : root.menuWidth
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
            item.width = Math.min(root.menuWidth, Math.max(1, root.outputWidth))
            item.forceActiveFocus()
        }
    }

    Timer {
        id: finishCloseTimer
        interval: 140
        repeat: false
        onTriggered: {
            if (root.contextMenuController
                && root.contextMenuController.lifecycle === 2)
                root.contextMenuController.completeClose()
        }
    }

    Connections {
        target: root.contextMenuController
        function onPresentationChanged() {
            root.syncMenuGeometry()
            if (root.visible) {
                if (root.contextMenuController
                    && root.contextMenuController.targetKind === 2
                    && trayMenuLoader.item)
                    trayMenuLoader.item.forceActiveFocus()
                else
                    menuView.forceActiveFocus()
            }
        }
        function onLifecycleChanged() {
            if (root.contextMenuController
                && root.contextMenuController.lifecycle === 2)
                finishCloseTimer.restart()
            else
                finishCloseTimer.stop()
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.contextMenuController) {
            root.contextMenuController.close()
            event.accepted = true
        }
    }
}
