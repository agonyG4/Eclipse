import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string userName: ""
    property url avatarUrl
    property bool collapsed: false

    implicitHeight: 62

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.collapsed ? 14 : 16
        anchors.rightMargin: root.collapsed ? 14 : 16
        spacing: 12

        ProfileAvatar {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            source: root.avatarUrl
            fallbackText: root.userName.length > 0 ? root.userName[0].toUpperCase() : "?"
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            visible: !root.collapsed
            opacity: visible ? 1 : 0

            Text {
                Layout.fillWidth: true
                text: root.userName
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Theme.fontWeightMedium
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Local account")
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTiny
                elide: Text.ElideRight
            }
        }
    }
}
