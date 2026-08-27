import QtQuick
import QtQuick.Controls.Basic
import Astrea.Shared as Shared

Item {
    id: root

    required property int index
    required property var model
    required property string desktopFileName
    required property string displayName
    required property string iconName
    required property string iconPath
    required property string iconUrl
    required property bool resolved
    required property bool launching
    required property bool pinned
    required property bool runtimeKnown
    required property bool running
    required property bool active
    required property int windowCount
    required property int slotWidth
    required property int slotHeight
    property var dockPanel
    property var contextMenuController: null
    property var dockSurfaceGeometry: null
    property string outputKey: ""
    property bool pointerTarget: false
    property real magnificationScale: 1.0
    property real visualOffsetX: 0
    property real visualOffsetY: 0
    property bool dragging: false
    property bool dragWasActive: false

    property int iconSize: 48
    signal activated(string desktopFileName)
    signal dragStarted(string desktopFileName)
    signal dragMoved(string desktopFileName, real translationX)
    signal dragFinished(string desktopFileName)
    signal dragCanceled(string desktopFileName)

    objectName: desktopFileName
    width: slotWidth
    height: slotHeight

    SystemPalette { id: systemPalette }

    Shared.AstreaAppIcon {
        id: appIcon
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        width: root.iconSize
        height: root.iconSize
        scale: root.magnificationScale * (root.dragging ? 1.06 : 1)
        transformOrigin: Item.Bottom
        iconName: root.iconName
        iconPath: root.iconPath
        iconUrl: root.iconUrl
        appName: root.displayName
        // 2x source sampling remains sharp at the configured 2.0 maximum.
        sourcePixelSize: root.iconSize * 2
        iconRadius: 10
        fallbackRadius: 10
    }

    transform: Translate {
        x: root.visualOffsetX
        y: root.visualOffsetY
    }
    // The visual topmost icon should also win pointer targeting where scaled
    // icon bounds overlap neighboring resting slots.
    z: root.dragging ? 100 : root.magnificationScale
    Behavior on visualOffsetX {
        enabled: !root.dragging
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on visualOffsetY {
        enabled: !root.dragging
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }
    Behavior on magnificationScale {
        NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
    }

    TapHandler {
        id: tapHandler
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        margin: root.iconSize * (root.magnificationScale - 1) / 2
        onTapped: {
            if (!root.dragWasActive)
                root.activated(root.desktopFileName)
            root.dragWasActive = false
        }
    }

    TapHandler {
        id: contextTapHandler
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            if (!root.contextMenuController || !root.dockSurfaceGeometry || !root.dockPanel)
                return
            const topLeft = root.mapToItem(root.dockPanel, 0, 0)
            const outputRect = root.dockSurfaceGeometry.outputLocalDelegateRect(
                root.dockPanel.outputWidth, root.dockPanel.outputHeight,
                Math.round(root.dockPanel.width), Math.round(root.dockPanel.height),
                DockController.bottomMargin,
                Qt.rect(topLeft.x, topLeft.y, root.width, root.height))
            root.contextMenuController.presentDock(root.desktopFileName,
                outputRect.x, outputRect.y, outputRect.width, outputRect.height,
                root.outputKey)
        }
    }

    DragHandler {
        id: dragHandler
        enabled: root.pinned && (!root.dockPanel || root.dockPanel.draggedDesktopFileName === ""
                                  || root.dockPanel.draggedDesktopFileName === root.desktopFileName)
        target: null
        margin: root.iconSize * (root.magnificationScale - 1) / 2
        dragThreshold: 8
        onActiveChanged: {
            if (active) {
                root.dragWasActive = true
                root.dragStarted(root.desktopFileName)
            } else if (root.dragWasActive) {
                root.dragFinished(root.desktopFileName)
                // Keep the release event from being interpreted as a tap, then
                // allow the next physical click to activate normally.
                Qt.callLater(function() { root.dragWasActive = false })
            }
        }
        onTranslationChanged: if (active) root.dragMoved(root.desktopFileName, translation.x)
        onCanceled: {
            if (root.dragWasActive) {
                root.dragCanceled(root.desktopFileName)
                Qt.callLater(function() { root.dragWasActive = false })
            }
        }
    }

    ToolTip.visible: root.pointerTarget
    ToolTip.delay: 420
    ToolTip.text: root.runtimeKnown && root.running
        ? qsTr("%1 (%2 windows)").arg(root.displayName).arg(root.windowCount)
        : qsTr("%1").arg(root.displayName)

    Rectangle {
        anchors.fill: appIcon
        radius: appIcon.iconRadius
        color: "#18FFFFFF"
        visible: tapHandler.pressed || dragHandler.active
    }

    Rectangle {
        anchors.horizontalCenter: appIcon.horizontalCenter
        anchors.top: appIcon.bottom
        anchors.topMargin: 2
        width: root.active ? 18 : 8
        height: 3
        radius: 1.5
        color: systemPalette.highlight
        visible: root.runtimeKnown && root.running
    }

}
