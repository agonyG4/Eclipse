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
    readonly property var profileLabels: ["Economy", "Balanced", "Performance"]
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
            title: "Performance"
            subtitle: "Power profile, CPU scheduler and tuning."
        }

        Rectangle {
            objectName: "performanceBackendStatus"
            visible: root.controller.busy || root.errorMessage !== ""
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            implicitHeight: statusText.implicitHeight + 18
            radius: Components.Theme.controlRadius
            color: Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035)
            border.width: 1
            border.color: Components.Theme.cardBorder
            Text {
                id: statusText
                anchors.left: parent.left
                anchors.right: retryRefresh.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: root.controller.busy ? "Reading or applying performance settings…" : root.errorMessage
                color: root.errorMessage !== "" ? Components.Theme.errorColor : Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }
            Controls.ButtonCapsule {
                id: retryRefresh
                objectName: "performanceRetryRefreshButton"
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.controller.busy && root.errorMessage !== ""
                label: "Retry"
                onClicked: root.controller.refresh()
            }
        }

        Form.SectionHeader { text: "POWER PROFILE"; Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "preferredPowerProfileRow"
                label: "Preferred profile"
                sublabel: root.status.powerprofilesctl
                    ? "Active system profile: " + (root.status.profile || "Unknown")
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

        Form.SectionHeader { text: "SCHED-EXT"; Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: "Active scheduler"
                sublabel: "Observed sched-ext state from the system."
                Text {
                    text: root.current.active ? (root.current.scheduler_label || root.current.scheduler || "Active") : "Inactive"
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                }
            }
            Form.SettingRow {
                objectName: "schedulerApplyRow"
                label: "Apply sched-ext profile"
                sublabel: "Selected scheduler: " + root.schedulerName(root.selectedScheduler) + " · " + root.modeName(root.selectedMode)
                Controls.ButtonCapsule {
                    objectName: "applySchedulerButton"
                    label: "Apply"
                    enabled: root.schedulerAvailable && !root.controller.busy
                    Accessible.name: "Apply scheduler profile"
                    onClicked: root.controller.applyScheduler(root.selectedScheduler, root.selectedMode)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "AVAILABLE SCHEDULERS"; Layout.bottomMargin: 12 }
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
                    Text {
                        text: modelData.id === root.selectedScheduler ? "Selected" : (root.supported.indexOf(modelData.id) >= 0 ? "Choose" : "Unavailable")
                        color: modelData.id === root.selectedScheduler ? Components.Theme.accent : Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    onClicked: root.setPreference("scx_scheduler", modelData.id)
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: root.advancedSchedulers ? "Hide additional schedulers" : "Show additional schedulers"
                sublabel: "Schedulers detected on this system, with the available choices listed above."
                clickable: true
                Controls.ButtonCapsule {
                    label: root.advancedSchedulers ? "Hide" : "Show"
                    onClicked: root.advancedSchedulers = !root.advancedSchedulers
                }
                onClicked: root.advancedSchedulers = !root.advancedSchedulers
                isLast: !root.advancedSchedulers
            }
        }

        Form.SectionHeader { text: "SCHEDULER MODE"; Layout.bottomMargin: 12 }
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
                    Text {
                        text: modelData.id === root.selectedMode ? "Selected" : "Choose"
                        color: modelData.id === root.selectedMode ? Components.Theme.accent : Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    onClicked: root.setPreference("scx_mode", modelData.id)
                }
            }
        }

        Form.SectionHeader { text: "SYSTEM BEHAVIOR"; Layout.bottomMargin: 12 }
        Form.FormCard {
            Form.SettingRow {
                objectName: "gamemodeRow"
                label: "GameMode"
                sublabel: root.status.gamemode
                    ? (root.status.gamemode_active ? "GameMode is installed and currently active." : "Request performance hints while a game is active.")
                    : "GameMode is not installed; this preference is unavailable."
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
