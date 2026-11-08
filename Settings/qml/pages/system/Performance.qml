import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "performancePage"

    readonly property var controller: SettingsController.performance
    readonly property var snapshot: parseSnapshot(controller.stateJson)
    readonly property var config: snapshot.config || ({})
    readonly property var status: snapshot.status || ({})
    readonly property string errorMessage: controller.lastError || status.error || ""
    readonly property var scx: status.scx || ({})
    readonly property var current: scx.current || ({})
    readonly property var catalog: scx.catalog || []
    readonly property var modes: scx.modes || []
    readonly property var supported: scx.supported || []
    property bool advancedSchedulers: false

    readonly property var profileIds: ["power-saver", "balanced", "performance"]
    readonly property var profileLabels: [I18n.tr("settings.performance.economy", "Economy"), I18n.tr("settings.performance.balanced", "Balanced"), I18n.tr("settings.performance.performance", "Performance")]
    readonly property int selectedProfile: Math.max(0, root.profileIds.indexOf(root.config.profile || "balanced"))
    readonly property string selectedScheduler: root.config.scx_scheduler || "bpfland"
    readonly property string selectedMode: root.config.scx_mode || "gaming"
    readonly property bool schedulerAvailable: !!root.scx.scxctl && root.supported.indexOf(root.selectedScheduler) >= 0

    function parseSnapshot(serialized) {
        try { return JSON.parse(serialized || "{}") } catch (error) { return ({}) }
    }
    function setPreference(key, value) {
        root.controller.setPreference(key, JSON.stringify(value))
    }
    function visibleSchedulers() {
        if (root.advancedSchedulers) return root.catalog
        return root.catalog.slice(0, 4)
    }
    function schedulerName(id) {
        for (let i = 0; i < root.catalog.length; ++i) {
            if (root.catalog[i].id === id) return root.catalog[i].label
        }
        return id
    }
    function modeName(id) {
        for (let i = 0; i < root.modes.length; ++i) {
            if (root.modes[i].id === id) return root.modes[i].label
        }
        return id
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.performance.performance", "Performance")
            subtitle: I18n.tr("settings.performance.power_profile_cpu_scheduler_and_tuning", "Power profile, CPU scheduler and tuning.")
        }

        Components.BackendNotice {
            objectName: "performanceBackendStatus"
            retryObjectName: "performanceRetryRefreshButton"
            busy: root.controller.busy
            error: root.errorMessage
            busyText: I18n.tr("settings.performance.reading_or_applying_performance_settings", "Reading or applying performance settings…")
            onRetry: root.controller.refresh()
        }

        Form.SectionHeader { text: I18n.tr("settings.performance.power_profile", "POWER PROFILE"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "preferredPowerProfileRow"
                label: I18n.tr("settings.performance.preferred_profile", "Preferred profile")
                sublabel: root.status.powerprofilesctl
                    ? I18n.tr("settings.performance.active_system_profile", "Active system profile: ") + (root.status.profile || I18n.tr("settings.performance.unknown", "Unknown"))
                    : "powerprofilesctl is not installed; profile changes are unavailable."
                isLast: true
                Controls.SelectButton {
                    objectName: "powerProfileSelect"
                    width: 156
                    options: root.profileLabels
                    selectedIndex: root.selectedProfile
                    label: root.profileLabels[root.selectedProfile]
                    enabled: !!root.status.powerprofilesctl && !root.controller.busy
                    onSelected: index => root.controller.applyProfile(root.profileIds[index])
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.performance.sched_ext", "SCHED-EXT"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("settings.performance.active_scheduler", "Active scheduler")
                sublabel: I18n.tr("settings.performance.observed_sched_ext_state_from_the_system", "Observed sched-ext state from the system.")
                Text {
                    text: root.current.active ? (root.current.scheduler_label || root.current.scheduler || I18n.tr("settings.performance.active", "Active")) : I18n.tr("settings.performance.inactive", "Inactive")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.typography.role("caption").family
                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                }
            }
            Form.SettingRow {
                objectName: "schedulerApplyRow"
                label: I18n.tr("settings.performance.apply_sched_ext_profile", "Apply sched-ext profile")
                sublabel: I18n.tr("settings.performance.selected_scheduler", "Selected scheduler: ") + root.schedulerName(root.selectedScheduler) + " · " + root.modeName(root.selectedMode)
                Controls.ButtonCapsule {
                    objectName: "applySchedulerButton"
                    label: I18n.tr("settings.performance.apply", "Apply")
                    enabled: root.schedulerAvailable && !root.controller.busy
                    Accessible.name: "Apply scheduler profile"
                    onClicked: root.controller.applyScheduler(root.selectedScheduler, root.selectedMode)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.performance.available_schedulers", "AVAILABLE SCHEDULERS"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "recommendedSchedulersCard"
            Layout.bottomMargin: 24
            Repeater {
                model: root.visibleSchedulers()
                delegate: Form.SettingRow {
                    required property var modelData
                    required property int index
                    clickable: !root.controller.busy && root.supported.indexOf(modelData.id) >= 0
                    label: modelData.label
                    sublabel: modelData.best_for + " · " + modelData.description
                    isLast: index === root.visibleSchedulers().length - 1
                    Controls.ButtonCapsule {
                        readonly property bool isSelected: modelData.id === root.selectedScheduler
                        readonly property bool isSupported: root.supported.indexOf(modelData.id) >= 0
                        objectName: "schedulerChoice-" + modelData.id
                        label: isSelected ? I18n.tr("settings.performance.selected", "Selected") : (isSupported ? I18n.tr("settings.performance.choose", "Choose") : I18n.tr("settings.performance.unavailable", "Unavailable"))
                        iconText: isSelected ? "\uf00c" : ""
                        primary: isSelected
                        enabled: (isSelected || isSupported) && !root.controller.busy
                        onClicked: if (!isSelected) root.setPreference("scx_scheduler", modelData.id)
                    }
                    onClicked: root.setPreference("scx_scheduler", modelData.id)
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: root.advancedSchedulers ? I18n.tr("settings.performance.hide_additional_schedulers", "Hide additional schedulers") : I18n.tr("settings.performance.show_additional_schedulers", "Show additional schedulers")
                sublabel: I18n.tr("settings.performance.schedulers_detected_on_this_system_with_the", "Schedulers detected on this system, with the available choices listed above.")
                clickable: true
                Controls.ButtonCapsule {
                    label: root.advancedSchedulers ? I18n.tr("settings.performance.hide", "Hide") : I18n.tr("settings.performance.show", "Show")
                    onClicked: root.advancedSchedulers = !root.advancedSchedulers
                }
                onClicked: root.advancedSchedulers = !root.advancedSchedulers
                isLast: !root.advancedSchedulers
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.performance.scheduler_mode", "SCHEDULER MODE"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "schedulerModesCard"
            Layout.bottomMargin: 24
            Repeater {
                model: root.modes
                delegate: Form.SettingRow {
                    required property var modelData
                    required property int index
                    clickable: !root.controller.busy
                    label: modelData.label
                    sublabel: modelData.description
                    isLast: index === root.modes.length - 1
                    Controls.ButtonCapsule {
                        readonly property bool isSelected: modelData.id === root.selectedMode
                        objectName: "schedulerModeChoice-" + modelData.id
                        label: isSelected ? I18n.tr("settings.performance.selected", "Selected") : I18n.tr("settings.performance.choose", "Choose")
                        iconText: isSelected ? "\uf00c" : ""
                        primary: isSelected
                        enabled: !root.controller.busy
                        onClicked: if (!isSelected) root.setPreference("scx_mode", modelData.id)
                    }
                    onClicked: root.setPreference("scx_mode", modelData.id)
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.performance.system_behavior", "SYSTEM BEHAVIOR"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Form.SettingRow {
                objectName: "gamemodeRow"
                label: "GameMode"
                sublabel: root.status.gamemode
                    ? (root.status.gamemode_active ? I18n.tr("settings.performance.gamemode_is_installed_and_currently_active", "GameMode is installed and currently active.") : I18n.tr("settings.performance.request_performance_hints_while_a_game_is_ac", "Request performance hints while a game is active."))
                    : I18n.tr("settings.performance.gamemode_is_not_installed_this_preference_is", "GameMode is not installed; this preference is unavailable.")
                Controls.ToggleSwitch {
                    accessibleName: "GameMode"
                    checked: !!root.config.prefer_gamemode
                    enabled: !!root.status.gamemode && !root.controller.busy
                    onToggled: checked => root.setPreference("prefer_gamemode", checked)
                }
                isLast: true
            }
        }
    }
}
