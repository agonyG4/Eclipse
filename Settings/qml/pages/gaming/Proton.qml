import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "protonPage"

    readonly property var controller: SettingsController.gaming
    readonly property var snapshot: parseSnapshot(controller.stateJson)
    readonly property var proton: snapshot.proton || ({})
    readonly property var gamescopeConfig: snapshot.gamescope || ({})
    readonly property var status: snapshot.status || ({})
    readonly property var capabilities: status.capabilities || ({})
    readonly property var launcher: status.native_launcher || ({})
    readonly property var presetIds: ["recommended", "nvidia", "hdr", "diagnostic"]
    readonly property var presets: ["Recommended", "NVIDIA features", "HDR display", "Diagnostics"]
    readonly property string currentPreset: root.proton.preset || "recommended"
    readonly property int presetIndex: Math.max(0, root.presetIds.indexOf(root.currentPreset))
    readonly property bool customPreset: root.currentPreset === "custom"
    readonly property var syncModes: ["Proton default", "Disable Esync", "Disable Fsync", "Disable both"]
    readonly property var syncModeIds: ["default", "disable-esync", "disable-fsync", "disable-both"]
    readonly property int syncModeIndex: Math.max(0, root.syncModeIds.indexOf(root.proton.sync_mode || "default"))
    readonly property var sharpness: ["0", "1", "2", "3", "4", "5"]
    readonly property var caps: root.capabilities
    readonly property bool manualGamescope: !root.proton.use_gamescope_profile

    function parseSnapshot(serialized) {
        try { return JSON.parse(serialized || "{}") } catch (error) { return ({}) }
    }
    function setOption(key, value) {
        root.controller.setProtonOption(key, JSON.stringify(value))
    }
    function setInteger(key, text) {
        const value = Number(text)
        if (Number.isInteger(value)) root.setOption(key, value)
    }
    function pathText(key) {
        return root.status[key] || "Path is not available"
    }
    function activeFlags() {
        const flags = []
        if (root.proton.gamemode) flags.push("GameMode")
        if (root.proton.mangohud) flags.push("MangoHud")
        if (root.proton.gamescope) flags.push("Gamescope")
        return flags.length ? flags.join(" · ") : "No runtime wrappers enabled"
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        objectName: "protonScrollPage"
        anchors.fill: parent
        enabled: !root.controller.busy
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: "Proton"
            subtitle: "How Steam games launch through Proton."
        }

        Components.BackendNotice {
            objectName: "protonBackendStatus"
            retryObjectName: "protonRetryRefreshButton"
            busy: root.controller.busy
            error: root.controller.lastError
            busyText: "Saving Proton settings…"
            onRetry: root.controller.refresh()
        }

        Form.SectionHeader { text: "SETUP"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonSetupCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "steamLaunchOptionRow"
                label: "Steam launch option"
                sublabel: root.launcher.available ? "Copy this command into Steam's launch options." : "The astrea-gaming launcher is unavailable."
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 10
                Layout.bottomMargin: 14
                spacing: 6
                Controls.FormTextField {
                    monospace: true
                    objectName: "steamLaunchOptionField"
                    Accessible.name: "Steam launch option"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    readOnly: true
                    text: root.status.launch_command || ""
                }
                Controls.ButtonCapsule {
                    objectName: "saveLaunchOptionButton"
                    label: "Copy"
                    enabled: !!root.launcher.available
                    onClicked: SettingsController.copyGamingLaunchCommand()
                }
            }
            Form.SettingRow { label: "Active flags"; sublabel: root.activeFlags() }
            Form.SettingRow { label: "Command prefix"; sublabel: (root.status.proton_prefix || []).join(" ") || "No command prefix"; sublabelTextFormat: Text.PlainText }
            Form.SettingRow { label: "Wrapper"; sublabel: root.launcher.path || "Launcher is not installed"; sublabelTextFormat: Text.PlainText; isLast: true }
        }

        Form.SectionHeader { text: "PROFILE"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonProfileCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "protonPresetRow"
                label: "Proton preset"
                sublabel: root.customPreset ? "Custom settings are active; choose a preset to replace them." : "Profiles for common game compatibility needs."
                Controls.SelectButton {
                    objectName: "protonPresetSelect"
                    width: 142
                    options: root.presets
                    selectedIndex: root.presetIndex
                    label: root.customPreset ? "Custom" : root.presets[root.presetIndex]
                    onSelected: index => root.setOption("preset", root.presetIds[index])
                }
            }
            Form.SettingRow {
                objectName: "syncBehaviorRow"
                label: "Sync behavior"
                sublabel: "Keep Proton defaults unless a game needs Esync or Fsync disabled."
                Controls.SelectButton {
                    width: 142
                    options: root.syncModes
                    selectedIndex: root.syncModeIndex
                    label: root.syncModes[root.syncModeIndex]
                    onSelected: index => root.setOption("sync_mode", root.syncModeIds[index])
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "RUNTIME"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonRuntimeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "protonGamemodeRow"
                label: "GameMode"
                sublabel: "Allow games to request performance hints."
                Controls.ToggleSwitch { accessibleName: "GameMode"; checked: !!root.proton.gamemode; enabled: !!root.caps.gamemode; onToggled: checked => root.setOption("gamemode", checked) }
            }
            Form.SettingRow {
                objectName: "mangohudRow"
                label: "MangoHud"
                sublabel: "Show frame rate, frame time and hardware telemetry."
                Controls.ToggleSwitch { accessibleName: "MangoHud"; checked: !!root.proton.mangohud; enabled: !!root.caps.mangohud; onToggled: checked => root.setOption("mangohud", checked) }
            }
            Form.SettingRow {
                objectName: "protonGamescopeRow"
                label: "Gamescope"
                sublabel: "Wrap the game with the Gamescope options below."
                Controls.ToggleSwitch { accessibleName: "Gamescope"; checked: !!root.proton.gamescope; enabled: !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope", checked) }
                isLast: true
            }
        }

        Form.SectionHeader { text: "GAMESCOPE"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "gamescopeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "steamGamescopeProfileRow"
                label: "Use SteamOS profile"
                sublabel: root.manualGamescope ? "Use manually configured Gamescope values below." : "Follow the Gamescope profile configured for this Proton wrapper."
                Controls.ToggleSwitch { accessibleName: "Use SteamOS profile"; checked: !!root.proton.use_gamescope_profile; enabled: !!root.caps.gamescope; onToggled: checked => root.setOption("use_gamescope_profile", checked) }
            }
            Form.SettingRow {
                label: "Width"
                sublabel: "Nested Gamescope output width."
                Controls.FormTextField {
                    objectName: "gamescopeWidthField"
                    Accessible.name: "Width"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    text: String(root.proton.gamescope_width || "")
                    horizontalAlignment: TextInput.AlignRight
                    onEditingFinished: root.setInteger("gamescope_width", text)
                }
            }
            Form.SettingRow {
                label: "Height"
                sublabel: "Nested Gamescope output height."
                Controls.FormTextField {
                    objectName: "gamescopeHeightField"
                    Accessible.name: "Height"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    text: String(root.proton.gamescope_height || "")
                    horizontalAlignment: TextInput.AlignRight
                    onEditingFinished: root.setInteger("gamescope_height", text)
                }
            }
            Form.SettingRow {
                label: "Refresh rate"
                sublabel: "Manual nested Gamescope refresh limit."
                Controls.FormTextField {
                    objectName: "gamescopeRefreshField"
                    Accessible.name: "Refresh rate"
                    enabled: root.manualGamescope
                    implicitWidth: 96
                    text: String(root.proton.gamescope_refresh || "")
                    horizontalAlignment: TextInput.AlignRight
                    onEditingFinished: root.setInteger("gamescope_refresh", text)
                }
            }
            Form.SettingRow { label: "Fullscreen"; sublabel: "Start the nested session in fullscreen."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Fullscreen"; checked: !!root.proton.gamescope_fullscreen; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_fullscreen", checked) } }
            Form.SettingRow { label: "Immediate flips"; sublabel: "Reduce latency on display paths that support it."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Immediate flips"; checked: !!root.proton.gamescope_immediate_flips; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_immediate_flips", checked) } }
            Form.SettingRow { label: "Hide cursor"; sublabel: "Keep the pointer hidden for controller-first games."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Hide cursor"; checked: !!root.proton.gamescope_hide_cursor; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_hide_cursor", checked) } }
            Form.SettingRow { label: "Force cursor grab"; sublabel: "Keep relative mouse input inside the game."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Force cursor grab"; checked: !!root.proton.gamescope_force_grab_cursor; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_force_grab_cursor", checked) } }
            Form.SettingRow { label: "Adaptive sync"; sublabel: "Request variable refresh when the display supports it."; enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: "Adaptive sync"; checked: !!root.proton.gamescope_adaptive_sync; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_adaptive_sync", checked) } }
            Form.SettingRow {
                label: "Extra arguments"
                sublabel: "Additional Gamescope flags after generated resolution values."
                Controls.FormTextField {
                    objectName: "gamescopeExtraArgumentsField"
                    Accessible.name: "Extra arguments"
                    enabled: root.manualGamescope
                    implicitWidth: 130
                    placeholderText: "Optional flags"
                    text: root.proton.gamescope_extra_args || ""
                    onEditingFinished: root.setOption("gamescope_extra_args", text)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "GRAPHICS FEATURES"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonGraphicsCard"
            Layout.bottomMargin: 24
            Form.SettingRow { label: "NVIDIA NVAPI"; sublabel: "Enable NVIDIA-specific features for supported games."; Controls.ToggleSwitch { accessibleName: "NVIDIA NVAPI"; checked: !!root.proton.enable_nvapi; onToggled: checked => root.setOption("enable_nvapi", checked) } }
            Form.SettingRow { label: "HDR path"; sublabel: "Enable DXVK HDR for an HDR-ready display setup."; Controls.ToggleSwitch { accessibleName: "HDR path"; checked: !!root.proton.dxvk_hdr; onToggled: checked => root.setOption("dxvk_hdr", checked) } }
            Form.SettingRow { label: "Fullscreen FSR"; sublabel: "Upscale games rendered below native resolution."; Controls.ToggleSwitch { accessibleName: "Fullscreen FSR"; checked: !!root.proton.fsr; onToggled: checked => root.setOption("fsr", checked) } }
            Form.SettingRow {
                label: "FSR sharpness"
                sublabel: "Lower values are sharper; used when Fullscreen FSR is on."
                Controls.SelectButton {
                    objectName: "fsrSharpnessSelect"
                    width: 132
                    options: root.sharpness
                    selectedIndex: Math.max(0, Number(root.proton.fsr_strength || 0))
                    label: root.sharpness[Math.max(0, Math.min(5, Number(root.proton.fsr_strength || 0)))]
                    onSelected: index => root.setOption("fsr_strength", index)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "ADVANCED COMPATIBILITY"; Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonAdvancedCard"
            Form.SettingRow { label: "Hide NVIDIA GPU"; sublabel: "Compatibility option for games that select the wrong GPU."; Controls.ToggleSwitch { accessibleName: "Hide NVIDIA GPU"; checked: !!root.proton.hide_nvidia_gpu; onToggled: checked => root.setOption("hide_nvidia_gpu", checked) } }
            Form.SettingRow { label: "WineD3D fallback"; sublabel: "Use WineD3D instead of DXVK when rendering fails."; Controls.ToggleSwitch { accessibleName: "WineD3D fallback"; checked: !!root.proton.use_wined3d; onToggled: checked => root.setOption("use_wined3d", checked) } }
            Form.SettingRow { label: "Force VKD3D DXR"; sublabel: "Force the DirectX ray tracing path for a specific game."; Controls.ToggleSwitch { accessibleName: "Force VKD3D DXR"; checked: !!root.proton.vkd3d_dxr; onToggled: checked => root.setOption("vkd3d_dxr", checked) } }
            Form.SettingRow { label: "Legacy DXVK Async"; sublabel: "Non-default flag for older async-patched DXVK builds."; Controls.ToggleSwitch { accessibleName: "Legacy DXVK Async"; checked: !!root.proton.dxvk_async; onToggled: checked => root.setOption("dxvk_async", checked) } }
            Form.SettingRow {
                label: "Custom environment"
                sublabel: "Space-separated KEY=VALUE pairs for one-off fixes."
                Controls.FormTextField {
                    objectName: "protonCustomEnvironmentField"
                    Accessible.name: "Custom environment"
                    implicitWidth: 158
                    text: root.proton.custom_env || ""
                    onEditingFinished: root.setOption("custom_env", text)
                }
            }
            Form.SettingRow {
                label: "Custom prefix"
                sublabel: "Optional command inserted before the game command."
                Controls.FormTextField {
                    objectName: "protonCustomPrefixField"
                    Accessible.name: "Custom prefix"
                    implicitWidth: 158
                    placeholderText: "Optional command"
                    text: root.proton.custom_prefix || ""
                    onEditingFinished: root.setOption("custom_prefix", text)
                }
                isLast: true
            }
        }
    }
}
