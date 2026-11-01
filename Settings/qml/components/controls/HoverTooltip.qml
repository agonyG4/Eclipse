import QtQuick
import QtQuick.Controls
import ".." as Components

// Reusable Eclipse hover hint. Uses the same material, typography and motion
// as Settings controls, rather than the Qt Quick Controls native ToolTip.
Popup {
    id: root
    objectName: "astreaHoverTooltip"
    property Item targetItem: null
    property bool requested: false
    property string label: ""
    parent: Overlay.overlay
    modal: false
    focus: false
    closePolicy: Popup.NoAutoClose
    padding: 0
    width: Math.min(280, tooltipText.implicitWidth + 20)
    height: Math.max(28, tooltipText.implicitHeight + 12)

    function placeTooltip() {
        if (!targetItem || !parent)
            return
        const point = targetItem.mapToItem(parent, targetItem.width / 2, 0)
        x = Math.max(8, Math.min(point.x - width / 2, parent.width - width - 8))
        const above = point.y - height - 7
        y = above >= 8 ? above : Math.min(parent.height - height - 8,
                                         point.y + targetItem.height + 7)
    }

    function cancelTooltip() {
        showDelay.stop()
        if (visible)
            close()
    }

    onRequestedChanged: {
        if (requested && label.length > 0 && targetItem)
            showDelay.restart()
        else
            cancelTooltip()
    }
    onTargetItemChanged: cancelTooltip()
    onLabelChanged: if (!label.length) cancelTooltip()

    Timer {
        id: showDelay
        interval: 480
        repeat: false
        onTriggered: {
            if (root.requested && root.targetItem) {
                root.placeTooltip()
                root.open()
            }
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Components.Theme.animationQuick
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: Components.Theme.animationQuick
            easing.type: Easing.InCubic
        }
    }
    background: Rectangle {
        radius: Components.Theme.controlRadius
        color: Qt.rgba(Components.Theme.popupBg.r,
                       Components.Theme.popupBg.g,
                       Components.Theme.popupBg.b,
                       Math.max(0.96, Components.Theme.popupBg.a))
        border.width: 1
        border.color: Components.Theme.cardBorder
    }
    contentItem: Text {
        id: tooltipText
        objectName: "astreaHoverTooltipLabel"
        text: root.label
        width: Math.min(260, implicitWidth)
        color: Components.Theme.textPrimary
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        font.weight: Components.Theme.typography.role("caption").weight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
}
