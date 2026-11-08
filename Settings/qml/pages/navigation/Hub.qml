import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/form" as Form
import "../../components/controls" as Controls
import "../../components/navigation" as Navigation

Item {
    id: root
    objectName: "settingsHubPage"

    FontLoader { id: tablerIcons; source: "qrc:/Astrea/Settings/assets/fonts/tabler-icons-300.ttf" }

    readonly property var destination: SettingsController.currentDestination
    readonly property var destinations: SettingsController.currentDestinationChildren

    function translatedLabel(item) {
        const key = item && item.labelKey !== undefined ? item.labelKey : ""
        if (key.length > 0)
            return I18n.tr(key, item && item.label ? item.label : "")
        return item && item.label ? item.label : ""
    }

    function translatedSubtitle(item) {
        const key = item && item.subtitleKey !== undefined ? item.subtitleKey : ""
        if (key.length > 0)
            return I18n.tr(key, item && item.subtitle ? item.subtitle : "")
        return item && item.subtitle ? item.subtitle : ""
    }

    function iconSourceFor(item) {
        const iconKey = item && item.iconKey !== undefined ? item.iconKey : ""
        if (iconKey !== "") {
            const themed = SettingsController.iconUrlForWeight(
                iconKey, SettingsController.navigationIconTheme,
                SettingsController.navigationIconWeight).toString()
            if (themed !== "")
                return themed
        }

        const iconSource = item && item.iconSource !== undefined ? item.iconSource : ""
        if (iconSource !== "")
            return iconSource

        return ""
    }

    readonly property string heroIconSource: root.iconSourceFor(root.destination)

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: root.translatedLabel(root.destination)
            subtitle: root.translatedSubtitle(root.destination)

            leading: Rectangle {
                implicitWidth: 56
                implicitHeight: 56
                radius: Components.Theme.radiusLarge
                color: Qt.rgba(1, 1, 1, Components.Theme.isLight ? 0.28 : 0.08)
                border.width: 1
                border.color: Components.Theme.cardBorder

                Controls.ThemedIcon {
                    anchors.fill: parent
                    anchors.margins: 14
                    iconSource: root.heroIconSource
                    tint: Components.Theme.textPrimary
                    visible: root.heroIconSource !== ""
                }

                Text {
                    anchors.centerIn: parent
                    text: root.destination && root.destination.sym ? root.destination.sym : ""
                    visible: root.heroIconSource === ""
                    color: Components.Theme.accent
                    font.family: tablerIcons.name
                    font.pixelSize: 32
                }
            }
        }

        Form.FormCard {
            Layout.fillWidth: true
            spacing: 0
            margins: 0

            Repeater {
                model: root.destinations
                delegate: Navigation.HubNavigationRow {
                    required property var modelData
                    required property int index
                    property var descriptor: modelData

                    destinationId: descriptor.entryId
                    label: root.translatedLabel(descriptor)
                    sublabel: root.translatedSubtitle(descriptor)
                    iconSource: descriptor.iconKey
                        ? SettingsController.iconUrlForWeight(descriptor.iconKey,
                            SettingsController.navigationIconTheme,
                            SettingsController.navigationIconWeight)
                        : descriptor.iconSource || ""
                    sym: descriptor.sym || ""
                    enabled: descriptor.entryEnabled === true
                    isLast: index === root.destinations.length - 1
                    onClicked: SettingsController.navigateTo(descriptor.entryId)
                }
            }
        }
    }
}
