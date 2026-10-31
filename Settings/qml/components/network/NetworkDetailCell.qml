import QtQuick
import QtQuick.Layouts
import ".." as Components

// Quiet macOS-style setting row. Keep source data bound when its tab is hidden.
Item {
    id: root

    property string label: ""
    property string sublabel: ""
    property int sublabelTextFormat: Text.PlainText

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    implicitHeight: Math.max(44, content.implicitHeight + 16)

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Components.Theme.spacingLarge
        anchors.rightMargin: Components.Theme.spacingLarge
        spacing: Components.Theme.spacingLarge

        Text {
            Layout.preferredWidth: 175
            Layout.maximumWidth: 175
            Layout.minimumWidth: 90
            text: root.label
            textFormat: Text.PlainText
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.sublabel
            textFormat: root.sublabelTextFormat
            color: Components.Theme.textPrimary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            font.weight: Components.Theme.fontWeightMedium
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.WrapAnywhere
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Components.Theme.spacingLarge
        height: 1
        color: Components.Theme.cardBorder
    }
}
