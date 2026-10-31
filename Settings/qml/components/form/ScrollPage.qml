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

    // Mouse wheels arrive in coarse notches; glide each notch into place the
    // way macOS does, folding rapid notches into one continuous motion.
    // Touchpads send pixel deltas with their own momentum and keep the native
    // path. The layer sits above the page but only claims wheel events.
    property real wheelStep: 100
    property real wheelTarget: 0
    readonly property Item wheelLayer: Item {
        objectName: "scrollPageWheelLayer"
        parent: root
        anchors.fill: parent
        z: 1

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse
            onWheel: event => {
                const flick = root.contentItem
                const notches = event.angleDelta.y / 120
                if (!flick || notches === 0 || event.pixelDelta.y !== 0) {
                    event.accepted = false
                    return
                }
                const limit = Math.max(0, flick.contentHeight - flick.height)
                if (!wheelGlide.running)
                    root.wheelTarget = flick.contentY
                root.wheelTarget = Math.max(0, Math.min(limit,
                    root.wheelTarget - notches * root.wheelStep))
                wheelGlide.to = root.wheelTarget
                wheelGlide.restart()
                event.accepted = true
            }
        }
    }

    NumberAnimation {
        id: wheelGlide
        target: root.contentItem
        property: "contentY"
        duration: Components.Theme.animationSlow
        easing.type: Components.Theme.easingStandard
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
