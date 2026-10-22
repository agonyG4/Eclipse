import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "protonPage"

    property string launchOption: "astrea-gaming %command%"
    property string extraArgs: ""
    property string customEnv: "DXVK_FRAME_RATE=60"
    property string customPrefix: ""
    property string outputWidth: "1920"
    property string outputHeight: "1080"
    property string refreshRate: "60"
    property int preset: 0
    property int syncBehavior: 0
    property int fsrSharpness: 2
    property bool useSteamProfile: true
    property bool gamemode: true
    property bool mangohud: false
    property bool gamescope: false
    property bool fullscreen: true
    property bool immediateFlips: false
    property bool hideCursor: false
    property bool forceCursorGrab: false
    property bool adaptiveSync: false
    property bool nvapi: false
    property bool hdr: false
    property bool fsr: false
    property bool hideNvidia: false
    property bool wineD3d: false
    property bool vkd3dDxr: false
    property bool dxvkAsync: false

    readonly property var presets: ["Recommended", "NVIDIA features", "HDR display", "Diagnostics", "Custom"]
    readonly property var syncModes: ["Proton default", "Disable Esync", "Disable Fsync", "Disable both"]
    readonly property var sharpness: ["0", "1", "2", "3", "4", "5"]
    readonly property bool manualGamescope: !root.useSteamProfile

    Form.ScrollPage {
        objectName: "protonScrollPage"
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "protonPreviewNotice"
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

        Form.SectionHeader { text: "PROTON"; Layout.bottomMargin: 10 }
        Form.SectionHeader { text: "SETUP"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "protonSetupCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "steamLaunchOptionRow"
                label: "Steam launch option"
                sublabel: "Command used to start games through the Proton wrapper."
                RowLayout {
                    spacing: 6
                    TextField {
                        objectName: "steamLaunchOptionField"
                        Accessible.name: "Steam launch option"
                        Layout.preferredWidth: 134
                        implicitHeight: 34
                        text: root.launchOption
                        color: Components.Theme.textPrimary
                        selectionColor: Components.Theme.accent
                        selectedTextColor: Components.Theme.accentForeground
                        placeholderTextColor: Components.Theme.textTertiary
                        font.family: Components.Theme.monoFontFamily
                        font.pixelSize: Components.Theme.fontSizeTiny
                        background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                        onTextEdited: root.launchOption = text
                    }
                    Controls.ButtonCapsule { objectName: "saveLaunchOptionButton"; label: "Save"; enabled: false }
                }
            }
            Form.SettingRow { label: "Active flags"; sublabel: "GameMode · MangoHud optional · Gamescope optional" }
            Form.SettingRow { label: "Command prefix"; sublabel: "Example: gamemoderun mangohud" }
            Form.SettingRow { label: "Wrapper"; sublabel: "~/.local/bin/astrea-gaming"; sublabelTextFormat: Text.PlainText; isLast: true }
        }

        Form.SectionHeader { text: "PROFILE"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "protonProfileCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "protonPresetRow"
                label: "Proton preset"
                sublabel: "Profiles for common game compatibility needs."
                Controls.SelectButton {
                    objectName: "protonPresetSelect"
                    width: 142
                    options: root.presets
                    selectedIndex: root.preset
                    label: root.presets[root.preset]
                    onSelected: index => root.preset = index
                }
            }
            Form.SettingRow {
                objectName: "syncBehaviorRow"
                label: "Sync behavior"
                sublabel: "Keep Proton defaults unless a game needs Esync or Fsync disabled."
                Controls.SelectButton {
                    width: 142
                    options: root.syncModes
                    selectedIndex: root.syncBehavior
                    label: root.syncModes[root.syncBehavior]
                    onSelected: index => root.syncBehavior = index
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "RUNTIME"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "protonRuntimeCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "protonGamemodeRow"
                label: "GameMode"
                sublabel: "Allow games to request performance hints."
                Controls.ToggleSwitch { accessibleName: "GameMode"; checked: root.gamemode; onToggled: checked => root.gamemode = checked }
            }
            Form.SettingRow {
                objectName: "mangohudRow"
                label: "MangoHud"
                sublabel: "Show frame rate, frame time and hardware telemetry."
                Controls.ToggleSwitch { accessibleName: "MangoHud"; checked: root.mangohud; onToggled: checked => root.mangohud = checked }
            }
            Form.SettingRow {
                objectName: "protonGamescopeRow"
                label: "Gamescope"
                sublabel: "Wrap the game with the Gamescope options below."
                Controls.ToggleSwitch { accessibleName: "Gamescope"; checked: root.gamescope; onToggled: checked => root.gamescope = checked }
                isLast: true
            }
        }

        Form.SectionHeader { text: "GAMESCOPE"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "gamescopeCard"
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "steamGamescopeProfileRow"
                label: "Use SteamOS profile"
                sublabel: "Use the example resolution and refresh values below."
                Controls.ToggleSwitch { accessibleName: "Use SteamOS profile"; checked: root.useSteamProfile; onToggled: checked => root.useSteamProfile = checked }
            }
            Form.SettingRow {
                label: "Width"
                sublabel: "Nested Gamescope output width."
                TextField {
                    objectName: "gamescopeWidthField"
                    Accessible.name: "Width"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    implicitHeight: 32
                    text: root.outputWidth
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.pixelSize: Components.Theme.fontSizeSmall
                    horizontalAlignment: TextInput.AlignRight
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.outputWidth = text
                }
            }
            Form.SettingRow {
                label: "Height"
                sublabel: "Nested Gamescope output height."
                TextField {
                    objectName: "gamescopeHeightField"
                    Accessible.name: "Height"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    implicitHeight: 32
                    text: root.outputHeight
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.pixelSize: Components.Theme.fontSizeSmall
                    horizontalAlignment: TextInput.AlignRight
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.outputHeight = text
                }
            }
            Form.SettingRow {
                label: "Refresh rate"
                sublabel: "Manual nested Gamescope refresh limit."
                TextField {
                    objectName: "gamescopeRefreshField"
                    Accessible.name: "Refresh rate"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    implicitHeight: 32
                    text: root.refreshRate
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.pixelSize: Components.Theme.fontSizeSmall
                    horizontalAlignment: TextInput.AlignRight
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.refreshRate = text
                }
            }
            Form.SettingRow { label: "Fullscreen"; sublabel: "Start the nested session in fullscreen."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Fullscreen"; checked: root.fullscreen; onToggled: checked => root.fullscreen = checked } }
            Form.SettingRow { label: "Immediate flips"; sublabel: "Reduce latency on display paths that support it."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Immediate flips"; checked: root.immediateFlips; onToggled: checked => root.immediateFlips = checked } }
            Form.SettingRow { label: "Hide cursor"; sublabel: "Keep the pointer hidden for controller-first games."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Hide cursor"; checked: root.hideCursor; onToggled: checked => root.hideCursor = checked } }
            Form.SettingRow { label: "Force cursor grab"; sublabel: "Keep relative mouse input inside the game."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Force cursor grab"; checked: root.forceCursorGrab; onToggled: checked => root.forceCursorGrab = checked } }
            Form.SettingRow { label: "Adaptive sync"; sublabel: "Request variable refresh when the display supports it."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Adaptive sync"; checked: root.adaptiveSync; onToggled: checked => root.adaptiveSync = checked } }
            Form.SettingRow {
                label: "Extra arguments"
                sublabel: "Additional Gamescope flags after generated resolution values."
                TextField {
                    objectName: "gamescopeExtraArgumentsField"
                    Accessible.name: "Extra arguments"
                    enabled: root.manualGamescope
                    implicitWidth: 130
                    implicitHeight: 32
                    placeholderText: "Optional flags"
                    text: root.extraArgs
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.pixelSize: Components.Theme.fontSizeTiny
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.extraArgs = text
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "GRAPHICS FEATURES"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "protonGraphicsCard"
            Layout.bottomMargin: 22
            Form.SettingRow { label: "NVIDIA NVAPI"; sublabel: "Enable NVIDIA-specific features for supported games."; Controls.ToggleSwitch { accessibleName: "NVIDIA NVAPI"; checked: root.nvapi; onToggled: checked => root.nvapi = checked } }
            Form.SettingRow { label: "HDR path"; sublabel: "Enable DXVK HDR for an HDR-ready display setup."; Controls.ToggleSwitch { accessibleName: "HDR path"; checked: root.hdr; onToggled: checked => root.hdr = checked } }
            Form.SettingRow { label: "Fullscreen FSR"; sublabel: "Upscale games rendered below native resolution."; Controls.ToggleSwitch { accessibleName: "Fullscreen FSR"; checked: root.fsr; onToggled: checked => root.fsr = checked } }
            Form.SettingRow {
                label: "FSR sharpness"
                sublabel: "Lower values are sharper; used when Fullscreen FSR is on."
                Controls.SelectButton {
                    objectName: "fsrSharpnessSelect"
                    width: 132
                    options: root.sharpness
                    selectedIndex: root.fsrSharpness
                    label: root.sharpness[root.fsrSharpness]
                    onSelected: index => root.fsrSharpness = index
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "ADVANCED COMPATIBILITY"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "protonAdvancedCard"
            Form.SettingRow { label: "Hide NVIDIA GPU"; sublabel: "Compatibility option for games that select the wrong GPU."; Controls.ToggleSwitch { accessibleName: "Hide NVIDIA GPU"; checked: root.hideNvidia; onToggled: checked => root.hideNvidia = checked } }
            Form.SettingRow { label: "WineD3D fallback"; sublabel: "Use WineD3D instead of DXVK when rendering fails."; Controls.ToggleSwitch { accessibleName: "WineD3D fallback"; checked: root.wineD3d; onToggled: checked => root.wineD3d = checked } }
            Form.SettingRow { label: "Force VKD3D DXR"; sublabel: "Force the DirectX ray tracing path for a specific game."; Controls.ToggleSwitch { accessibleName: "Force VKD3D DXR"; checked: root.vkd3dDxr; onToggled: checked => root.vkd3dDxr = checked } }
            Form.SettingRow { label: "Legacy DXVK Async"; sublabel: "Non-default flag for older async-patched DXVK builds."; Controls.ToggleSwitch { accessibleName: "Legacy DXVK Async"; checked: root.dxvkAsync; onToggled: checked => root.dxvkAsync = checked } }
            Form.SettingRow {
                label: "Custom environment"
                sublabel: "Space-separated KEY=VALUE pairs for one-off fixes."
                TextField {
                    objectName: "protonCustomEnvironmentField"
                    Accessible.name: "Custom environment"
                    implicitWidth: 158
                    implicitHeight: 32
                    text: root.customEnv
                    color: Components.Theme.textPrimary
                    selectionColor: Components.Theme.accent
                    selectedTextColor: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: Components.Theme.fontSizeMicro
                    background: Rectangle { radius: Components.Theme.controlRadius; color: Components.Theme.windowBackground; border.width: 1; border.color: Components.Theme.cardBorder }
                    onTextEdited: root.customEnv = text
                }
            }
            Form.SettingRow {
                label: "Custom prefix"
                sublabel: "Optional command inserted before the game command."
                TextField {
                    objectName: "protonCustomPrefixField"
                    Accessible.name: "Custom prefix"
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
    }
}
