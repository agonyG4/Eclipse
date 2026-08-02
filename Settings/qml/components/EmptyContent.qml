import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string sectionTitle: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(460, root.width - 80)
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 54
            Layout.preferredHeight: 54
            radius: 17
            color: Theme.surfaceSelected

            Text {
                anchors.centerIn: parent
                text: "⚙"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 25
                font.weight: Theme.fontWeightMedium
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.sectionTitle.length > 0 ? root.sectionTitle : qsTr("Settings")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeHeader
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("The native Settings foundation is ready. Pages and system integrations are intentionally not part of this phase.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeNormal
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
