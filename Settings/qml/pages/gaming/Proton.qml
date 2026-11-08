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
    readonly property var presets: [I18n.tr("settings.proton.recommended", "Recommended"), I18n.tr("settings.proton.nvidia_features", "NVIDIA features"), I18n.tr("settings.proton.hdr_display", "HDR display"), I18n.tr("settings.proton.diagnostics", "Diagnostics")]
    readonly property string currentPreset: root.proton.preset || "recommended"
    readonly property int presetIndex: Math.max(0, root.presetIds.indexOf(root.currentPreset))
    readonly property bool customPreset: root.currentPreset === "custom"
    readonly property var syncModes: [I18n.tr("settings.proton.proton_default", "Proton default"), I18n.tr("settings.proton.disable_esync", "Disable Esync"), I18n.tr("settings.proton.disable_fsync", "Disable Fsync"), I18n.tr("settings.proton.disable_both", "Disable both")]
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
        return root.status[key] || I18n.tr("settings.proton.path_is_not_available", "Path is not available")
    }
    function activeFlags() {
        const flags = []
        if (root.proton.gamemode) flags.push("GameMode")
        if (root.proton.mangohud) flags.push("MangoHud")
        if (root.proton.gamescope) flags.push("Gamescope")
        return flags.length ? flags.join(" · ") : I18n.tr("settings.proton.no_runtime_wrappers_enabled", "No runtime wrappers enabled")
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
            subtitle: I18n.tr("settings.proton.how_steam_games_launch_through_proton", "How Steam games launch through Proton.")
        }

        Components.BackendNotice {
            objectName: "protonBackendStatus"
            retryObjectName: "protonRetryRefreshButton"
            busy: root.controller.busy
            error: root.controller.lastError
            busyText: I18n.tr("settings.proton.saving_proton_settings", "Saving Proton settings…")
            onRetry: root.controller.refresh()
        }

        Form.SectionHeader { text: I18n.tr("settings.proton.setup", "SETUP"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonSetupCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "steamLaunchOptionRow"
                label: I18n.tr("settings.proton.steam_launch_option", "Steam launch option")
                sublabel: root.launcher.available ? I18n.tr("settings.proton.copy_this_command_into_steam_s_launch_option", "Copy this command into Steam's launch options.") : I18n.tr("settings.proton.the_astrea_gaming_launcher_is_unavailable", "The astrea-gaming launcher is unavailable.")
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
                    label: I18n.tr("settings.proton.copy", "Copy")
                    enabled: !!root.launcher.available
                    onClicked: SettingsController.copyGamingLaunchCommand()
                }
            }
            Form.SettingRow { label: I18n.tr("settings.proton.active_flags", "Active flags"); sublabel: root.activeFlags() }
            Form.SettingRow { label: I18n.tr("settings.proton.command_prefix", "Command prefix"); sublabel: (root.status.proton_prefix || []).join(" ") || I18n.tr("settings.proton.no_command_prefix", "No command prefix"); sublabelTextFormat: Text.PlainText }
            Form.SettingRow { label: I18n.tr("settings.proton.wrapper", "Wrapper"); sublabel: root.launcher.path || I18n.tr("settings.proton.launcher_is_not_installed", "Launcher is not installed"); sublabelTextFormat: Text.PlainText; isLast: true }
        }

        Form.SectionHeader { text: I18n.tr("settings.proton.profile", "PROFILE"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonProfileCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "protonPresetRow"
                label: I18n.tr("settings.proton.proton_preset", "Proton preset")
                sublabel: root.customPreset ? I18n.tr("settings.proton.custom_settings_are_active_choose_a_preset_t", "Custom settings are active; choose a preset to replace them.") : I18n.tr("settings.proton.profiles_for_common_game_compatibility_needs", "Profiles for common game compatibility needs.")
                Controls.SelectButton {
                    objectName: "protonPresetSelect"
                    width: 142
                    options: root.presets
                    selectedIndex: root.presetIndex
                    label: root.customPreset ? I18n.tr("settings.proton.custom", "Custom") : root.presets[root.presetIndex]
                    onSelected: index => root.setOption("preset", root.presetIds[index])
                }
            }
            Form.SettingRow {
                objectName: "syncBehaviorRow"
                label: I18n.tr("settings.proton.sync_behavior", "Sync behavior")
                sublabel: I18n.tr("settings.proton.keep_proton_defaults_unless_a_game_needs_esy", "Keep Proton defaults unless a game needs Esync or Fsync disabled.")
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

        Form.SectionHeader { text: I18n.tr("settings.proton.runtime", "RUNTIME"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonRuntimeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "protonGamemodeRow"
                label: "GameMode"
                sublabel: I18n.tr("settings.proton.allow_games_to_request_performance_hints", "Allow games to request performance hints.")
                Controls.ToggleSwitch { accessibleName: "GameMode"; checked: !!root.proton.gamemode; enabled: !!root.caps.gamemode; onToggled: checked => root.setOption("gamemode", checked) }
            }
            Form.SettingRow {
                objectName: "mangohudRow"
                label: "MangoHud"
                sublabel: I18n.tr("settings.proton.show_frame_rate_frame_time_and_hardware_tele", "Show frame rate, frame time and hardware telemetry.")
                Controls.ToggleSwitch { accessibleName: "MangoHud"; checked: !!root.proton.mangohud; enabled: !!root.caps.mangohud; onToggled: checked => root.setOption("mangohud", checked) }
            }
            Form.SettingRow {
                objectName: "protonGamescopeRow"
                label: "Gamescope"
                sublabel: I18n.tr("settings.proton.wrap_the_game_with_the_gamescope_options_bel", "Wrap the game with the Gamescope options below.")
                Controls.ToggleSwitch { accessibleName: "Gamescope"; checked: !!root.proton.gamescope; enabled: !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope", checked) }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.proton.gamescope", "GAMESCOPE"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "gamescopeCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "steamGamescopeProfileRow"
                label: I18n.tr("settings.proton.use_steamos_profile", "Use SteamOS profile")
                sublabel: root.manualGamescope ? I18n.tr("settings.proton.use_manually_configured_gamescope_values_bel", "Use manually configured Gamescope values below.") : I18n.tr("settings.proton.follow_the_gamescope_profile_configured_for", "Follow the Gamescope profile configured for this Proton wrapper.")
                Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.use_steamos_profile", "Use SteamOS profile"); checked: !!root.proton.use_gamescope_profile; enabled: !!root.caps.gamescope; onToggled: checked => root.setOption("use_gamescope_profile", checked) }
            }
            Form.SettingRow {
                label: I18n.tr("settings.proton.width", "Width")
                sublabel: I18n.tr("settings.proton.nested_gamescope_output_width", "Nested Gamescope output width.")
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
                label: I18n.tr("settings.proton.height", "Height")
                sublabel: I18n.tr("settings.proton.nested_gamescope_output_height", "Nested Gamescope output height.")
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
                label: I18n.tr("settings.proton.refresh_rate", "Refresh rate")
                sublabel: I18n.tr("settings.proton.manual_nested_gamescope_refresh_limit", "Manual nested Gamescope refresh limit.")
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
            Form.SettingRow { label: I18n.tr("settings.proton.fullscreen", "Fullscreen"); sublabel: I18n.tr("settings.proton.start_the_nested_session_in_fullscreen", "Start the nested session in fullscreen."); enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.fullscreen", "Fullscreen"); checked: !!root.proton.gamescope_fullscreen; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_fullscreen", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.immediate_flips", "Immediate flips"); sublabel: I18n.tr("settings.proton.reduce_latency_on_display_paths_that_support", "Reduce latency on display paths that support it."); enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.immediate_flips", "Immediate flips"); checked: !!root.proton.gamescope_immediate_flips; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_immediate_flips", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.hide_cursor", "Hide cursor"); sublabel: I18n.tr("settings.proton.keep_the_pointer_hidden_for_controller_first", "Keep the pointer hidden for controller-first games."); enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.hide_cursor", "Hide cursor"); checked: !!root.proton.gamescope_hide_cursor; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_hide_cursor", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.force_cursor_grab", "Force cursor grab"); sublabel: I18n.tr("settings.proton.keep_relative_mouse_input_inside_the_game", "Keep relative mouse input inside the game."); enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.force_cursor_grab", "Force cursor grab"); checked: !!root.proton.gamescope_force_grab_cursor; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_force_grab_cursor", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.adaptive_sync", "Adaptive sync"); sublabel: I18n.tr("settings.proton.request_variable_refresh_when_the_display_su", "Request variable refresh when the display supports it."); enabled: root.manualGamescope; Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.adaptive_sync", "Adaptive sync"); checked: !!root.proton.gamescope_adaptive_sync; enabled: root.manualGamescope && !!root.caps.gamescope; onToggled: checked => root.setOption("gamescope_adaptive_sync", checked) } }
            Form.SettingRow {
                label: I18n.tr("settings.proton.extra_arguments", "Extra arguments")
                sublabel: I18n.tr("settings.proton.additional_gamescope_flags_after_generated_r", "Additional Gamescope flags after generated resolution values.")
                Controls.FormTextField {
                    objectName: "gamescopeExtraArgumentsField"
                    Accessible.name: "Extra arguments"
                    enabled: root.manualGamescope
                    implicitWidth: 130
                    placeholderText: I18n.tr("settings.proton.optional_flags", "Optional flags")
                    text: root.proton.gamescope_extra_args || ""
                    onEditingFinished: root.setOption("gamescope_extra_args", text)
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.proton.graphics_features", "GRAPHICS FEATURES"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonGraphicsCard"
            Layout.bottomMargin: 24
            Form.SettingRow { label: "NVIDIA NVAPI"; sublabel: I18n.tr("settings.proton.enable_nvidia_specific_features_for_supporte", "Enable NVIDIA-specific features for supported games."); Controls.ToggleSwitch { accessibleName: "NVIDIA NVAPI"; checked: !!root.proton.enable_nvapi; onToggled: checked => root.setOption("enable_nvapi", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.hdr_path", "HDR path"); sublabel: I18n.tr("settings.proton.enable_dxvk_hdr_for_an_hdr_ready_display_set", "Enable DXVK HDR for an HDR-ready display setup."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.hdr_path", "HDR path"); checked: !!root.proton.dxvk_hdr; onToggled: checked => root.setOption("dxvk_hdr", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.fullscreen_fsr", "Fullscreen FSR"); sublabel: I18n.tr("settings.proton.upscale_games_rendered_below_native_resoluti", "Upscale games rendered below native resolution."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.fullscreen_fsr", "Fullscreen FSR"); checked: !!root.proton.fsr; onToggled: checked => root.setOption("fsr", checked) } }
            Form.SettingRow {
                label: I18n.tr("settings.proton.fsr_sharpness", "FSR sharpness")
                sublabel: I18n.tr("settings.proton.lower_values_are_sharper_used_when_fullscree", "Lower values are sharper; used when Fullscreen FSR is on.")
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

        Form.SectionHeader { text: I18n.tr("settings.proton.advanced_compatibility", "ADVANCED COMPATIBILITY"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "protonAdvancedCard"
            Form.SettingRow { label: I18n.tr("settings.proton.hide_nvidia_gpu", "Hide NVIDIA GPU"); sublabel: I18n.tr("settings.proton.compatibility_option_for_games_that_select_t", "Compatibility option for games that select the wrong GPU."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.hide_nvidia_gpu", "Hide NVIDIA GPU"); checked: !!root.proton.hide_nvidia_gpu; onToggled: checked => root.setOption("hide_nvidia_gpu", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.wined3d_fallback", "WineD3D fallback"); sublabel: I18n.tr("settings.proton.use_wined3d_instead_of_dxvk_when_rendering_f", "Use WineD3D instead of DXVK when rendering fails."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.wined3d_fallback", "WineD3D fallback"); checked: !!root.proton.use_wined3d; onToggled: checked => root.setOption("use_wined3d", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.force_vkd3d_dxr", "Force VKD3D DXR"); sublabel: I18n.tr("settings.proton.force_the_directx_ray_tracing_path_for_a_spe", "Force the DirectX ray tracing path for a specific game."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.force_vkd3d_dxr", "Force VKD3D DXR"); checked: !!root.proton.vkd3d_dxr; onToggled: checked => root.setOption("vkd3d_dxr", checked) } }
            Form.SettingRow { label: I18n.tr("settings.proton.legacy_dxvk_async", "Legacy DXVK Async"); sublabel: I18n.tr("settings.proton.non_default_flag_for_older_async_patched_dxv", "Non-default flag for older async-patched DXVK builds."); Controls.ToggleSwitch { accessibleName: I18n.tr("settings.proton.legacy_dxvk_async", "Legacy DXVK Async"); checked: !!root.proton.dxvk_async; onToggled: checked => root.setOption("dxvk_async", checked) } }
            Form.SettingRow {
                label: I18n.tr("settings.proton.custom_environment", "Custom environment")
                sublabel: I18n.tr("settings.proton.space_separated_key_value_pairs_for_one_off", "Space-separated KEY=VALUE pairs for one-off fixes.")
                Controls.FormTextField {
                    objectName: "protonCustomEnvironmentField"
                    Accessible.name: "Custom environment"
                    implicitWidth: 158
                    text: root.proton.custom_env || ""
                    onEditingFinished: root.setOption("custom_env", text)
                }
            }
            Form.SettingRow {
                label: I18n.tr("settings.proton.custom_prefix", "Custom prefix")
                sublabel: I18n.tr("settings.proton.optional_command_inserted_before_the_game_co", "Optional command inserted before the game command.")
                Controls.FormTextField {
                    objectName: "protonCustomPrefixField"
                    Accessible.name: "Custom prefix"
                    implicitWidth: 158
                    placeholderText: I18n.tr("settings.proton.optional_command", "Optional command")
                    text: root.proton.custom_prefix || ""
                    onEditingFinished: root.setOption("custom_prefix", text)
                }
                isLast: true
            }
        }
    }
}
