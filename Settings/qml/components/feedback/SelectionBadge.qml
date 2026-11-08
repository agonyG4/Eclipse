import QtQuick
import ".." as Components

// Accent check badge shown in the corner of a selected card or tile. Pair it
// with a 2 px accent border on the tile so every selectable tile in Settings
// reads the same way.
Rectangle {
    id: badge

    property bool selected: false
    property int diameter: 18

    visible: selected
    anchors.top: parent ? parent.top : undefined
    anchors.right: parent ? parent.right : undefined
    anchors.topMargin: Components.Theme.spacingSmall
    anchors.rightMargin: Components.Theme.spacingSmall
    width: diameter
    height: diameter
    radius: diameter / 2
    color: Components.Theme.accent
    Accessible.ignored: true

    Text {
        anchors.centerIn: parent
        text: ""
        color: Components.Theme.accentForeground
        font.family: Components.Theme.iconFontFamily
        font.pixelSize: Math.round(badge.diameter * 0.56)
        font.weight: Components.Theme.fontWeightBold
    }
}
