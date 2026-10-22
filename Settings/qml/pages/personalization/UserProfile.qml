import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "userProfilePage"

    property string userName: "alex"
    property string displayName: "Alex Morgan"
    property string pendingDisplayName: "Alex Morgan"
    property bool automaticLogin: false

    Form.ScrollPage {
        objectName: "userProfileScrollPage"
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "userProfilePreviewNotice"
            Layout.fillWidth: true
            Layout.bottomMargin: 18
            implicitHeight: 34
            radius: Components.Theme.controlRadius
            color: Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035)
            border.width: 1
            border.color: Components.Theme.cardBorder
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                text: "Visual preview · Sample data, local changes"
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
            }
        }

        Form.SectionHeader { text: "USER PROFILE"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "userProfileCard"
            Layout.bottomMargin: 22

            Item {
                objectName: "userProfileHeader"
                Layout.fillWidth: true
                implicitHeight: 178
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        width: 88
                        height: 88
                        Components.AvatarImage {
                            objectName: "userProfileAvatar"
                            anchors.fill: parent
                            fallbackText: root.displayName.length > 0 ? root.displayName[0].toUpperCase() : "?"
                            fallbackFontPixelSize: 36
                            fallbackFontWeight: Components.Theme.fontWeightDemiBold
                            borderWidth: 1
                            borderColor: Components.Theme.cardBorder
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 27
                            height: 27
                            radius: 14
                            color: Components.Theme.cardBg
                            border.width: 1
                            border.color: Components.Theme.cardBorder
                            Text {
                                anchors.centerIn: parent
                                text: "✎"
                                color: Components.Theme.textTertiary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeSmall
                            }
                            MouseArea { anchors.fill: parent; enabled: false }
                        }
                    }
                    Text {
                        objectName: "userProfileDisplayName"
                        Layout.alignment: Qt.AlignHCenter
                        text: root.displayName || root.userName
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeTitle
                        font.weight: Components.Theme.fontWeightDemiBold
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Administrator · " + root.userName
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                }
            }

            Form.SettingRow {
                objectName: "displayNameRow"
                label: "Display Name"
                sublabel: "Name shown on the lock screen and in menus."
                RowLayout {
                    spacing: 6
                    TextField {
                        objectName: "displayNameField"
                        Accessible.name: "Display Name"
                        Layout.preferredWidth: 138
                        implicitHeight: 34
                        text: root.pendingDisplayName
                        color: Components.Theme.textPrimary
                        selectionColor: Components.Theme.accent
                        selectedTextColor: Components.Theme.accentForeground
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                        onTextEdited: root.pendingDisplayName = text
                    }
                    Controls.ButtonCapsule {
                        objectName: "saveDisplayNameButton"
                        label: "Update preview"
                        primary: true
                        onClicked: {
                            const normalized = root.pendingDisplayName.trim().replace(/\s+/g, " ")
                            if (normalized.length > 0) {
                                root.pendingDisplayName = normalized
                                root.displayName = normalized
                            }
                        }
                    }
                }
            }

            Form.SettingRow {
                objectName: "changePasswordRow"
                label: "Change Password"
                sublabel: "Update the login and administrator password."
                Controls.ButtonCapsule {
                    objectName: "changePasswordButton"
                    label: "Change…"
                    enabled: false
                }
            }

            Form.SettingRow {
                objectName: "automaticLoginRow"
                label: "Automatic Login SDDM"
                sublabel: "Start a session for this user without asking for a password."
                Controls.ToggleSwitch {
                    accessibleName: "Automatic Login SDDM"
                    checked: root.automaticLogin
                    onToggled: checked => root.automaticLogin = checked
                }
                isLast: true
            }
        }
    }
}
