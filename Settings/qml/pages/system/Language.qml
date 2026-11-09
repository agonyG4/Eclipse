import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form
import "../../components/typography" as Typography

Item {
    id: root
    objectName: "languagePage"

    readonly property var controller: SettingsController.region
    readonly property var snapshot: controller.state
    readonly property var config: snapshot.config || ({})
    readonly property var region: config.region || ({})
    readonly property var languageEntries: snapshot.languages || []
    readonly property var countryEntries: snapshot.countries || []
    readonly property var zoneEntries: snapshot.timeZones || []
    readonly property var languages: languageEntries.map(item => item.name)
    readonly property int selectedLanguage: languageEntries.map(item => item.id).indexOf(config.language || "en_US")
    readonly property string countryCode: region.country_code || "BR"
    readonly property var currentCountry: countryEntries.find(item => item.id === countryCode) || null
    readonly property string zoneId: snapshot.currentTimeZone || ""
    readonly property var currentZone: zoneEntries.find(item => item.id === zoneId) || null
    readonly property int selectedTimeFormat: timeFormatIds.indexOf(region.time_format || "system")
    readonly property bool automaticLocation: !!region.automatic_location
    readonly property bool locationAvailable: !!snapshot.locationAvailable
    readonly property var timeFormatIds: ["system", "24h", "12h"]
    readonly property var timeFormats: [I18n.tr("settings.language.system_default_format", "System default (%1)").replace("%1", snapshot.effectiveTimeFormat || "24h"), I18n.tr("settings.language.24_hour", "24-hour"), I18n.tr("settings.language.12_hour_am_pm", "12-hour (AM/PM)")]

    property string countrySearch: ""
    property string zoneSearch: ""
    readonly property var filteredCountries: { snapshot; return controller.filterCountries(countrySearch) }
    readonly property var filteredZones: {
        snapshot
        const zones = controller.filterTimeZones(zoneSearch)
        // Empty search groups the selected country's zones above the rest.
        if (zoneSearch.trim() === "" && zones.length > 0) {
            const hasLocal = !!zones[0].inCountry
            for (let i = 0; i < zones.length; ++i) {
                if (i === 0 && hasLocal)
                    zones[i].heading = I18n.tr("settings.language.zones_in_this_country", "In this country")
                else if (hasLocal && !zones[i].inCountry && zones[i - 1].inCountry)
                    zones[i].heading = I18n.tr("settings.language.all_time_zones", "All time zones")
            }
        }
        return zones
    }

    readonly property var detection: controller.detection
    readonly property string detectionStatus: detection.status || "idle"
    readonly property bool detecting: detectionStatus === "locating"
    readonly property bool detectionFound: detectionStatus === "found"
    readonly property bool ipFallbackOffered: detection.source === "system" && ["unavailable", "denied", "timeout", "failed", "blocked"].indexOf(detectionStatus) !== -1

    function detectionMessage() {
        if (detectionFound) {
            const zone = detection.zoneName || detection.zone
            const offset = detection.zoneOffset ? " (" + detection.zoneOffset + ")" : ""
            return I18n.tr("settings.language.detected_summary", "Looks like %1, %2. This is an estimate from the nearest reference city, so check it before applying.")
                .replace("%1", detection.countryName)
                .replace("%2", zone + offset)
        }
        const ipOffer = I18n.tr("settings.language.ip_offer", " You can estimate from your IP address instead, which sends one request to ipapi.co and reveals your IP to it.")
        switch (detectionStatus) {
        case "unavailable":
            return I18n.tr("settings.language.detect_unavailable", "The system location service (GeoClue) isn't available. Install the geoclue package to use it.") + ipOffer
        case "denied":
            return I18n.tr("settings.language.detect_denied", "Location access was denied.") + ipOffer
        case "blocked":
            return I18n.tr("settings.language.detect_blocked", "System geolocation is blocked by the setting below.") + ipOffer
        case "timeout":
            return I18n.tr("settings.language.detect_timeout", "The location service didn't answer in time.") + ipOffer
        case "failed":
            return detection.source === "ip"
                ? I18n.tr("settings.language.detect_ip_invalid", "ipapi.co didn't return a usable location. Choose your country below.")
                : I18n.tr("settings.language.detect_failed", "The location service couldn't find a location.") + ipOffer
        case "network":
            return I18n.tr("settings.language.detect_network", "Couldn't reach ipapi.co. Check your connection or choose your country below.")
        case "refused":
            return I18n.tr("settings.language.detect_refused", "ipapi.co refused the request, probably a rate limit. Try later or choose your country below.")
        }
        return ""
    }
    function zoneChangeError() {
        if (snapshot.timeZoneChange === "denied")
            return I18n.tr("settings.language.zone_denied", "The time zone wasn't changed: authorization was denied.")
        if (snapshot.timeZoneChange === "failed")
            return I18n.tr("settings.language.zone_failed", "The time zone couldn't be changed by the system.")
        return ""
    }

    property date now: new Date()
    readonly property string effectiveFormat: region.time_format && region.time_format !== "system" ? region.time_format : (snapshot.effectiveTimeFormat || "24h")

    onZoneIdChanged: {
        Date.timeZoneUpdated()
        now = new Date()
    }
    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        onTriggered: root.now = new Date()
    }

    // Saving a preference takes milliseconds: only a wait the user can notice, such
    // as the password prompt, may dim the page or announce itself.
    property bool slowOperation: false
    Timer {
        interval: 400
        running: root.controller.busy
        onTriggered: root.slowOperation = true
    }
    Connections {
        target: root.controller
        function onBusyChanged() {
            if (!root.controller.busy)
                root.slowOperation = false
        }
    }

    Component.onCompleted: root.controller.refresh()

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.bottomMargin: Components.Theme.spacingSmall
        Layout.leftMargin: Components.Theme.spacingXLarge
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("label").family
        font.pixelSize: Components.Theme.typography.role("label").pixelSize
        font.weight: Components.Theme.fontWeightDemiBold
    }
    component Footnote: Text {
        Layout.fillWidth: true
        Layout.leftMargin: Components.Theme.spacingXLarge
        Layout.rightMargin: Components.Theme.spacingXLarge
        wrapMode: Text.Wrap
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
    }

    Form.ScrollPage {
        anchors.fill: parent
        enabled: !root.slowOperation
        contentMargins: 32
        maxWidth: 760

        Form.PageHeader {
            title: I18n.tr("settings.pages.language.title", "Language & Region")
            subtitle: I18n.tr("settings.pages.language.subtitle", "Language, regional formats and time.")
        }

        Components.BackendNotice {
            objectName: "languageBackendStatus"
            busy: root.slowOperation
            error: root.controller.lastError || root.zoneChangeError()
            showAction: !busy && root.controller.lastError.length > 0
            onRetry: root.controller.refresh()
        }

        Form.FormCard {
            objectName: "locationSummaryCard"

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingXLarge
                spacing: Components.Theme.spacingLarge

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Components.Theme.spacingMicro

                    Typography.DisplayLabel {
                        objectName: "locationSummaryClock"
                        typographyRole: "display"
                        Layout.fillWidth: true
                        font.weight: Components.Theme.fontWeightLight
                        font.features: { "tnum": 1 }
                        text: Qt.formatTime(root.now, root.effectiveFormat === "12h" ? "h:mm AP" : "HH:mm")
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.now.toLocaleDateString(Qt.locale(root.config.language || "en_US"), Locale.LongFormat)
                        elide: Text.ElideRight
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("label").family
                        font.pixelSize: Components.Theme.typography.role("label").pixelSize
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Components.Theme.spacingTiny
                        spacing: Components.Theme.spacingSmall
                        Text {
                            objectName: "locationSummaryFlag"
                            visible: text !== ""
                            text: root.currentCountry ? root.currentCountry.flag : ""
                            font.pixelSize: Components.Theme.typography.role("label").pixelSize + 1
                        }
                        Text {
                            objectName: "locationSummaryDetail"
                            Layout.fillWidth: true
                            text: {
                                const parts = []
                                if (root.currentCountry)
                                    parts.push(root.currentCountry.name)
                                if (root.currentZone)
                                    parts.push(root.currentZone.name, root.currentZone.trailing)
                                return parts.join(" · ")
                            }
                            elide: Text.ElideRight
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.typography.role("caption").family
                            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        }
                    }
                }

                Controls.ButtonCapsule {
                    objectName: "detectLocationButton"
                    Layout.alignment: Qt.AlignVCenter
                    controlHeight: 30
                    enabled: !root.detecting
                    iconText: "\uf124"
                    iconSize: 12
                    label: I18n.tr("settings.language.detect_location", "Detect location")
                    onClicked: root.controller.detectLocation()
                }
            }

            // The answer unfolds inside the card it belongs to.
            Item {
                objectName: "detectionNotice"
                readonly property string message: root.detecting ? "" : root.detectionMessage()
                visible: root.detectionStatus !== "idle" || Layout.preferredHeight > 0
                Layout.fillWidth: true
                Layout.preferredHeight: root.detectionStatus !== "idle" ? detectionRow.implicitHeight + Components.Theme.spacingLarge * 2 : 0
                clip: true
                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: Components.Theme.animationNormal; easing.type: Components.Theme.easingStandard }
                }

                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Components.Theme.spacingXLarge }
                    height: 1
                    color: Components.Theme.cardBorder
                }

                RowLayout {
                    id: detectionRow
                    anchors {
                        left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                        leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge
                    }
                    spacing: Components.Theme.spacingMedium

                    BusyIndicator {
                        visible: root.detecting
                        running: root.detecting
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.detecting
                            ? (root.detection.source === "ip"
                                ? I18n.tr("settings.language.detecting_ip", "Asking ipapi.co for an estimate from your IP address…")
                                : I18n.tr("settings.language.detecting_system", "Asking the system location service…"))
                            : parent.parent.message
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("caption").family
                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                    }
                    Controls.ButtonCapsule {
                        objectName: "detectionDismiss"
                        Layout.alignment: Qt.AlignVCenter
                        visible: !root.detecting
                        controlHeight: 28
                        horizontalPadding: Components.Theme.spacingMedium
                        label: I18n.tr("settings.language.dismiss", "Dismiss")
                        onClicked: root.controller.dismissDetection()
                    }
                    Controls.ButtonCapsule {
                        objectName: "detectionAction"
                        Layout.alignment: Qt.AlignVCenter
                        visible: root.detecting || root.detectionFound || root.ipFallbackOffered
                        controlHeight: 28
                        horizontalPadding: Components.Theme.spacingMedium
                        primary: root.detectionFound
                        label: root.detecting ? I18n.tr("settings.language.cancel", "Cancel")
                            : root.detectionFound ? I18n.tr("settings.language.apply", "Apply")
                            : I18n.tr("settings.language.use_ip_address", "Use IP address")
                        onClicked: {
                            if (root.detecting)
                                root.controller.cancelDetection()
                            else if (root.detectionFound)
                                root.controller.applyDetection()
                            else
                                root.controller.detectLocationByIp()
                        }
                    }
                }
            }
        }

        Footnote {
            Layout.topMargin: Components.Theme.spacingSmall
            Layout.bottomMargin: 28
            text: I18n.tr("settings.language.detect_privacy", "Only runs when you ask. GeoClue may consult a network location service; the coordinates aren't saved and nothing changes until you apply the result.")
        }

        SectionTitle {
            text: I18n.tr("settings.language.section_location", "Location")
        }

        Form.FormCard {
            objectName: "locationCard"
            Layout.bottomMargin: 28
            Form.SettingRow {
                objectName: "countrySelectRow"
                label: I18n.tr("settings.language.country_or_region", "Country or region")
                sublabel: I18n.tr("settings.language.sets_local_conventions_for_dates_and_numbers", "Sets local conventions for dates and numbers.")
                Controls.SearchableSelect {
                    objectName: "countrySelect"
                    label: root.currentCountry ? root.currentCountry.name : I18n.tr("settings.language.select_location", "Select location")
                    leadingText: root.currentCountry ? root.currentCountry.flag : ""
                    accessibleName: I18n.tr("settings.language.country_or_region", "Country or region")
                    currentId: root.countryCode
                    options: root.filteredCountries
                    searchText: root.countrySearch
                    searchPlaceholder: I18n.tr("settings.language.search_country_placeholder", "Search by name or code")
                    emptyText: I18n.tr("settings.language.no_matching_country", "No country matches your search.")
                    onSearchEdited: text => root.countrySearch = text
                    onClosed: root.countrySearch = ""
                    onSelected: id => root.controller.setOption("country_code", id)
                }
            }
            Form.SettingRow {
                objectName: "timeZoneSelectRow"
                label: I18n.tr("settings.language.time_zone", "Time zone")
                sublabel: I18n.tr("settings.language.time_zone_description", "Sets the system clock. Changing it may ask for your password.")
                Controls.SearchableSelect {
                    objectName: "timeZoneSelect"
                    label: root.currentZone ? root.currentZone.name + " · " + root.currentZone.trailing
                        : (root.zoneId || I18n.tr("settings.language.select_time_zone", "Select time zone"))
                    accessibleName: I18n.tr("settings.language.time_zone", "Time zone")
                    currentId: root.zoneId
                    options: root.filteredZones
                    popupWidth: 440
                    searchText: root.zoneSearch
                    searchPlaceholder: I18n.tr("settings.language.search_zone_placeholder", "Search by city, country or UTC offset")
                    emptyText: I18n.tr("settings.language.no_matching_zone", "No time zone matches your search.")
                    onSearchEdited: text => root.zoneSearch = text
                    onClosed: root.zoneSearch = ""
                    onSelected: id => {
                        if (id !== root.zoneId)
                            root.controller.setTimeZone(id)
                    }
                }
            }
            Form.SettingRow {
                objectName: "automaticLocationRow"
                label: I18n.tr("settings.language.automatic_location", "Automatic location")
                sublabel: !root.locationAvailable
                    ? I18n.tr("settings.language.geoclue_not_installed", "GeoClue isn't installed, so there is no system geolocation to allow or block.")
                    : root.automaticLocation
                        ? I18n.tr("settings.language.allow_system_geolocation_the_selected_region", "Allow system geolocation. The selected region still controls regional formatting.")
                        : I18n.tr("settings.language.system_geolocation_is_blocked_use_a_manually", "System geolocation is blocked; use a manually selected location.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.language.automatic_location", "Automatic location")
                    enabled: root.locationAvailable
                    checked: root.locationAvailable && root.automaticLocation
                    onToggled: checked => root.controller.setOption("automatic_location", checked)
                }
                isLast: true
            }
        }

        SectionTitle {
            text: I18n.tr("settings.language.section_language_formats", "Language & Formats")
        }

        Form.FormCard {
            objectName: "languageRegionCard"
            Form.SettingRow {
                objectName: "languageSelectRow"
                label: I18n.tr("settings.language.language", "Language")
                sublabel: I18n.tr("settings.language.sets_the_preferred_locale_reopen_apps_to_app", "Sets the preferred locale. Reopen apps to apply translations; unavailable translations use English.")
                Controls.SearchableSelect {
                    objectName: "languageSelect"
                    searchable: false
                    label: root.languages[root.selectedLanguage] || I18n.tr("settings.language.loading", "Loading…")
                    accessibleName: I18n.tr("settings.language.language", "Language")
                    currentId: root.config.language || ""
                    options: root.languageEntries
                    onSelected: id => root.controller.setOption("language", id)
                }
            }
            Form.SettingRow {
                objectName: "timeFormatRow"
                label: I18n.tr("settings.language.time_format", "Time format")
                sublabel: I18n.tr("settings.language.controls_24_hour_or_am_pm_time_in_apps_and_t", "Controls 24-hour or AM/PM time in apps and the clock.")
                Controls.SearchableSelect {
                    objectName: "timeFormatSelect"
                    searchable: false
                    label: root.timeFormats[root.selectedTimeFormat] || ""
                    accessibleName: I18n.tr("settings.language.time_format", "Time format")
                    currentId: root.region.time_format || "system"
                    options: root.timeFormatIds.map((id, index) => ({ id: id, name: root.timeFormats[index] }))
                    onSelected: id => root.controller.setOption("time_format", id)
                }
                isLast: true
            }
        }

        Footnote {
            objectName: "formatPreviewRow"
            Layout.topMargin: Components.Theme.spacingSmall
            horizontalAlignment: Text.AlignLeft
            text: {
                const preview = root.snapshot.formatPreview
                return preview ? I18n.tr("settings.language.format_preview", "Format preview") + ": " + preview.date + " · " + preview.number : ""
            }
        }
    }
}
