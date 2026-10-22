import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "applicationsPage"

    property string searchText: ""
    property string selectedAppId: ""

    readonly property var apps: [
        { id: "org.gnome.Calculator", name: "Calculator", description: "Perform calculations, unit conversions and quick checks.", kind: "Desktop application", size: "3.8 MB", file: "/usr/bin/gnome-calculator", category: "Utilities", privacy: "No special permissions" },
        { id: "org.gnome.Nautilus", name: "Files", description: "Browse and organize files and folders.", kind: "Desktop application", size: "8.2 MB", file: "/usr/bin/nautilus", category: "System", privacy: "Home folder access" },
        { id: "org.gnome.TextEditor", name: "Text Editor", description: "A simple editor for text and code.", kind: "Desktop application", size: "2.4 MB", file: "/usr/bin/gnome-text-editor", category: "Utilities", privacy: "No special permissions" },
        { id: "org.mozilla.firefox", name: "Firefox", description: "Browse the web with Firefox.", kind: "Desktop application", size: "286 MB", file: "/usr/bin/firefox", category: "Network", privacy: "Network access" },
        { id: "org.gnome.Terminal", name: "Terminal", description: "Use a command-line environment.", kind: "Desktop application", size: "5.1 MB", file: "/usr/bin/gnome-terminal", category: "System", privacy: "Shell access" },
        { id: "com.spotify.Client", name: "Music Player", description: "Listen to music and podcasts.", kind: "Desktop application", size: "114 MB", file: "/usr/bin/music-player", category: "Audio", privacy: "Network access" }
    ]
    readonly property var filteredApps: root.apps.filter(app => root.searchText.trim() === "" || app.name.toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1 || app.id.toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1 || app.category.toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1)
    readonly property var selectedApp: {
        for (let i = 0; i < root.apps.length; ++i) {
            if (root.apps[i].id === root.selectedAppId)
                return root.apps[i]
        }
        return null
    }

    Form.ScrollPage {
        objectName: "applicationsScrollPage"
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "applicationsPreviewNotice"
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
                            text: root.filteredApps.length + " apps shown · " + root.apps.length + " sample entries"
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
                    clickable: true
                    label: modelData.name
                    sublabel: modelData.description
                    isLast: index === root.filteredApps.length - 1
                    Text {
                        text: root.selectedAppId === modelData.id ? "Selected" : modelData.category
                        color: root.selectedAppId === modelData.id ? Components.Theme.accent : Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeTiny
                    }
                    onClicked: root.selectedAppId = modelData.id
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
                Controls.ButtonCapsule { label: "Create shortcut"; enabled: false }
                Controls.ButtonCapsule { label: "Open location"; enabled: false }
                Item { Layout.fillWidth: true }
                Controls.ButtonCapsule { objectName: "uninstallApplicationButton"; label: "Uninstall"; danger: true; enabled: false }
            }
        }
    }
}
