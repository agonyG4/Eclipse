import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "userProfilePage"

    readonly property var controller: SettingsController.profile
    readonly property var snapshot: controller.state
    readonly property string userName: snapshot.user || SettingsController.userName
    readonly property string displayName: snapshot.displayName || userName
    property string pendingDisplayName: displayName
    readonly property bool automaticLogin: !!snapshot.automaticLogin
    FileDialog {
        id: avatarDialog
        title: "Choose a profile picture"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: root.controller.setAvatar(selectedFile)
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        objectName: "userProfileScrollPage"
        anchors.fill: parent
        enabled: !root.controller.busy
        contentMargins: 24
        maxWidth: 840

        Components.BackendNotice {
            objectName: "userProfileBackendStatus"
            busy: root.controller.busy
            error: root.controller.lastError || root.snapshot.accountError || ""
            onRetry: root.controller.refresh()
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
                            imagePath: String(root.snapshot.avatar || "")
                            imageVersion: root.snapshot.avatarVersion || 0
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
                            MouseArea { anchors.fill: parent; enabled: !!root.snapshot.accountsAvailable && !root.controller.busy; cursorShape: Qt.PointingHandCursor; onClicked: avatarDialog.open() }
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
                        text: (root.snapshot.administrator ? "Administrator · " : "Standard user · ") + root.userName
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
                        label: "Save"
                        enabled: !!root.snapshot.accountsAvailable && root.pendingDisplayName.trim().length > 0
                        primary: true
                        onClicked: root.controller.setDisplayName(root.pendingDisplayName)
                    }
                }
            }

            Form.SettingRow {
                objectName: "changePasswordRow"
                label: "Change Password"
                sublabel: root.snapshot.passwordAvailable ? "Open the system password tool in a terminal." : "Install xdg-terminal-exec to open the system password tool."
                Controls.ButtonCapsule {
                    objectName: "changePasswordButton"
                    label: "Change…"
                    enabled: !!root.snapshot.passwordAvailable
                    onClicked: root.controller.changePassword()
                }
            }

            Form.SettingRow {
                objectName: "automaticLoginRow"
                label: "Automatic Login SDDM"
                sublabel: root.snapshot.autologinAvailable ? "Start the installed session " + root.snapshot.session + " without asking for a password." : "Install the profile helper and an SDDM session to enable this control."
                Controls.ToggleSwitch {
                    accessibleName: "Automatic Login SDDM"
                    enabled: !!root.snapshot.autologinAvailable
                    checked: root.automaticLogin
                    onToggled: checked => root.controller.setAutomaticLogin(checked)
                }
                isLast: true
            }
        }
    }
}
