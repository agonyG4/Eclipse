import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string sectionTitle: ""

    Rectangle {
        anchors.fill: parent
        color: "#0E2038"
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.panelGradientStart }
            GradientStop { position: 0.42; color: Theme.panelBackground }
            GradientStop { position: 1.0; color: Theme.panelGradientEnd }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Theme.panelBorder
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        width: parent.width * 0.64
        height: parent.height * 0.48
        radius: width * 0.5
        color: Theme.windowGlow
        opacity: Theme.dark ? 0.08 : 0.14
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        anchors.topMargin: 22
        anchors.bottomMargin: Theme.pageMargin
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: qsTr("Settings")
                color: Theme.textTertiary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Theme.fontWeightMedium
            }

            Text {
                Layout.fillWidth: true
                text: root.sectionTitle.length > 0 ? root.sectionTitle : qsTr("Settings")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeHeader
                font.weight: Theme.fontWeightDemiBold
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.separator
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(520, parent.width - 48)
                spacing: 14

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 58
                    radius: 18
                    color: Theme.surfaceSelected
                    border.width: 1
                    border.color: Theme.accent
                    opacity: 0.92

                    Text {
                        anchors.centerIn: parent
                        text: "⚙"
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: 26
                        font.weight: Theme.fontWeightMedium
                        Accessible.ignored: true
                    }
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
    }
}
