import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "languagePage"

    readonly property var controller: SettingsController.region
    readonly property var snapshot: controller.state
    readonly property var config: snapshot.config || ({})
    readonly property var region: config.region || ({})
    readonly property var languageEntries: snapshot.languages || []
    readonly property var countryEntries: snapshot.countries || []
    readonly property var languages: languageEntries.map(item => item.name)
    readonly property var countries: countryEntries.map(item => item.name)
    readonly property int selectedLanguage: languageEntries.map(item => item.id).indexOf(config.language || "en_US")
    readonly property int selectedCountry: countryEntries.map(item => item.id).indexOf(region.country_code || "BR")
    readonly property int selectedTimeFormat: timeFormatIds.indexOf(region.time_format || "system")
    readonly property bool automaticLocation: !!region.automatic_location
    property string countrySearch: ""
    readonly property var filteredCountries: countryEntries.filter(item => root.countrySearch.trim() === "" || (item.name + " " + item.id).toLowerCase().indexOf(root.countrySearch.toLowerCase()) !== -1)
    readonly property var timeFormatIds: ["system", "24h", "12h"]
    readonly property var timeFormats: ["System default (" + (snapshot.effectiveTimeFormat || "24h") + ")", "24-hour", "12-hour (AM/PM)"]

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        anchors.fill: parent
        enabled: !root.controller.busy
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.pages.language.title", "Language & Region")
            subtitle: I18n.tr("settings.pages.language.subtitle", "Language, regional formats and time.")
        }

        Components.BackendNotice {
            objectName: "languageBackendStatus"
            busy: root.controller.busy
            error: root.controller.lastError
            onRetry: root.controller.refresh()
        }

        Form.FormCard {
            objectName: "languageRegionCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "languageSelectRow"
                label: "Language"
                sublabel: "Sets the preferred locale. Reopen apps to apply translations; unavailable translations use English."
                Controls.SelectButton {
                    objectName: "languageSelect"
                    width: 190
                    options: root.languages
                    selectedIndex: root.selectedLanguage
                    label: root.languages[root.selectedLanguage] || "Loading…"
                    onSelected: index => root.controller.setOption("language", root.languageEntries[index].id)
                }
            }
            Form.SettingRow {
                objectName: "countrySearchRow"
                label: "Search location"
                sublabel: "Filter the country and region choices below."
                Controls.SearchField {
                    objectName: "countrySearchField"
                    width: 186
                    controlHeight: 36
                    placeholderText: "Country or region"
                    text: root.countrySearch
                    onTextEdited: value => root.countrySearch = value
                }
            }
            Form.SettingRow {
                objectName: "countrySelectRow"
                label: "Country or region"
                sublabel: root.filteredCountries.length > 0 ? "Sets local conventions for dates and numbers." : "No matching countries. Clear the search to see all choices."
                Controls.SelectButton {
                    objectName: "countrySelect"
                    width: 164
                    enabled: root.filteredCountries.length > 0
                    options: root.filteredCountries.map(item => item.name)
                    selectedIndex: root.filteredCountries.map(item => item.id).indexOf(root.region.country_code || "BR")
                    label: root.filteredCountries.length > 0 ? (root.countries[root.selectedCountry] || "Select location") : "No matches"
                    onSelected: index => {
                        root.controller.setOption("country_code", root.filteredCountries[index].id)
                    }
                }
            }
            Form.SettingRow {
                objectName: "timeFormatRow"
                label: "Time format"
                sublabel: "Controls 24-hour or AM/PM time in apps and the clock."
                Controls.SelectButton {
                    objectName: "timeFormatSelect"
                    width: 164
                    options: root.timeFormats
                    selectedIndex: root.selectedTimeFormat
                    label: root.timeFormats[root.selectedTimeFormat]
                    onSelected: index => root.controller.setOption("time_format", root.timeFormatIds[index])
                }
            }
            Form.SettingRow {
                objectName: "automaticLocationRow"
                label: "Automatic location"
                sublabel: root.automaticLocation
                    ? "Allow system geolocation. The selected region still controls regional formatting."
                    : "System geolocation is blocked; use a manually selected location."
                Controls.ToggleSwitch {
                    accessibleName: "Automatic location"
                    enabled: !!root.snapshot.locationAvailable
                    checked: root.automaticLocation
                    onToggled: checked => root.controller.setOption("automatic_location", checked)
                }
                isLast: true
            }
        }
    }
}
