import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string userName: ""
    property url avatarUrl
    property bool collapsed: false

    implicitHeight: 68

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        radius: Theme.radiusLarge
        color: "#626872"
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.accountBackground }
            GradientStop { position: 1.0; color: Theme.accountGradientEnd }
        }
        border.width: 1
        border.color: Theme.accountBorder
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
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
                font.pixelSize: Theme.fontSizeTitle
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
