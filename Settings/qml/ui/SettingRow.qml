import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string title: ""
    property string description: ""
    default property alias controlData: controlHost.data

    implicitHeight: Math.max(56, labels.implicitHeight + 20)

    RowLayout {
        anchors.fill: parent
        spacing: 16

        ColumnLayout {
            id: labels
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                font.weight: Theme.fontWeightMedium
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: controlHost
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
