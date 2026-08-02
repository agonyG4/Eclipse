import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string title: ""
    property string description: ""

    spacing: 4

    Text {
        Layout.fillWidth: true
        text: root.title
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Theme.fontWeightDemiBold
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        visible: root.description.length > 0
        text: root.description
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Theme.fontWeightNormal
        wrapMode: Text.WordWrap
    }
}
