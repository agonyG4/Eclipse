import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "compatibilityPage"

    readonly property var controller: SettingsController.gaming
    readonly property var snapshot: parseSnapshot(controller.stateJson)
    readonly property var compatibility: snapshot.compatibility || ({})
    readonly property var status: snapshot.status || ({})
    readonly property var capabilities: status.capabilities || ({})
    readonly property var runnerIds: availableRunnerIds()
    readonly property var runners: runnerLabels()
    readonly property int runnerIndex: Math.max(0, root.runnerIds.indexOf(root.compatibility.runner || "proton"))
    readonly property bool usesWine: root.compatibility.runner === "wine" || (root.compatibility.runner === "auto" && !root.capabilities.proton)
    readonly property bool customOptionsEnabled: !root.compatibility.use_proton_profile

    function parseSnapshot(serialized) {
        try { return JSON.parse(serialized || "{}") } catch (error) { return ({}) }
    }
    function availableRunnerIds() {
        const choices = []
        if (root.capabilities.proton) choices.push("proton")
        if (root.capabilities.wine) choices.push("wine")
        choices.push("auto")
        const configured = root.compatibility.runner || "proton"
        if (choices.indexOf(configured) < 0) choices.unshift(configured)
        return choices
    }
    function runnerLabels() {
        return root.runnerIds.map(id => {
            const name = id === "proton" ? "Proton" : (id === "wine" ? "Wine" : I18n.tr("settings.compatibility.auto", "Auto"))
            return id !== "auto" && !root.capabilities[id] ? name + " " + I18n.tr("settings.compatibility.runner_unavailable", "(unavailable)") : name
        })
    }
    function setOption(key, value) {
        root.controller.setCompatibilityOption(key, JSON.stringify(value))
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        objectName: "compatibilityScrollPage"
        anchors.fill: parent
        enabled: !root.controller.busy
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.compatibility.compatibility", "Compatibility")
            subtitle: I18n.tr("settings.compatibility.how_windows_programs_open_outside_steam", "How Windows programs open outside Steam.")
        }

        Components.BackendNotice {
            objectName: "compatibilityBackendStatus"
            retryObjectName: "compatibilityRetryRefreshButton"
            busy: root.controller.busy
            error: root.controller.lastError
            busyText: I18n.tr("settings.compatibility.saving_compatibility_settings", "Saving compatibility settings…")
            onRetry: root.controller.refresh()
        }

        Form.SectionHeader { text: I18n.tr("settings.compatibility.runner", "RUNNER"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityGeneralCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "exeRunnerRow"
                label: I18n.tr("settings.compatibility.open_exe_with", "Open .exe with")
                sublabel: root.runnerIds.length > 1
                    ? I18n.tr("settings.compatibility.select_an_installed_compatibility_runner", "Select an installed compatibility runner.")
                    : I18n.tr("settings.compatibility.only_automatic_runner_selection_is_available", "Only automatic runner selection is available on this system.")
                Controls.SelectButton {
                    objectName: "exeRunnerSelect"
                    width: 190
                    options: root.runners
                    selectedIndex: root.runnerIndex
                    label: root.runners[root.runnerIndex]
                    enabled: root.runnerIds.length > 1
                    onSelected: index => root.setOption("runner", root.runnerIds[index])
                }
            }
            Form.SettingRow {
                objectName: "useProtonProfileRow"
                label: I18n.tr("settings.compatibility.use_proton_profile", "Use Proton profile")
                sublabel: I18n.tr("settings.compatibility.reuse_gamemode_mangohud_gamescope_and_flags", "Reuse GameMode, MangoHud, Gamescope and flags from Proton.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.compatibility.use_proton_profile", "Use Proton profile")
                    checked: !!root.compatibility.use_proton_profile
                    onToggled: checked => root.setOption("use_proton_profile", checked)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.compatibility.runtime", "RUNTIME"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityRuntimeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "compatibilityGamemodeRow"
                label: "GameMode"
                sublabel: root.customOptionsEnabled
                    ? (root.capabilities.gamemode ? I18n.tr("settings.compatibility.add_performance_hints_before_starting_the_ru", "Add performance hints before starting the runner.") : I18n.tr("settings.compatibility.gamemode_is_not_installed", "GameMode is not installed."))
                    : I18n.tr("settings.compatibility.controlled_by_the_proton_profile", "Controlled by the Proton profile.")
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch {
                    accessibleName: "GameMode"
                    checked: !!root.compatibility.gamemode
                    enabled: root.customOptionsEnabled && !!root.capabilities.gamemode
                    onToggled: checked => root.setOption("gamemode", checked)
                }
            }
            Form.SettingRow {
                label: "MangoHud"
                sublabel: root.customOptionsEnabled
                    ? (root.capabilities.mangohud ? I18n.tr("settings.compatibility.show_performance_telemetry_while_the_game_ru", "Show performance telemetry while the game runs.") : I18n.tr("settings.compatibility.mangohud_is_not_installed", "MangoHud is not installed."))
                    : I18n.tr("settings.compatibility.controlled_by_the_proton_profile", "Controlled by the Proton profile.")
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch {
                    accessibleName: "MangoHud"
                    checked: !!root.compatibility.mangohud
                    enabled: root.customOptionsEnabled && !!root.capabilities.mangohud
                    onToggled: checked => root.setOption("mangohud", checked)
                }
            }
            Form.SettingRow {
                label: "Gamescope"
                sublabel: root.customOptionsEnabled
                    ? (root.capabilities.gamescope ? I18n.tr("settings.compatibility.use_gamescope_tuning_from_the_proton_page", "Use Gamescope tuning from the Proton page.") : I18n.tr("settings.compatibility.gamescope_is_not_installed", "Gamescope is not installed."))
                    : I18n.tr("settings.compatibility.controlled_by_the_proton_profile", "Controlled by the Proton profile.")
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch {
                    accessibleName: "Gamescope"
                    checked: !!root.compatibility.gamescope
                    enabled: root.customOptionsEnabled && !!root.capabilities.gamescope
                    onToggled: checked => root.setOption("gamescope", checked)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.compatibility.advanced", "ADVANCED"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityAdvancedCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("settings.compatibility.custom_environment", "Custom environment")
                sublabel: root.customOptionsEnabled ? I18n.tr("settings.compatibility.space_separated_key_value_pairs_for_this_run", "Space-separated KEY=VALUE pairs for this runner.") : I18n.tr("settings.compatibility.turn_off_use_proton_profile_to_edit_these_op", "Turn off Use Proton profile to edit these options.")
                enabled: root.customOptionsEnabled
                Controls.FormTextField {
                    objectName: "compatibilityEnvironmentField"
                    Accessible.name: "Custom environment"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    placeholderText: "KEY=VALUE"
                    text: root.compatibility.extra_env || ""
                    onEditingFinished: root.setOption("extra_env", text)
                }
            }
            Form.SettingRow {
                label: I18n.tr("settings.compatibility.custom_prefix", "Custom prefix")
                sublabel: root.customOptionsEnabled ? I18n.tr("settings.compatibility.arguments_inserted_before_wine_or_proton_lau", "Arguments inserted before Wine or Proton launches the game.") : I18n.tr("settings.compatibility.turn_off_use_proton_profile_to_edit_these_op", "Turn off Use Proton profile to edit these options.")
                enabled: root.customOptionsEnabled
                Controls.FormTextField {
                    objectName: "compatibilityPrefixField"
                    Accessible.name: "Custom prefix"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    placeholderText: I18n.tr("settings.compatibility.optional_command", "Optional command")
                    text: root.compatibility.extra_prefix || ""
                    onEditingFinished: root.setOption("extra_prefix", text)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.compatibility.paths", "PATHS"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityPathsCard"
            Form.SettingRow {
                objectName: "compatibilityPrefixPathRow"
                label: I18n.tr("settings.compatibility.prefix", "Prefix")
                sublabel: (root.usesWine ? root.status.wine_prefix_path : root.status.prefix_path) || I18n.tr("settings.compatibility.path_is_not_available", "Path is not available")
                sublabelTextFormat: Text.PlainText
            }
            Form.SettingRow {
                objectName: "compatibilityLogsPathRow"
                label: I18n.tr("settings.compatibility.logs", "Logs")
                sublabel: root.status.log_path || I18n.tr("settings.compatibility.path_is_not_available", "Path is not available")
                sublabelTextFormat: Text.PlainText
                isLast: true
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: 38 }
    }
}
