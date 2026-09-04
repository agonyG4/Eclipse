import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/form" as Form
import "../../components/navigation" as Navigation

Item {
    id: root
    objectName: "settingsHubPage"

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
        if (iconKey !== "")
            return SettingsController.iconUrl(iconKey, Theme.iconTheme).toString()

        const iconSource = item && item.iconSource !== undefined ? item.iconSource : ""
        if (iconSource !== "")
            return iconSource

        return ""
    }

    readonly property string heroIconSource: root.iconSourceFor(root.destination)

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 760

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingLarge
            margins: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 228

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 520)
                    spacing: Components.Theme.spacingSmall

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 72
                        height: 72
                        radius: Components.Theme.radiusLarge
                        color: Qt.rgba(1, 1, 1, Components.Theme.isLight ? 0.28 : 0.08)
                        border.width: 1
                        border.color: Components.Theme.cardBorder

                        Image {
                            anchors.fill: parent
                            anchors.margins: 16
                            source: root.heroIconSource
                            sourceSize: Qt.size(96, 96)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: source !== ""
                        }

                        Text {
                            anchors.centerIn: parent
                            text: root.destination && root.destination.sym ? root.destination.sym : ""
                            visible: root.heroIconSource === ""
                            color: Components.Theme.accent
                            font.family: Components.Theme.monoFontFamily
                            font.pixelSize: Components.Theme.fontSizeIconLarge
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.translatedLabel(root.destination)
                        horizontalAlignment: Text.AlignHCenter
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeHeader
                        font.weight: Components.Theme.fontWeightDemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.translatedSubtitle(root.destination)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSubtitle
                        opacity: Components.Theme.opacitySecondary
                    }
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
                        ? SettingsController.iconUrl(descriptor.iconKey, Theme.iconTheme)
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
