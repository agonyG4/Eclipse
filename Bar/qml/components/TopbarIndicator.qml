import QtQuick

IndicatorButton {
    id: root

    property var popupController: null
    property int popupKind: 0
    property Item anchorItem: null
    property int anchorOffset: 0
    property string registeredOrderKey: ""
    readonly property bool popupActive: root.popupController
        ? root.popupController.surfaceRequired && root.popupController.kind === root.popupKind
        : false

    active: root.popupActive

    onWidthChanged: {
        if (root.statusLayout)
            root.statusLayout.rebuild()
    }

    signal activated(real anchorX)

    Component.onCompleted: {
        if (root.statusLayout && root.reorderable && root.orderKey) {
            const item = root.reorderItem || root
            root.registeredOrderKey = root.statusLayout.registerItem(item, root.orderKey)
            root.orderKey = root.registeredOrderKey
        }
    }
    Component.onDestruction: {
        if (root.statusLayout && root.reorderable && root.registeredOrderKey)
            root.statusLayout.unregisterItem(root.reorderItem || root, root.registeredOrderKey)
    }

    function resolvedAnchorX() {
        if (root.anchorItem)
            return Math.round(root.anchorOffset
                              + root.mapToItem(root.anchorItem,
                                               root.width / 2, root.height / 2).x)
        return Math.round(root.anchorOffset
                          + root.mapToItem(null, root.width / 2, root.height / 2).x)
    }

    function togglePopup(anchorX) {
        if (!root.popupController || root.popupKind === 0)
            return
        const x = root.resolvedAnchorX()
        switch (root.popupKind) {
        case 1: root.popupController.toggleAstreaMenu(x); break
        case 3: root.popupController.toggleNetwork(x); break
        case 4: root.popupController.toggleBluetooth(x); break
        case 5: root.popupController.toggleVolume(x); break
        default: break
        }
    }

    onClicked: anchorX => {
        if (root.interactive) {
            root.togglePopup(anchorX)
            root.activated(root.resolvedAnchorX())
        }
    }
}
