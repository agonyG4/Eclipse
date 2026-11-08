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
    property real savedListScrollY: 0

    // Page-local refinements. Core Borealis theme colors and components remain authoritative.
    readonly property real groupRadius: Math.max(16, Components.Theme.cardRadius + 4)
    readonly property real insetRadius: Math.max(10, groupRadius - 6)

    readonly property color quietSurface: Components.Theme.isLight
        ? Qt.rgba(0, 0, 0, 0.035) : Qt.rgba(1, 1, 1, 0.043)
    readonly property color quietBorder: Components.Theme.isLight
        ? Qt.rgba(0, 0, 0, 0.065) : Qt.rgba(1, 1, 1, 0.07)
    readonly property color hoverSurface: Components.Theme.isLight
        ? Qt.rgba(0, 0, 0, 0.052) : Qt.rgba(1, 1, 1, 0.065)

    readonly property var controller: SettingsController.applications
    readonly property var apps: controller.state.apps || []
    readonly property var selectedApp: controller.state.selected
        && controller.state.selected.id === root.selectedAppId
        ? controller.state.selected : null
    readonly property var selectedSummary: root.apps.find(app => app.id === root.selectedAppId) || null
    readonly property var displayApp: root.selectedApp || root.selectedSummary
    readonly property var filteredApps: root.apps.filter(function(app) {
        const query = root.searchText.trim().toLocaleLowerCase()
        return query === "" || [app.name, app.description, app.id]
            .join(" ").toLocaleLowerCase().indexOf(query) !== -1
    })

    function openApp(app) {
        if (!app || !app.id)
            return
        root.savedListScrollY = appsList.contentY
        root.selectedAppId = app.id
        root.controller.select(app.id)
    }

    function closeDetails() {
        root.selectedAppId = ""
        Qt.callLater(() => {
            if (root.selectedAppId === "")
                appsList.contentY = Math.max(0, Math.min(root.savedListScrollY,
                    Math.max(0, appsList.contentHeight - appsList.height)))
        })
    }

    Component.onCompleted: root.controller.refresh()

    Connections {
        target: root.controller
        function onStateChanged() {
            if (root.selectedAppId !== "" && !root.controller.busy
                    && root.apps.length > 0 && !root.selectedSummary)
                root.closeDetails()
        }
    }

    Dialog {
        id: removalDialog
        objectName: "applicationRemovalDialog"
        anchors.centerIn: parent
        modal: true
        width: Math.min(root.width - 40, 480)
        title: root.selectedApp
            ? root.selectedApp.actionLabel + " — " + root.selectedApp.name
            : I18n.tr("settings.apps.remove_application", "Remove application")
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
            font.family: Components.Theme.typography.role("label").family
            font.pixelSize: Components.Theme.typography.role("label").pixelSize
            lineHeight: Components.Theme.typography.role("label").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
            font.weight: Components.Theme.typography.role("label").weight
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
                    label: I18n.tr("settings.apps.cancel", "Cancel")
                    onClicked: removalDialog.reject()
                }
                Controls.ButtonCapsule {
                    objectName: "confirmApplicationRemovalButton"
                    label: root.selectedApp ? root.selectedApp.actionLabel : I18n.tr("settings.apps.remove", "Remove")
                    danger: true
                    focus: false
                    onClicked: removalDialog.accept()
                }
            }
        }
        contentItem: Text {
            text: root.selectedApp
                ? I18n.tr("settings.apps.target", "Target: ") + root.selectedApp.target + "\n"
                    + (root.selectedApp.method === "launcher"
                        ? I18n.tr("settings.apps.only_the_launcher_is_removed_application_fil", "Only the launcher is removed. Application files are kept.")
                        : I18n.tr("settings.apps.the_application_or_package_will_be_removed_a", "The application or package will be removed. Application data is kept."))
                : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Components.Theme.textPrimary
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        }
        onAccepted: {
            if (root.selectedApp && root.selectedApp.canUninstall)
                root.controller.uninstall(root.selectedAppId, root.selectedApp.removalToken)
        }
    }

    Item {
        id: catalogPage
        anchors.fill: parent
        visible: root.selectedAppId === ""

        ColumnLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 16
            anchors.bottomMargin: 18
            // Same column as Form.ScrollPage: 32px margins, 900px wide.
            width: Math.min(900, Math.max(0, parent.width - 78))
            spacing: 0

            Form.PageHeader {
                titleObjectName: "applicationsCatalogTitle"
                title: I18n.tr("settings.apps.apps", "Apps")
                subtitle: I18n.tr("settings.apps.browse_and_manage_apps_on_this_device", "Browse and manage apps on this device.")

                Rectangle {
                    implicitHeight: 29
                    implicitWidth: appsCount.implicitWidth + 20
                    radius: height / 2
                    color: root.quietSurface
                    border { width: 1; color: root.quietBorder }
                    Text {
                        id: appsCount
                        objectName: "applicationsCountLabel"
                        anchors.centerIn: parent
                        text: root.filteredApps.length === root.apps.length
                            ? I18n.tr("settings.apps.count_all", "%1 apps").replace("%1", root.apps.length)
                            : I18n.tr("settings.apps.count_filtered", "%1 of %2")
                                .replace("%1", root.filteredApps.length).replace("%2", root.apps.length)
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("caption").family
                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                        font.weight: Components.Theme.typography.role("caption").weight
                    }
                }
            }

            Controls.SearchField {
                objectName: "applicationsSearchField"
                id: applicationsSearch
                Layout.fillWidth: true
                Layout.bottomMargin: Components.Theme.spacingLarge
                controlHeight: 46
                placeholderText: I18n.tr("settings.apps.search_applications", "Search applications")
                iconText: "⌕"
                text: root.searchText
                onTextEdited: value => {
                    root.searchText = value
                    appsList.positionViewAtBeginning()
                }
            }

            Components.BackendNotice {
                objectName: "applicationsBackendStatus"
                busy: root.controller.busy
                error: root.controller.lastError
                onRetry: root.controller.refresh()
            }

            Rectangle {
                id: installedCard
                objectName: "installedApplicationsCard"
                Layout.fillWidth: true
                Layout.bottomMargin: 18
                // The native virtualized ListView is the catalog's only scrollable area.
                // Whole rows only, so the viewport never cuts a row in half.
                readonly property real available: Math.max(104,
                    catalogPage.height - installedCard.y - 52)
                readonly property int fittingRows: Math.max(1, Math.floor((available - 12) / 78))
                Layout.preferredHeight: Math.max(104,
                    Math.min(root.filteredApps.length, fittingRows) * 78 + 12)
                radius: root.groupRadius
                color: Components.Theme.cardBg
                border.width: 1
                border.color: Components.Theme.cardBorder
                clip: true

                ListView {
                    id: appsList
                    objectName: "installedApplicationsList"
                    anchors.fill: parent
                    topMargin: 6
                    bottomMargin: 6
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.filteredApps
                    reuseItems: true
                    ScrollBar.vertical: ScrollBar {
                        objectName: "applicationsListScrollbar"
                        policy: ScrollBar.AsNeeded
                    }
                    delegate: ItemDelegate {
                        id: appRow
                        required property var modelData
                        required property int index
                        objectName: "applicationRow_" + modelData.id.replace(/[^A-Za-z0-9]/g, "_")
                        width: ListView.view.width
                        height: 78
                        padding: 0
                        hoverEnabled: true
                        activeFocusOnTab: true
                        Accessible.name: modelData.name
                        Accessible.description: modelData.description || modelData.category
                        onClicked: root.openApp(modelData)

                        background: Rectangle {
                            anchors { fill: parent; margins: 5; leftMargin: 7; rightMargin: 7 }
                            radius: root.insetRadius
                            color: appRow.pressed ? root.quietSurface
                                : (appRow.hovered || appRow.activeFocus ? root.hoverSurface : "transparent")
                            border.width: appRow.activeFocus ? 1 : 0
                            border.color: Components.Theme.accent
                            Behavior on color {
                                ColorAnimation {
                                    duration: Components.Theme.animationQuick
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        contentItem: Item {
                            RowLayout {
                                anchors { fill: parent; leftMargin: 19; rightMargin: 19 }
                                spacing: 13
                                Components.AppIcon {
                                    appData: appRow.modelData
                                    iconSize: 46
                                    iconRadius: 0
                                    fallbackColor: Qt.rgba(Components.Theme.accent.r,
                                                           Components.Theme.accent.g,
                                                           Components.Theme.accent.b, 0.12)
                                    fallbackTextColor: Components.Theme.textPrimary
                                    Layout.preferredWidth: 46
                                    Layout.preferredHeight: 46
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: appRow.modelData.name || appRow.modelData.id
                                        color: Components.Theme.textPrimary
                                        font.family: Components.Theme.typography.role("body").family
                                        font.pixelSize: Components.Theme.typography.role("body").pixelSize + 2
                                        lineHeight: Components.Theme.typography.role("body").lineHeight
                                        lineHeightMode: Text.ProportionalHeight
                                        font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                        font.weight: Components.Theme.typography.role("body").weight
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: appRow.modelData.description || appRow.modelData.id
                                        color: Components.Theme.textSecondary
                                        font.family: Components.Theme.typography.role("caption").family
                                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                                        lineHeightMode: Text.ProportionalHeight
                                        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                        elide: Text.ElideRight
                                    }
                                }
                                Text {
                                    objectName: "applicationRowChevron"
                                    text: "›"
                                    color: Components.Theme.textTertiary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: 23
                                    opacity: appRow.hovered ? 1 : 0.68
                                    Layout.alignment: Qt.AlignVCenter
                                    Behavior on opacity {
                                        NumberAnimation { duration: Components.Theme.animationQuick }
                                    }
                                }
                            }
                            Rectangle {
                                anchors { bottom: parent.bottom; left: parent.left; right: parent.right;
                                          leftMargin: 76; rightMargin: 18 }
                                height: 1
                                color: root.quietBorder
                                visible: appRow.index !== root.filteredApps.length - 1
                            }
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: !root.controller.busy && root.filteredApps.length === 0
                    text: root.searchText.trim() !== ""
                        ? I18n.tr("settings.apps.no_applications_match_your_search", "No applications match your search.")
                        : I18n.tr("settings.apps.no_installed_applications_found", "No installed applications found.")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.typography.role("caption").family
                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                }
            }
            Item { Layout.fillHeight: true }
        }
    }

    Form.ScrollPage {
        id: detailScroll
        objectName: "applicationsScrollPage"
        anchors.fill: parent
        visible: root.selectedAppId !== ""
        contentMargins: 32
        maxWidth: 900

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            spacing: 12
            Controls.ButtonCapsule {
                objectName: "closeApplicationDetailsButton"
                label: I18n.tr("settings.apps.back", "‹  Back")
                horizontalPadding: 16
                onClicked: root.closeDetails()
            }
            Text {
                text: I18n.tr("settings.apps.applications", "Applications")
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                color: Components.Theme.textTertiary
            }
            Item { Layout.fillWidth: true }
        }

        Components.BackendNotice {
            busy: root.controller.busy && !root.selectedApp
            error: root.controller.lastError
            onRetry: root.controller.select(root.selectedAppId)
        }

        // Retain the original 64px app-identity card, with calmer typography.
        Form.FormCard {
            objectName: "applicationDetailsCard"
            radius: root.groupRadius
            Layout.bottomMargin: 18
            Item {
                Layout.fillWidth: true
                implicitHeight: 132
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 19
                    Components.AppIcon {
                        objectName: "applicationDetailIcon"
                        appData: root.displayApp
                        iconSize: 76
                        iconRadius: 0
                        fallbackColor: Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g,
                                               Components.Theme.accent.b, 0.14)
                        fallbackTextColor: Components.Theme.textPrimary
                        Layout.preferredWidth: 76
                        Layout.preferredHeight: 76
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 4
                        Text {
                            objectName: "applicationDetailTitle"
                            Layout.fillWidth: true
                            text: root.displayApp ? root.displayApp.name : I18n.tr("settings.apps.loading_application", "Loading application…")
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.typography.role("display").pixelSize
                            font.weight: Components.Theme.fontWeightDemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.displayApp
                                ? (root.displayApp.description || root.displayApp.id) : ""
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.typography.role("caption").family
                            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                            lineHeight: Components.Theme.typography.role("caption").lineHeight
                            lineHeightMode: Text.ProportionalHeight
                            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // Two inset metadata pills and two compact technical rows, as in AstreaOS.
        Form.FormCard {
            objectName: "applicationInformationCard"
            radius: root.groupRadius
            Layout.bottomMargin: 18
            Item {
                Layout.fillWidth: true
                implicitHeight: infoContent.implicitHeight + 44
                ColumnLayout {
                    id: infoContent
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 22 }
                    spacing: 14

                    Text {
                        text: I18n.tr("settings.apps.information", "Information")
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("title").family
                        font.pixelSize: Components.Theme.typography.role("title").pixelSize
                        lineHeight: Components.Theme.typography.role("title").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("title").letterSpacing
                        font.weight: Components.Theme.typography.role("title").weight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: 72
                            radius: root.insetRadius
                            color: root.quietSurface
                            border { width: 1; color: root.quietBorder }
                            ColumnLayout {
                                anchors { fill: parent; margins: 12 }
                                spacing: 4
                                Text {
                                    text: I18n.tr("settings.apps.type", "Type")
                                    color: Components.Theme.textSecondary
                                    font.family: Components.Theme.typography.role("caption").family
                                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.selectedApp ? root.selectedApp.kind : I18n.tr("settings.apps.loading", "Loading…")
                                    color: Components.Theme.textPrimary
                                    font.family: Components.Theme.typography.role("body").family
                                    font.pixelSize: Components.Theme.typography.role("body").pixelSize
                                    lineHeight: Components.Theme.typography.role("body").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                    font.weight: Components.Theme.typography.role("body").weight
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: 72
                            radius: root.insetRadius
                            color: root.quietSurface
                            border { width: 1; color: root.quietBorder }
                            ColumnLayout {
                                anchors { fill: parent; margins: 12 }
                                spacing: 4
                                Text {
                                    text: I18n.tr("settings.apps.installed_size", "Installed size")
                                    color: Components.Theme.textSecondary
                                    font.family: Components.Theme.typography.role("caption").family
                                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.selectedApp ? root.selectedApp.size : I18n.tr("settings.apps.checking", "Checking…")
                                    color: Components.Theme.textPrimary
                                    font.family: Components.Theme.typography.role("body").family
                                    font.pixelSize: Components.Theme.typography.role("body").pixelSize
                                    lineHeight: Components.Theme.typography.role("body").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                    font.weight: Components.Theme.typography.role("body").weight
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Item {
                        objectName: "applicationIdRow"
                        Layout.fillWidth: true
                        implicitHeight: 30
                        RowLayout {
                            anchors.fill: parent
                            spacing: 10
                            Text {
                                text: "ID"
                                color: Components.Theme.textSecondary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                Layout.preferredWidth: 60
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.displayApp ? root.displayApp.id : ""
                                color: Components.Theme.textPrimary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                elide: Text.ElideMiddle
                                textFormat: Text.PlainText
                            }
                        }
                    }
                    Item {
                        objectName: "applicationFileRow"
                        Layout.fillWidth: true
                        implicitHeight: 30
                        RowLayout {
                            anchors.fill: parent
                            spacing: 10
                            Text {
                                text: I18n.tr("settings.apps.file", "File")
                                color: Components.Theme.textSecondary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                Layout.preferredWidth: 60
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.selectedApp ? root.selectedApp.file : ""
                                color: Components.Theme.textPrimary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                elide: Text.ElideMiddle
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 18
            radius: root.groupRadius
            Item {
                Layout.fillWidth: true
                implicitHeight: privacyContent.implicitHeight + 44
                ColumnLayout {
                    id: privacyContent
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 22 }
                    spacing: 14
                    Text {
                        text: I18n.tr("settings.apps.privacy_access", "Privacy & access")
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("title").family
                        font.pixelSize: Components.Theme.typography.role("title").pixelSize
                        lineHeight: Components.Theme.typography.role("title").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("title").letterSpacing
                        font.weight: Components.Theme.typography.role("title").weight
                    }
                    Repeater {
                        model: root.selectedApp ? (root.selectedApp.privacyItems || []) : []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: privacyRow.implicitHeight + 24
                            radius: root.insetRadius
                            color: root.quietSurface
                            border { width: 1; color: root.quietBorder }
                            ColumnLayout {
                                id: privacyRow
                                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Components.Theme.textPrimary
                                    font.family: Components.Theme.typography.role("body").family
                                    font.pixelSize: Components.Theme.typography.role("body").pixelSize
                                    lineHeight: Components.Theme.typography.role("body").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                    font.weight: Components.Theme.typography.role("body").weight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    textFormat: Text.PlainText
                                    color: Components.Theme.textSecondary
                                    font.family: Components.Theme.typography.role("caption").family
                                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                    Text {
                        id: privacyNote
                        Layout.fillWidth: true
                        text: root.selectedApp ? root.selectedApp.privacyNote
                            : I18n.tr("settings.apps.checking_sandbox_permissions", "Checking sandbox permissions…")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("caption").family
                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 28
            radius: root.groupRadius
            Item {
                Layout.fillWidth: true
                implicitHeight: actionsContent.implicitHeight + 44
                ColumnLayout {
                    id: actionsContent
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 22 }
                    spacing: 15
                    Text {
                        text: I18n.tr("settings.apps.actions", "Actions")
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("title").family
                        font.pixelSize: Components.Theme.typography.role("title").pixelSize
                        lineHeight: Components.Theme.typography.role("title").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("title").letterSpacing
                        font.weight: Components.Theme.typography.role("title").weight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Controls.Button {
                            objectName: "createApplicationShortcutButton"
                            label: I18n.tr("settings.apps.create_shortcut", "Create shortcut")
                            controlHeight: 36
                            enabled: root.selectedApp !== null && !root.controller.busy
                            onClicked: root.controller.createShortcut(root.selectedAppId)
                        }
                        Controls.Button {
                            objectName: "openApplicationLocationButton"
                            label: I18n.tr("settings.apps.open_location", "Open location")
                            controlHeight: 36
                            enabled: root.selectedApp !== null && !root.controller.busy
                            onClicked: root.controller.openLocation(root.selectedAppId)
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: root.quietBorder
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 4
                            Text {
                                Layout.fillWidth: true
                                text: root.selectedApp && root.selectedApp.method === "launcher"
                                    ? I18n.tr("settings.apps.remove_launcher", "Remove launcher") : I18n.tr("settings.apps.uninstall_application", "Uninstall application")
                                color: Components.Theme.textPrimary
                                font.family: Components.Theme.typography.role("body").family
                                font.pixelSize: Components.Theme.typography.role("body").pixelSize
                                lineHeight: Components.Theme.typography.role("body").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                font.weight: Components.Theme.typography.role("body").weight
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.selectedApp && root.selectedApp.method === "launcher"
                                    ? I18n.tr("settings.apps.only_the_shortcut_entry_will_be_removed", "Only the shortcut entry will be removed.")
                                    : I18n.tr("settings.apps.application_files_may_be_removed_your_data_i", "Application files may be removed. Your data is kept.")
                                color: Components.Theme.textSecondary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                wrapMode: Text.Wrap
                            }
                        }
                        Controls.Button {
                            objectName: "uninstallApplicationButton"
                            label: root.selectedApp ? root.selectedApp.actionLabel : I18n.tr("settings.apps.uninstall", "Uninstall")
                            danger: true
                            controlHeight: 36
                            foregroundColor: Components.Theme.errorColor
                            color: enabled
                                ? Qt.rgba(Components.Theme.errorColor.r, Components.Theme.errorColor.g,
                                          Components.Theme.errorColor.b, 0.09)
                                : root.quietSurface
                            border.color: Qt.rgba(Components.Theme.errorColor.r,
                                                  Components.Theme.errorColor.g,
                                                  Components.Theme.errorColor.b, 0.20)
                            enabled: root.selectedApp !== null
                                && !!root.selectedApp.canUninstall && !root.controller.busy
                            onClicked: removalDialog.open()
                        }
                    }
                }
            }
        }
    }
}
