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
            const name = id === "proton" ? "Proton" : (id === "wine" ? "Wine" : "Auto")
            return id !== "auto" && !root.capabilities[id] ? name + " (unavailable)" : name
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
            title: "Compatibility"
            subtitle: "How Windows programs open outside Steam."
        }

        Rectangle {
            objectName: "compatibilityBackendStatus"
            visible: root.controller.busy || root.controller.lastError !== ""
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
                text: root.controller.busy ? "Saving compatibility settings…" : root.controller.lastError
                color: root.controller.lastError !== "" ? Components.Theme.errorColor : Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.Wrap
            }
            Controls.ButtonCapsule {
                id: retryRefresh
                objectName: "compatibilityRetryRefreshButton"
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.controller.busy && root.controller.lastError !== ""
                label: "Retry"
                onClicked: root.controller.refresh()
            }
        }

        Form.SectionHeader { text: "RUNNER"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityGeneralCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "exeRunnerRow"
                label: "Open .exe with"
                sublabel: root.runnerIds.length > 1
                    ? "Select an installed compatibility runner."
                    : "Only automatic runner selection is available on this system."
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
                label: "Use Proton profile"
                sublabel: "Reuse GameMode, MangoHud, Gamescope and flags from Proton."
                Controls.ToggleSwitch {
                    accessibleName: "Use Proton profile"
                    checked: !!root.compatibility.use_proton_profile
                    onToggled: checked => root.setOption("use_proton_profile", checked)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "RUNTIME"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityRuntimeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "compatibilityGamemodeRow"
                label: "GameMode"
                sublabel: root.customOptionsEnabled
                    ? (root.capabilities.gamemode ? "Add performance hints before starting the runner." : "GameMode is not installed.")
                    : "Controlled by the Proton profile."
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
                    ? (root.capabilities.mangohud ? "Show performance telemetry while the game runs." : "MangoHud is not installed.")
                    : "Controlled by the Proton profile."
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
                    ? (root.capabilities.gamescope ? "Use Gamescope tuning from the Proton page." : "Gamescope is not installed.")
                    : "Controlled by the Proton profile."
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

        Form.SectionHeader { text: "ADVANCED"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityAdvancedCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: "Custom environment"
                sublabel: root.customOptionsEnabled ? "Space-separated KEY=VALUE pairs for this runner." : "Turn off Use Proton profile to edit these options."
                enabled: root.customOptionsEnabled
                TextField {
                    objectName: "compatibilityEnvironmentField"
                    Accessible.name: "Custom environment"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    implicitHeight: 32
                    placeholderText: "KEY=VALUE"
                    placeholderTextColor: Components.Theme.textTertiary
                    text: root.compatibility.extra_env || ""
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: Components.Theme.fontSizeMicro
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onEditingFinished: root.setOption("extra_env", text)
                }
            }
            Form.SettingRow {
                label: "Custom prefix"
                sublabel: root.customOptionsEnabled ? "Arguments inserted before Wine or Proton launches the game." : "Turn off Use Proton profile to edit these options."
                enabled: root.customOptionsEnabled
                TextField {
                    objectName: "compatibilityPrefixField"
                    Accessible.name: "Custom prefix"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    implicitHeight: 32
                    placeholderText: "Optional command"
                    placeholderTextColor: Components.Theme.textTertiary
                    text: root.compatibility.extra_prefix || ""
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: Components.Theme.fontSizeMicro
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onEditingFinished: root.setOption("extra_prefix", text)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "PATHS"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "compatibilityPathsCard"
            Form.SettingRow {
                objectName: "compatibilityPrefixPathRow"
                label: "Prefix"
                sublabel: (root.usesWine ? root.status.wine_prefix_path : root.status.prefix_path) || "Path is not available"
                sublabelTextFormat: Text.PlainText
            }
            Form.SettingRow {
                objectName: "compatibilityLogsPathRow"
                label: "Logs"
                sublabel: root.status.log_path || "Path is not available"
                sublabelTextFormat: Text.PlainText
                isLast: true
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: 38 }
    }
}
