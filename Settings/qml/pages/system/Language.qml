import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "languagePage"

    property int selectedLanguage: 0
    property int selectedCountry: 0
    property int selectedTimeFormat: 0
    property bool automaticLocation: false
    property string countrySearch: ""

    readonly property var languages: ["English (United States)", "English (United Kingdom)", "Português (Brasil)", "Español", "Français"]
    readonly property var countries: ["United States", "United Kingdom", "Brazil", "Canada", "France", "Germany", "Japan"]
    readonly property var filteredCountries: root.countries.filter(country => root.countrySearch.trim() === "" || country.toLowerCase().indexOf(root.countrySearch.toLowerCase()) !== -1)
    readonly property var timeFormats: ["System default (24-hour)", "24-hour", "12-hour (AM/PM)"]

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "languagePreviewNotice"
            Layout.fillWidth: true
            Layout.bottomMargin: 22
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

        Form.SectionHeader { text: "LANGUAGE & REGION"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "languageRegionCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "languageSelectRow"
                label: "Language"
                sublabel: "Choose the language used by apps and the shell."
                Controls.SelectButton {
                    objectName: "languageSelect"
                    width: 190
                    options: root.languages
                    selectedIndex: root.selectedLanguage
                    label: root.languages[root.selectedLanguage]
                    onSelected: index => root.selectedLanguage = index
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
                    options: root.filteredCountries
                    selectedIndex: root.filteredCountries.indexOf(root.countries[root.selectedCountry])
                    label: root.filteredCountries.length > 0 ? root.countries[root.selectedCountry] : "No matches"
                    onSelected: index => {
                        const country = root.filteredCountries[index]
                        root.selectedCountry = root.countries.indexOf(country)
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
                    onSelected: index => root.selectedTimeFormat = index
                }
            }
            Form.SettingRow {
                objectName: "automaticLocationRow"
                label: "Automatic location"
                sublabel: root.automaticLocation
                    ? "Location services would choose the region automatically."
                    : "Use the country or region selected above."
                Controls.ToggleSwitch {
                    accessibleName: "Automatic location"
                    checked: root.automaticLocation
                    onToggled: checked => root.automaticLocation = checked
                }
                isLast: true
            }
        }
    }
}
