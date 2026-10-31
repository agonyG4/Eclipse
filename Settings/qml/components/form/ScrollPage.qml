import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as Components

/**
 * ScrollPage - A standardized scrollable container for settings pages.
 * Ensures the scrollbar is at the far right edge of the window while
 * maintaining centered content with proper margins.
 */
ScrollView {
    id: root
    anchors.fill: parent
    clip: true

    // Configurable properties
    property real contentMargins: Components.Theme.pageMargin
    property real topContentInset: Math.min(16, contentMargins)
    property real maxWidth:       800  // Maximum width for the content area
    property real scrollGap:      14

    // Default property allows placing items directly inside ScrollPage
    default property alias content: contentColumn.data

    contentWidth: availableWidth
    // The trailing gutter belongs to scrollable contentHeight. Putting a footer
    // Item in contentColumn.data places it before pages' aliased children.
    contentHeight: contentColumn.implicitHeight + root.contentMargins + 12
    ScrollBar.vertical: ScrollBar {
        parent: root
        anchors {
            top: root.top
            bottom: root.bottom
            right: root.right
            topMargin: root.contentMargins
            bottomMargin: root.contentMargins + 12
            rightMargin: 0
        }
        policy: ScrollBar.AsNeeded
    }

    // Page scrolling, macOS style. The handler sits on the page's Flickable,
    // so nested scrollables (Wi-Fi list, device strip) still see wheel events
    // first; only what reaches the page lands here.
    // - Mouse wheels (no scroll phase) arrive in coarse notches; each notch
    //   glides into place, and rapid notches fold into one continuous motion.
    //   QtWayland also attaches a pixel delta to notches, so notches are read
    //   from angleDelta only.
    // - Touchpads (begin/update/end/momentum phases) follow the finger 1:1,
    //   including any momentum the platform sends.
    // Past either end the page rubber-bands like macOS: it keeps moving with
    // growing resistance, then springs back once input stops.
    property real wheelStep: 100
    property real wheelTarget: 0
    property real overscrollLimit: 64
    // Unresisted distance the finger has travelled past the nearest end.
    property real touchOverscroll: 0

    // Apple's rubber-band curve: displacement approaches the limit
    // asymptotically however far the input pushes.
    function rubberBand(distance) {
        const stretch = 1 - 1 / (Math.abs(distance) * 0.55 / root.overscrollLimit + 1)
        return Math.sign(distance) * stretch * root.overscrollLimit
    }

    function scrollBounds() {
        const flick = root.contentItem
        const top = flick.originY
        return { top, bottom: top + Math.max(0, flick.contentHeight - flick.height) }
    }

    function nearestBound(y) {
        const bounds = scrollBounds()
        return y < bounds.top ? bounds.top : (y > bounds.bottom ? bounds.bottom : y)
    }

    function scrollWheel(notches) {
        const flick = root.contentItem
        const bounds = scrollBounds()
        springBack.stop()
        root.touchOverscroll = 0
        if (!wheelGlide.running && !wheelBounce.running)
            root.wheelTarget = nearestBound(flick.contentY)
        const raw = root.wheelTarget - notches * root.wheelStep
        root.wheelTarget = nearestBound(raw)
        if (raw === root.wheelTarget) {
            wheelBounce.stop()
            wheelGlide.to = raw
            wheelGlide.restart()
            return
        }
        // Spinning past an end stretches the page a little, then settles.
        wheelGlide.stop()
        bounceOut.to = root.wheelTarget + rubberBand(raw - root.wheelTarget)
        bounceBack.to = root.wheelTarget
        wheelBounce.restart()
    }

    function scrollTouch(delta) {
        const flick = root.contentItem
        wheelGlide.stop()
        wheelBounce.stop()
        springBack.stop()
        const bound = nearestBound(flick.contentY)
        const raw = (flick.contentY === bound ? bound : bound + root.touchOverscroll) - delta
        const target = nearestBound(raw)
        root.touchOverscroll = raw - target
        flick.contentY = target + rubberBand(root.touchOverscroll)
        settleTimer.restart()
    }

    // Attached once the ScrollView has built its Flickable.
    Component.onCompleted: pageWheel.parent = root.contentItem

    WheelHandler {
        id: pageWheel
        objectName: "scrollPageWheelHandler"
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        orientation: Qt.Vertical
        onWheel: event => {
            if (event.phase === Qt.NoScrollPhase && event.angleDelta.y !== 0) {
                root.scrollWheel(event.angleDelta.y / 120)
                return
            }
            const delta = event.pixelDelta.y !== 0
                ? event.pixelDelta.y : event.angleDelta.y / 8
            if (delta !== 0)
                root.scrollTouch(delta)
        }
    }

    NumberAnimation {
        id: wheelGlide
        target: root.contentItem
        property: "contentY"
        duration: Components.Theme.animationSlow
        easing.type: Components.Theme.easingStandard
    }

    SequentialAnimation {
        id: wheelBounce
        NumberAnimation {
            id: bounceOut
            target: root.contentItem
            property: "contentY"
            duration: Components.Theme.animationFast
            easing.type: Components.Theme.easingStandard
        }
        NumberAnimation {
            id: bounceBack
            target: root.contentItem
            property: "contentY"
            duration: Components.Theme.animationPopover
            easing.type: Components.Theme.easingEnter
        }
    }

    // Touchpads send no "finger lifted" signal we can rely on across
    // platforms (momentum may follow ScrollEnd), so settle after input stops.
    Timer {
        id: settleTimer
        interval: 120
        onTriggered: {
            const flick = root.contentItem
            const bound = root.nearestBound(flick.contentY)
            root.touchOverscroll = 0
            if (flick.contentY !== bound) {
                springBack.to = bound
                springBack.restart()
            }
        }
    }

    NumberAnimation {
        id: springBack
        target: root.contentItem
        property: "contentY"
        duration: Components.Theme.animationPopover
        easing.type: Components.Theme.easingEnter
    }

    ColumnLayout {
        id: contentColumn
        // Base width on availableWidth so the content yields space when the
        // vertical scrollbar becomes visible, keeping the indicator outside
        // the visual content block.
        width: Math.min(
            Math.max(0, root.availableWidth - (root.contentMargins * 2) - root.scrollGap),
            root.maxWidth
        )
        anchors.horizontalCenter: parent.horizontalCenter

        spacing: 0

        // Top inset is independent of the horizontal content margins.
        // A trailing spacer must not be placed before aliased page children.
        Item {
            Layout.preferredHeight: root.topContentInset
            Layout.minimumHeight: root.topContentInset
            Layout.maximumHeight: root.topContentInset
        }

        // Content items are appended by the page through the content alias.
        // Do not declare a trailing Item here: it would precede page children.
    }
}
