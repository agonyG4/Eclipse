import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "compatibilityPage"

    property int runner: 0
    property bool useProtonProfile: true
    property bool gamemode: true
    property bool mangohud: false
    property bool gamescope: false
    property string customEnvironment: ""
    property string customPrefix: ""

    readonly property var runners: ["Proton", "Wine", "Auto"]
    readonly property bool customOptionsEnabled: !root.useProtonProfile

    Form.ScrollPage {
        objectName: "compatibilityScrollPage"
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "compatibilityPreviewNotice"
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

        Form.SectionHeader { text: "COMPATIBILITY"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "compatibilityGeneralCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "exeRunnerRow"
                label: "Open .exe with"
                sublabel: "Choose which Windows compatibility layer to use."
                Controls.SelectButton {
                    objectName: "exeRunnerSelect"
                    width: 132
                    options: root.runners
                    selectedIndex: root.runner
                    label: root.runners[root.runner]
                    onSelected: index => root.runner = index
                }
            }
            Form.SettingRow {
                objectName: "useProtonProfileRow"
                label: "Use Proton profile"
                sublabel: "Reuse GameMode, MangoHud, Gamescope and flags from Proton."
                Controls.ToggleSwitch { accessibleName: "Use Proton profile"; checked: root.useProtonProfile; onToggled: checked => root.useProtonProfile = checked }
                isLast: true
            }
        }

        Form.SectionHeader { text: "RUNTIME"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "compatibilityRuntimeCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "compatibilityGamemodeRow"
                label: "GameMode"
                sublabel: root.useProtonProfile ? "Controlled by the Proton profile." : "Add performance hints before starting the runner."
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch { accessibleName: "GameMode"; checked: root.gamemode; enabled: root.customOptionsEnabled; onToggled: checked => root.gamemode = checked }
            }
            Form.SettingRow {
                label: "MangoHud"
                sublabel: root.useProtonProfile ? "Controlled by the Proton profile." : "Show performance telemetry while the game runs."
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch { accessibleName: "MangoHud"; checked: root.mangohud; enabled: root.customOptionsEnabled; onToggled: checked => root.mangohud = checked }
            }
            Form.SettingRow {
                label: "Gamescope"
                sublabel: root.useProtonProfile ? "Controlled by the Proton profile." : "Use the Gamescope tuning from the Proton page."
                enabled: root.customOptionsEnabled
                Controls.ToggleSwitch { accessibleName: "Gamescope"; checked: root.gamescope; enabled: root.customOptionsEnabled; onToggled: checked => root.gamescope = checked }
                isLast: true
            }
        }

        Form.SectionHeader { text: "ADVANCED"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "compatibilityAdvancedCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: "Custom environment"
                sublabel: root.useProtonProfile ? "Turn off Use Proton profile to edit these options." : "Space-separated KEY=VALUE pairs for this runner."
                enabled: root.customOptionsEnabled
                TextField {
                    objectName: "compatibilityEnvironmentField"
                    Accessible.name: "Custom environment"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    implicitHeight: 32
                    placeholderText: "KEY=VALUE"
                    text: root.customEnvironment
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: Components.Theme.fontSizeMicro
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.customEnvironment = text
                }
            }
            Form.SettingRow {
                label: "Custom prefix"
                sublabel: root.useProtonProfile ? "Turn off Use Proton profile to edit these options." : "Command inserted before Wine or Proton launches the game."
                enabled: root.customOptionsEnabled
                TextField {
                    objectName: "compatibilityPrefixField"
                    Accessible.name: "Custom prefix"
                    enabled: root.customOptionsEnabled
                    implicitWidth: 158
                    implicitHeight: 32
                    placeholderText: "Optional command"
                    text: root.customPrefix
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: Components.Theme.fontSizeMicro
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.customPrefix = text
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "PATHS"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "compatibilityPathsCard"
            Form.SettingRow {
                objectName: "compatibilityPrefixPathRow"
                label: "Prefix"
                sublabel: "~/.local/share/AstreaOS/windows-prefixes/shared/proton/pfx"
                sublabelTextFormat: Text.PlainText
            }
            Form.SettingRow {
                objectName: "compatibilityLogsPathRow"
                label: "Logs"
                sublabel: "~/.local/state/AstreaOS/windows-prefixes/logs"
                sublabelTextFormat: Text.PlainText
                Controls.ButtonCapsule { objectName: "saveCompatibilityButton"; label: "Save"; enabled: false }
                isLast: true
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: 38 }
    }
}
