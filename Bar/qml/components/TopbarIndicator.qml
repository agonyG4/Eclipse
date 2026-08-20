import QtQuick

IndicatorButton {
    id: root

    property var popupController: null
    property int popupKind: 0
    property Item anchorItem: null
    property int anchorOffset: 0

    signal activated(real anchorX)

    function resolvedAnchorX() {
        if (root.anchorItem)
            return Math.round(root.anchorOffset
                              + root.mapToItem(root.anchorItem,
                                               root.width / 2, root.height / 2).x)
        return Math.round(root.anchorOffset
                          + root.mapToItem(null, root.width / 2, root.height / 2).x)
    }

    function togglePopup(anchorX) {
        if (!root.popupController)
            return
        const x = root.resolvedAnchorX()
        switch (root.popupKind) {
        case 1: root.popupController.toggleAstreaMenu(x); break
        case 2: root.popupController.toggleClock(x); break
        case 3: root.popupController.toggleNetwork(x); break
        case 4: root.popupController.toggleBluetooth(x); break
        case 5: root.popupController.toggleVolume(x); break
        default: break
        }
    }

    onClicked: anchorX => {
        root.togglePopup(anchorX)
        root.activated(root.resolvedAnchorX())
    }
}
