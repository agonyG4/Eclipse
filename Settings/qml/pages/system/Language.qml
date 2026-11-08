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
    readonly property var timeFormats: [I18n.tr("settings.language.system_default_format", "System default (%1)").replace("%1", snapshot.effectiveTimeFormat || "24h"), I18n.tr("settings.language.24_hour", "24-hour"), I18n.tr("settings.language.12_hour_am_pm", "12-hour (AM/PM)")]

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
                label: I18n.tr("settings.language.language", "Language")
                sublabel: I18n.tr("settings.language.sets_the_preferred_locale_reopen_apps_to_app", "Sets the preferred locale. Reopen apps to apply translations; unavailable translations use English.")
                Controls.SelectButton {
                    objectName: "languageSelect"
                    width: 190
                    options: root.languages
                    selectedIndex: root.selectedLanguage
                    label: root.languages[root.selectedLanguage] || I18n.tr("settings.language.loading", "Loading…")
                    onSelected: index => root.controller.setOption("language", root.languageEntries[index].id)
                }
            }
            Form.SettingRow {
                objectName: "countrySearchRow"
                label: I18n.tr("settings.language.search_location", "Search location")
                sublabel: I18n.tr("settings.language.filter_the_country_and_region_choices_below", "Filter the country and region choices below.")
                Controls.SearchField {
                    objectName: "countrySearchField"
                    width: 190
                    controlHeight: 36
                    placeholderText: I18n.tr("settings.language.country_or_region", "Country or region")
                    text: root.countrySearch
                    onTextEdited: value => root.countrySearch = value
                }
            }
            Form.SettingRow {
                objectName: "countrySelectRow"
                label: I18n.tr("settings.language.country_or_region", "Country or region")
                sublabel: root.filteredCountries.length > 0 ? I18n.tr("settings.language.sets_local_conventions_for_dates_and_numbers", "Sets local conventions for dates and numbers.") : I18n.tr("settings.language.no_matching_countries_clear_the_search_to_se", "No matching countries. Clear the search to see all choices.")
                Controls.SelectButton {
                    objectName: "countrySelect"
                    width: 190
                    enabled: root.filteredCountries.length > 0
                    options: root.filteredCountries.map(item => item.name)
                    selectedIndex: root.filteredCountries.map(item => item.id).indexOf(root.region.country_code || "BR")
                    label: root.filteredCountries.length > 0 ? (root.countries[root.selectedCountry] || I18n.tr("settings.language.select_location", "Select location")) : I18n.tr("settings.language.no_matches", "No matches")
                    onSelected: index => {
                        root.controller.setOption("country_code", root.filteredCountries[index].id)
                    }
                }
            }
            Form.SettingRow {
                objectName: "timeFormatRow"
                label: I18n.tr("settings.language.time_format", "Time format")
                sublabel: I18n.tr("settings.language.controls_24_hour_or_am_pm_time_in_apps_and_t", "Controls 24-hour or AM/PM time in apps and the clock.")
                Controls.SelectButton {
                    objectName: "timeFormatSelect"
                    width: 190
                    options: root.timeFormats
                    selectedIndex: root.selectedTimeFormat
                    label: root.timeFormats[root.selectedTimeFormat]
                    onSelected: index => root.controller.setOption("time_format", root.timeFormatIds[index])
                }
            }
            Form.SettingRow {
                objectName: "automaticLocationRow"
                label: I18n.tr("settings.language.automatic_location", "Automatic location")
                sublabel: root.automaticLocation
                    ? I18n.tr("settings.language.allow_system_geolocation_the_selected_region", "Allow system geolocation. The selected region still controls regional formatting.")
                    : I18n.tr("settings.language.system_geolocation_is_blocked_use_a_manually", "System geolocation is blocked; use a manually selected location.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.language.automatic_location", "Automatic location")
                    enabled: !!root.snapshot.locationAvailable
                    checked: root.automaticLocation
                    onToggled: checked => root.controller.setOption("automatic_location", checked)
                }
                isLast: true
            }
        }
    }
}
