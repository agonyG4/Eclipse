import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "applicationsPage"

    property string searchText: ""
    property string selectedAppId: ""

    readonly property var controller: SettingsController.applications
    readonly property var apps: controller.state.apps || []
    readonly property var filteredApps: root.apps.filter(app => root.searchText.trim() === "" || (app.name + " " + app.id + " " + app.category).toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1)
    readonly property var selectedApp: controller.state.selected && controller.state.selected.id === root.selectedAppId ? controller.state.selected : null

    Dialog {
        id: removalDialog
        objectName: "applicationRemovalDialog"
        anchors.centerIn: parent
        modal: true
        width: Math.min(root.width - 48, 480)
        title: root.selectedApp ? root.selectedApp.actionLabel + " — " + root.selectedApp.name : "Remove application"
        padding: 18
        background: Rectangle {
            color: Components.Theme.popupBg
            radius: Components.Theme.cornerRadius
            border.width: 1
            border.color: Components.Theme.cardBorder
        }
        header: Text {
            text: removalDialog.title
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Components.Theme.textPrimary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeLarge
            font.weight: Components.Theme.fontWeightDemiBold
            leftPadding: 18
            rightPadding: 18
            topPadding: 16
        }
        footer: Item {
            implicitHeight: 52
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.bottomMargin: 12
                Item { Layout.fillWidth: true }
                Controls.ButtonCapsule {
                    objectName: "cancelApplicationRemovalButton"
                    label: "Cancel"
                    onClicked: removalDialog.reject()
                }
                Controls.ButtonCapsule {
                    objectName: "confirmApplicationRemovalButton"
                    label: root.selectedApp ? root.selectedApp.actionLabel : "Remove"
                    danger: true
                    focus: false
                    onClicked: removalDialog.accept()
                }
            }
        }
        contentItem: Text {
            text: root.selectedApp ? "Target: " + root.selectedApp.target + "\n" + (root.selectedApp.method === "launcher" ? "Only the launcher is removed. Application files are kept." : "The selected application or package will be removed. Application data is kept.") : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Components.Theme.textPrimary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
        }
        onAccepted: if (root.selectedApp) root.controller.uninstall(root.selectedAppId, root.selectedApp.removalToken)
    }
    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        objectName: "applicationsScrollPage"
        anchors.fill: parent
        enabled: !root.controller.busy
        contentMargins: 24
        maxWidth: 840

        Components.BackendNotice {
            objectName: "applicationsBackendStatus"
            busy: root.controller.busy
            error: root.controller.lastError
            onRetry: root.controller.refresh()
        }

        Form.SectionHeader { text: "APPS"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "installedApplicationsCard"
            visible: root.selectedApp === null
            Layout.bottomMargin: 22

            Item {
                Layout.fillWidth: true
                implicitHeight: 62
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: "Installed applications"
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeLarge
                            font.weight: Components.Theme.fontWeightDemiBold
                        }
                        Text {
                            text: root.filteredApps.length + " apps shown · " + root.apps.length + " installed applications"
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeTiny
                        }
                    }
                    Controls.SearchField {
                        objectName: "applicationsSearchField"
                        Layout.preferredWidth: 176
                        controlHeight: 36
                        placeholderText: "Search apps"
                        text: root.searchText
                        onTextEdited: value => root.searchText = value
                    }
                }
            }

            Repeater {
                model: root.filteredApps
                delegate: Form.SettingRow {
                    required property var modelData
                    required property int index
                    objectName: "applicationRow_" + modelData.id.replace(/[^A-Za-z0-9]/g, "_")
                    clickable: !root.controller.busy
                    label: modelData.name
                    sublabel: modelData.description
                    isLast: index === root.filteredApps.length - 1
                    Text {
                        text: root.selectedAppId === modelData.id ? "Selected" : modelData.category
                        color: root.selectedAppId === modelData.id ? Components.Theme.accent : Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeTiny
                    }
                    onClicked: { root.selectedAppId = modelData.id; root.controller.select(modelData.id) }
                }
            }

            Item {
                visible: root.filteredApps.length === 0
                Layout.fillWidth: true
                implicitHeight: 64
                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    text: "No apps match “" + root.searchText + "”."
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }

        Form.FormCard {
            objectName: "applicationDetailsCard"
            visible: root.selectedApp !== null
            Layout.bottomMargin: 22

            Item {
                Layout.fillWidth: true
                implicitHeight: 74
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: Components.Theme.cornerRadius
                        color: Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g, Components.Theme.accent.b, 0.15)
                        Text {
                            anchors.centerIn: parent
                            text: root.selectedApp ? root.selectedApp.name.slice(0, 1) : "A"
                            color: Components.Theme.accent
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeTitle
                            font.weight: Components.Theme.fontWeightDemiBold
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            objectName: "applicationDetailTitle"
                            text: root.selectedApp ? root.selectedApp.name : ""
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeLarge
                            font.weight: Components.Theme.fontWeightDemiBold
                        }
                        Text {
                            text: root.selectedApp ? root.selectedApp.description : ""
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeTiny
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    Controls.ButtonCapsule {
                        objectName: "closeApplicationDetailsButton"
                        label: "Back"
                        onClicked: root.selectedAppId = ""
                    }
                }
            }

            Form.SectionHeader { text: "INFORMATION"; Layout.bottomMargin: 6 }
            Form.SettingRow { label: "Type"; sublabel: root.selectedApp ? root.selectedApp.kind : "" }
            Form.SettingRow { label: "Size"; sublabel: root.selectedApp ? root.selectedApp.size : "" }
            Form.SettingRow { objectName: "applicationIdRow"; label: "ID"; sublabel: root.selectedApp ? root.selectedApp.id : "" }
            Form.SettingRow { objectName: "applicationFileRow"; label: "File"; sublabel: root.selectedApp ? root.selectedApp.file : ""; sublabelTextFormat: Text.PlainText; isLast: true }

            Form.SectionHeader { text: "PRIVACY"; Layout.topMargin: 16; Layout.bottomMargin: 6 }
            Form.SettingRow {
                objectName: "applicationPrivacyRow"
                label: "Permissions"
                sublabelTextFormat: Text.PlainText
                sublabel: root.selectedApp ? root.selectedApp.privacy : ""
                isLast: true
            }

            Form.SectionHeader { text: "ACTIONS"; Layout.topMargin: 16; Layout.bottomMargin: 8 }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 14
                spacing: 8
                Controls.ButtonCapsule { objectName: "createApplicationShortcutButton"; label: "Create shortcut"; onClicked: root.controller.createShortcut(root.selectedAppId) }
                Controls.ButtonCapsule { objectName: "openApplicationLocationButton"; label: "Open location"; onClicked: root.controller.openLocation(root.selectedAppId) }
                Item { Layout.fillWidth: true }
                Controls.ButtonCapsule { objectName: "uninstallApplicationButton"; label: root.selectedApp ? root.selectedApp.actionLabel : "Uninstall"; danger: true; enabled: root.selectedApp !== null && !!root.selectedApp.canUninstall; onClicked: removalDialog.open() }
            }
        }
    }
}
