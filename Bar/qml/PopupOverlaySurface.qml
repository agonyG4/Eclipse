import QtQuick
import QtQuick.Window
import Astrea.Effects
import Astrea.System
import "components"

Window {
    id: window

    property var barController: null
    property var clockService: null
    property var audioService: null
    property var networkService: null
    property var bluetoothService: null
    property var statusNotifierService: null
    property var popupController: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int sidePadding: barGeometry ? barGeometry.popupSidePadding : 8
    property int topOffset: barGeometry ? barGeometry.popupTop : 54
    // Attached popups hang from the bottom edge of the bar pills.
    property int attachedTop: barGeometry ? barGeometry.topMargin + barGeometry.pillHeight : 41
    property string popupStyle: preferences.popupStyle
    readonly property bool attachedStyle: popupStyle === "attached"
    readonly property real hiddenScale: 0.94
    readonly property int floatingLift: 8
    readonly property int enterDuration: attachedStyle ? 340 : 280
    readonly property int exitDuration: attachedStyle ? 260 : 220
    // 0 = hidden, 1 = presented. Every visual of the active popup derives
    // from this one value so an interrupted enter/exit reverses in place.
    property real presentation: 0
    property bool entering: true
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: outputWidth
    height: outputHeight

    ShellBarTheme { id: theme }
    TopbarPreferences { id: preferences }

    function cardProgress(card) {
        return card === window.activePopup ? window.presentation : 0
    }

    function cardOpacity(card) {
        const p = cardProgress(card)
        if (window.attachedStyle)
            return Math.max(0, Math.min(1, p * 4))
        // Fade in quickly, fade out along the whole exit.
        return Math.max(0, Math.min(1, window.entering ? p * 1.8 : p))
    }

    function cardScale(card) {
        if (window.attachedStyle)
            return 1
        return window.hiddenScale + (1 - window.hiddenScale) * cardProgress(card)
    }

    function cardY(card) {
        const p = cardProgress(card)
        if (window.attachedStyle)
            return -(1 - Math.max(0, Math.min(1, p))) * card.height
        return window.topOffset - (1 - p) * window.floatingLift
    }

    // The compositor blur is all-or-nothing per region, so a fading card
    // would leave a full-strength frosted rectangle behind it until the very
    // last frame. The blur region instead tracks the card's visible extent and
    // shrinks with it.
    Item {
        id: blurProxy
        objectName: "popupBlurProxy"
        readonly property Item card: window.activePopup
        readonly property real progress: card
            ? Math.max(0, Math.min(1, window.attachedStyle ? window.presentation
                                                           : window.cardOpacity(card)))
            : 0
        readonly property real cardScaleValue: card ? card.scale : 1
        x: card ? card.x + card.width * (1 - cardScaleValue) / 2 : 0
        y: !card ? 0 : window.attachedStyle ? window.attachedTop : card.y
        width: card ? card.width * cardScaleValue : 0
        height: card ? card.height * cardScaleValue * progress : 0
        visible: card !== null && card.visible && height >= 1
        readonly property real radius: card
            ? Math.min(card.radius * cardScaleValue, width / 2, height / 2) : 0
    }

    Item {
        id: blurTopStrip
        objectName: "popupBlurTopStrip"
        x: blurProxy.x
        y: blurProxy.y
        width: blurProxy.width
        height: Math.min(blurProxy.height, blurProxy.radius)
        visible: window.attachedStyle && blurProxy.visible && height >= 1
    }

    BackdropEffectRegions {
        id: backdropRegions
        anchors.fill: parent
        enabled: theme.isFrosted

        BackdropRegion {
            item: blurProxy
            enabled: blurProxy.visible
            radius: blurProxy.radius
        }
        BackdropRegion {
            item: blurTopStrip
            enabled: blurTopStrip.visible
            radius: 0
        }
    }

    MouseArea {
        objectName: "popupClickShield"
        anchors.fill: parent
        enabled: popupController && popupController.surfaceRequired
            && window.popupForKind(popupController.kind) !== null
        z: 0
        onClicked: popupController.close()
    }

    Item {
        objectName: "astreaMenuFrame"
        x: 0
        width: window.width
        y: window.attachedStyle ? window.attachedTop : 0
        height: Math.max(0, window.height - y)
        clip: window.attachedStyle
        z: 2

        AstreaMenu {
            id: astreaMenu
            objectName: "astreaMenu"
            visible: popupController && popupController.surfaceRequired
                      && popupController.kind === 1
            width: window.barGeometry
                ? window.barGeometry.popupWidth(window.outputWidth, astreaMenu.implicitWidth,
                                                window.sidePadding)
                               : astreaMenu.implicitWidth
            x: window.popupController && window.barGeometry
                ? window.barGeometry.popupX(window.outputWidth, astreaMenu.width,
                                            window.popupController.anchorX, window.sidePadding)
                : window.sidePadding
            y: window.cardY(astreaMenu)
            attachedTop: window.attachedStyle
            transformOrigin: Item.Top
            barController: window.barController
            popupController: window.popupController
            opacity: window.cardOpacity(astreaMenu)
            scale: window.cardScale(astreaMenu)
        }
    }

    Item {
        objectName: "networkPopupFrame"
        x: 0
        width: window.width
        y: window.attachedStyle ? window.attachedTop : 0
        height: Math.max(0, window.height - y)
        clip: window.attachedStyle
        z: 2

        NetworkPopup {
            id: networkPopup
            objectName: "networkPopup"
            visible: popupController && popupController.surfaceRequired
                      && popupController.kind === 3
            width: window.barGeometry
                ? window.barGeometry.popupWidth(window.outputWidth, networkPopup.implicitWidth,
                                                window.sidePadding)
                               : networkPopup.implicitWidth
            x: window.popupController && window.barGeometry
                ? window.barGeometry.popupX(window.outputWidth, networkPopup.width,
                                            window.popupController.anchorX, window.sidePadding)
                : window.sidePadding
            y: window.cardY(networkPopup)
            attachedTop: window.attachedStyle
            transformOrigin: Item.Top
            networkService: window.networkService
            opacity: window.cardOpacity(networkPopup)
            scale: window.cardScale(networkPopup)
        }
    }

    Item {
        objectName: "bluetoothPopupFrame"
        x: 0
        width: window.width
        y: window.attachedStyle ? window.attachedTop : 0
        height: Math.max(0, window.height - y)
        clip: window.attachedStyle
        z: 2

        BluetoothPopup {
            id: bluetoothPopup
            objectName: "bluetoothPopup"
            visible: popupController && popupController.surfaceRequired
                      && popupController.kind === 4
            width: window.barGeometry
                ? window.barGeometry.popupWidth(window.outputWidth, bluetoothPopup.implicitWidth,
                                                window.sidePadding)
                               : bluetoothPopup.implicitWidth
            x: window.popupController && window.barGeometry
                ? window.barGeometry.popupX(window.outputWidth, bluetoothPopup.width,
                                            window.popupController.anchorX, window.sidePadding)
                : window.sidePadding
            y: window.cardY(bluetoothPopup)
            attachedTop: window.attachedStyle
            transformOrigin: Item.Top
            bluetoothService: window.bluetoothService
            opacity: window.cardOpacity(bluetoothPopup)
            scale: window.cardScale(bluetoothPopup)
        }
    }

    Item {
        objectName: "volumePopupFrame"
        x: 0
        width: window.width
        y: window.attachedStyle ? window.attachedTop : 0
        height: Math.max(0, window.height - y)
        clip: window.attachedStyle
        z: 2

        VolumePopup {
            id: volumePopup
            objectName: "volumePopup"
            visible: popupController && popupController.surfaceRequired
                      && popupController.kind === 5
            width: window.barGeometry
                ? window.barGeometry.popupWidth(window.outputWidth, volumePopup.implicitWidth,
                                                window.sidePadding)
                               : volumePopup.implicitWidth
            x: window.popupController && window.barGeometry
                ? window.barGeometry.popupX(window.outputWidth, volumePopup.width,
                                            window.popupController.anchorX, window.sidePadding)
                : window.sidePadding
            y: window.cardY(volumePopup)
            attachedTop: window.attachedStyle
            transformOrigin: Item.Top
            audioService: window.audioService
            opacity: window.cardOpacity(volumePopup)
            scale: window.cardScale(volumePopup)
        }
    }

    property Item activePopup: null

    function popupForKind(kind) {
        switch (kind) {
        case 1: return astreaMenu
        case 3: return networkPopup
        case 4: return bluetoothPopup
        case 5: return volumePopup
        default: return null
        }
    }

    NumberAnimation {
        id: popupEnter
        objectName: "popupEnter"
        target: window
        property: "presentation"
        to: 1
        duration: window.enterDuration
        // Floating cards settle with a slight spring; attached sheets glide
        // out from under the bar and must not overshoot past its edge.
        easing.type: window.attachedStyle ? Easing.OutQuint : Easing.OutBack
        easing.overshoot: 1.15
    }

    NumberAnimation {
        id: popupExit
        objectName: "popupExit"
        target: window
        property: "presentation"
        to: 0
        duration: window.exitDuration
        easing.type: window.attachedStyle ? Easing.InOutCubic : Easing.InOutQuad
        onFinished: {
            if (popupController && popupController.closing
                    && window.activePopup === window.popupForKind(popupController.kind))
                popupController.completeClose()
        }
    }

    function syncPopupPresentation() {
        if (!popupController)
            return

        if (popupController.closing) {
            popupEnter.stop()
            activePopup = popupForKind(popupController.kind)
            entering = false
            if (activePopup)
                popupExit.restart()
            else
                popupController.completeClose()
            return
        }

        if (popupController.popupOpen) {
            popupExit.stop()
            const next = popupForKind(popupController.kind)
            if (next !== activePopup)
                presentation = 0
            activePopup = next
            entering = true
            if (activePopup)
                popupEnter.restart()
        } else {
            popupEnter.stop()
            popupExit.stop()
            activePopup = null
            presentation = 0
        }
    }

    Connections {
        target: popupController
        function onChanged() { window.syncPopupPresentation() }
    }

    Component.onCompleted: syncPopupPresentation()
}
