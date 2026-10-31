import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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

    // Page scrolling: Kirigami's WheelHandler model with macOS edges.
    // - Mouse wheels (no scroll phase; QtWayland sends angleDelta only) move
    //   20 px x the system's wheel-scroll lines per notch. The glide takes
    //   longer for longer moves, capped at animationSlow, and hi-res wheel
    //   steps too small to animate land at once. Notches fold into one motion.
    // - Touchpads (begin/update/end phases) follow the finger 1:1. QtWayland
    //   never sends momentum, so inertia is computed from the last strokes on
    //   release, decelerating at a constant rate like Kirigami.
    // - Past either end the page rubber-bands along Apple's curve and springs
    //   back; inertia that reaches an end bounces instead of stopping dead.
    // - Positions snap to physical pixels so text never rests blurred.
    // - For 400 ms after the page scrolls, a latch above the content keeps the
    //   wheel on the page, so it never sticks in a nested list passing under
    //   the pointer. Otherwise nested scrollables get the wheel first.
    readonly property real wheelStep: 20 * Math.max(1, Application.styleHints.wheelScrollLines)
    property real wheelTarget: 0
    property real overscrollLimit: 64
    // Unresisted distance the finger has travelled past the nearest end.
    property real touchOverscroll: 0
    readonly property bool scrollLatched: latchTimer.running
    property var touchStrokes: []
    // Travel (px) an inertia glide still had when it met an end; it becomes
    // the bounce's stretch, about 50 ms worth of the remaining speed.
    property real inertiaOverflow: 0

    // Apple's rubber-band curve: displacement approaches the limit
    // asymptotically however far the input pushes.
    function rubberBand(distance) {
        const stretch = 1 - 1 / (Math.abs(distance) * 0.55 / root.overscrollLimit + 1)
        return Math.sign(distance) * stretch * root.overscrollLimit
    }

    function pixelAligned(y) {
        const ratio = Math.max(1, Screen.devicePixelRatio || 1)
        return Math.round(y * ratio) / ratio
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

    function stopMotion() {
        wheelGlide.stop()
        wheelBounce.stop()
        springBack.stop()
        inertia.stop()
    }

    // Bounce off an end: stretch by `distance` along the rubber band, settle.
    function bounceAt(bound, distance, outDuration) {
        bounceOut.duration = outDuration
        bounceOut.to = pixelAligned(bound + rubberBand(distance))
        bounceBack.to = bound
        wheelBounce.restart()
    }

    function scrollWheel(notches) {
        const flick = root.contentItem
        springBack.stop()
        inertia.stop()
        root.touchOverscroll = 0
        if (!wheelGlide.running && !wheelBounce.running)
            root.wheelTarget = nearestBound(flick.contentY)
        const raw = root.wheelTarget - notches * root.wheelStep
        root.wheelTarget = pixelAligned(nearestBound(raw))
        wheelBounce.stop()
        if (raw !== nearestBound(raw)) {
            wheelGlide.stop()
            bounceAt(root.wheelTarget, raw - root.wheelTarget, Components.Theme.animationFast)
            return
        }
        const distance = Math.abs(root.wheelTarget - flick.contentY)
        const ratio = Math.max(1, Screen.devicePixelRatio || 1)
        if (distance * ratio <= 2) {
            wheelGlide.stop()
            flick.contentY = root.wheelTarget
            return
        }
        wheelGlide.duration = Math.max(50, Math.min(
            Math.round(distance * Components.Theme.animationSlow / root.wheelStep),
            Components.Theme.animationSlow))
        wheelGlide.to = root.wheelTarget
        wheelGlide.restart()
    }

    function scrollTouch(delta) {
        const flick = root.contentItem
        stopMotion()
        const bound = nearestBound(flick.contentY)
        const raw = (flick.contentY === bound ? bound : bound + root.touchOverscroll) - delta
        const target = nearestBound(raw)
        root.touchOverscroll = raw - target
        flick.contentY = pixelAligned(target + rubberBand(root.touchOverscroll))
        const now = Date.now()
        root.touchStrokes = root.touchStrokes
            .filter(stroke => now - stroke.time <= 100)
            .concat([{ time: now, delta }])
        settleTimer.restart()
    }

    // Kirigami's inertia: velocity from the last 100 ms of strokes, scaled by
    // 2.5, decelerating at 10000 px/s^2 along OutQuad.
    function releaseTouch() {
        settleTimer.stop()
        const flick = root.contentItem
        const strokes = root.touchStrokes
        root.touchStrokes = []
        const bound = nearestBound(flick.contentY)
        if (flick.contentY !== bound || strokes.length < 2) {
            settle()
            return
        }
        let total = 0
        for (const stroke of strokes)
            total += stroke.delta
        const elapsed = Math.max(1, strokes[strokes.length - 1].time - strokes[0].time)
        const velocity = -total * 1000 / elapsed * 2.5
        const deceleration = 10000
        const time = Math.abs(velocity / deceleration)
        const end = flick.contentY + time * velocity / 2
        const limited = nearestBound(end)
        if (time <= 0 || limited === flick.contentY) {
            settle()
            return
        }
        const progress = (limited - flick.contentY) / (end - flick.contentY)
        // Invert OutQuad to find how long the bounded part of the glide takes.
        const partial = time * (1 - Math.sqrt(Math.max(0, 1 - progress)))
        inertia.to = pixelAligned(limited)
        inertia.duration = Math.max(1, Math.round(partial * 1000))
        // Speed left when the glide meets an end becomes a bounce.
        root.inertiaOverflow = limited === end ? 0
            : velocity * Math.sqrt(Math.max(0, 1 - progress)) * 0.05
        inertia.restart()
    }

    function settle() {
        const flick = root.contentItem
        const bound = root.nearestBound(flick.contentY)
        root.touchOverscroll = 0
        if (flick.contentY !== bound) {
            springBack.to = bound
            springBack.restart()
        }
    }

    function handleWheel(event) {
        latchTimer.restart()
        if (event.phase === Qt.NoScrollPhase && event.angleDelta.y !== 0) {
            root.scrollWheel(event.angleDelta.y / 120)
            return
        }
        if (event.phase === Qt.ScrollEnd) {
            root.releaseTouch()
            return
        }
        const delta = event.pixelDelta.y !== 0
            ? event.pixelDelta.y : event.angleDelta.y / 8
        if (delta !== 0)
            root.scrollTouch(delta)
    }

    // Attached once the ScrollView has built its Flickable. Flickable hands
    // the handler to its moving contentItem, which still sits above nothing
    // but the page's own content.
    Component.onCompleted: pageWheel.parent = root.contentItem

    WheelHandler {
        id: pageWheel
        objectName: "scrollPageWheelHandler"
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        orientation: Qt.Vertical
        onWheel: event => root.handleWheel(event)
    }

    readonly property Item wheelLatch: Item {
        objectName: "scrollPageWheelLatch"
        parent: root
        anchors.fill: parent
        z: 1
        enabled: root.scrollLatched

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            orientation: Qt.Vertical
            onWheel: event => root.handleWheel(event)
        }
    }

    Timer {
        id: latchTimer
        interval: 400
    }

    NumberAnimation {
        id: wheelGlide
        target: root.contentItem
        property: "contentY"
        easing.type: Components.Theme.easingStandard
    }

    NumberAnimation {
        id: inertia
        target: root.contentItem
        property: "contentY"
        easing.type: Easing.OutQuad
        onFinished: {
            if (root.inertiaOverflow !== 0) {
                const overflow = root.inertiaOverflow
                root.inertiaOverflow = 0
                root.bounceAt(root.nearestBound(root.contentItem.contentY), overflow,
                              Components.Theme.animationFast)
            }
        }
    }

    SequentialAnimation {
        id: wheelBounce
        NumberAnimation {
            id: bounceOut
            target: root.contentItem
            property: "contentY"
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

    // Fallback for platforms that never send ScrollEnd: settle once input stops.
    Timer {
        id: settleTimer
        interval: 120
        onTriggered: {
            root.touchStrokes = []
            root.settle()
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
