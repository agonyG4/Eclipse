import QtQuick
import Astrea.Shared as Shared

// The unread pill at an icon's corner. It scales with the magnified icon, so its
// text is distance-field rendered: native glyphs would be a stretched bitmap.
Rectangle {
    id: root

    property int count: 0

    Shared.Typography { id: typography; surface: "dock" }

    x: parent ? Math.max(0, parent.width - width) : 0
    y: 0
    width: label.implicitWidth + 10
    height: Math.max(18, label.implicitHeight + 4)
    radius: height / 2
    color: "#d83b48"
    border.color: "#f6f6f6"
    border.width: 1
    z: 10

    Text {
        id: label
        objectName: "unreadCountLabel"
        anchors.centerIn: parent
        text: root.count > 99 ? "99+" : String(root.count)
        color: "white"
        font.family: typography.role("caption").family
        font.pixelSize: typography.role("caption").pixelSize
        font.weight: typography.role("caption").weight
        renderType: Text.QtRendering
    }
}
