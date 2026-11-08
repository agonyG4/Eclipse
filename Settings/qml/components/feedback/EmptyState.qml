import QtQuick
import QtQuick.Layouts
import ".." as Components

// Centered secondary message for an empty list inside a card.
Item {
    id: root

    property string text: ""

    Layout.fillWidth: true
    Layout.preferredHeight: visible ? implicitHeight : 0
    implicitHeight: 64
    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Text {
        anchors.fill: parent
        anchors.leftMargin: Components.Theme.spacingXLarge
        anchors.rightMargin: Components.Theme.spacingXLarge
        text: root.text
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("body").family
        font.pixelSize: Components.Theme.typography.role("body").pixelSize
        lineHeight: Components.Theme.typography.role("body").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
    }
}
