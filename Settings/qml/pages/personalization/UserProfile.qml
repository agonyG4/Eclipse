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
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.pages.user_profile.title", "User Profile")
            subtitle: I18n.tr("settings.pages.user_profile.subtitle", "Your name, picture and sign-in.")
        }

        Components.BackendNotice {
            objectName: "userProfileBackendStatus"
            busy: root.controller.busy
            error: root.controller.lastError || root.snapshot.accountError || ""
            onRetry: root.controller.refresh()
        }

        Form.FormCard {
            objectName: "userProfileCard"
            Layout.bottomMargin: 24

            Item {
                objectName: "userProfileHeader"
                Layout.fillWidth: true
                implicitHeight: profileHeaderColumn.implicitHeight + 48
                ColumnLayout {
                    id: profileHeaderColumn
                    anchors.centerIn: parent
                    spacing: 8
                    Item {
                        id: avatarFrame
                        Layout.alignment: Qt.AlignHCenter
                        Layout.bottomMargin: 4
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
                        // The picture itself is the target; hover reveals what clicking does.
                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: Qt.rgba(0, 0, 0, avatarMouse.containsMouse && avatarMouse.enabled ? 0.45 : 0)
                            Behavior on color { ColorAnimation { duration: Components.Theme.animationQuick } }
                            Text {
                                anchors.centerIn: parent
                                visible: avatarMouse.containsMouse && avatarMouse.enabled
                                text: "Edit"
                                color: "#ffffff"
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                font.weight: Components.Theme.typography.role("caption").weight
                            }
                        }
                        MouseArea {
                            id: avatarMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: !!root.snapshot.accountsAvailable && !root.controller.busy
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: avatarDialog.open()
                        }
                    }
                    Text {
                        objectName: "userProfileDisplayName"
                        Layout.alignment: Qt.AlignHCenter
                        text: root.displayName || root.userName
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("title").family
                        font.pixelSize: Components.Theme.typography.role("title").pixelSize
                        lineHeight: Components.Theme.typography.role("title").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("title").letterSpacing
                        font.weight: Components.Theme.typography.role("title").weight
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: (root.snapshot.administrator ? "Administrator · " : "Standard user · ") + root.userName
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("caption").family
                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                    }
                    Controls.ButtonCapsule {
                        objectName: "changeAvatarButton"
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        label: "Change Picture…"
                        accessibleName: "Choose a profile picture"
                        enabled: !!root.snapshot.accountsAvailable && !root.controller.busy
                        onClicked: avatarDialog.open()
                    }
                }
            }
        }

        Form.SectionHeader {
            text: "ACCOUNT"
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "userAccountCard"
            Layout.bottomMargin: 24

            Form.SettingRow {
                objectName: "displayNameRow"
                label: "Display Name"
                sublabel: "Name shown on the lock screen and in menus."
                RowLayout {
                    spacing: 6
                    TextField {
                        objectName: "displayNameField"
                        Accessible.name: "Display Name"
                        Layout.preferredWidth: 180
                        implicitHeight: 32
                        leftPadding: 10
                        rightPadding: 10
                        text: root.pendingDisplayName
                        color: Components.Theme.textPrimary
                        selectionColor: Components.Theme.accent
                        selectedTextColor: Components.Theme.accentForeground
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        background: Rectangle {
                            radius: Components.Theme.controlRadius
                            color: Components.Theme.windowBackground
                            border.width: 1
                            border.color: parent.activeFocus ? Components.Theme.accent : Components.Theme.cardBorder
                        }
                        onTextEdited: root.pendingDisplayName = text
                    }
                    Controls.ButtonCapsule {
                        objectName: "saveDisplayNameButton"
                        label: "Save"
                        // Only a real change is worth saving.
                        enabled: !!root.snapshot.accountsAvailable && root.pendingDisplayName.trim().length > 0
                            && root.pendingDisplayName.trim() !== root.displayName
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
                isLast: true
            }
        }

        Form.SectionHeader {
            text: "SIGN-IN"
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "userSignInCard"
            Layout.bottomMargin: 24

            Form.SettingRow {
                objectName: "automaticLoginRow"
                label: "Automatic login"
                sublabel: root.snapshot.autologinAvailable ? "Start the installed session " + root.snapshot.session + " without asking for a password." : "Install the profile helper and an SDDM session to enable this control."
                Controls.ToggleSwitch {
                    accessibleName: "Automatic login"
                    enabled: !!root.snapshot.autologinAvailable
                    checked: root.automaticLogin
                    onToggled: checked => root.controller.setAutomaticLogin(checked)
                }
                isLast: true
            }
        }
    }
}
