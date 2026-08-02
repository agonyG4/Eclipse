import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/form" as Form

Form.ScrollPage {
    id: root
    objectName: "compositorPage"

    maxWidth: 900

    property bool animationsEnabled: true
    property bool blurEnabled: true
    property bool shadowsEnabled: true
    property int vrrModeIndex: 0
    property int tearingPolicyIndex: 0
    property bool directScanoutEnabled: true
    property int tripleBufferingModeIndex: 0
    property bool xwaylandEnabled: true
    property int hardwareCursorModeIndex: 0

    readonly property var vrrOptions: [
        I18n.tr("apps.settings.pages.system.compositor.option.automatic", "Automatic"),
        I18n.tr("apps.settings.pages.system.compositor.option.always_enabled", "Always enabled"),
        I18n.tr("apps.settings.pages.system.compositor.option.disabled", "Disabled")
    ]
    readonly property var tearingPolicyOptions: [
        I18n.tr("apps.settings.pages.system.compositor.option.automatic", "Automatic"),
        I18n.tr("apps.settings.pages.system.compositor.option.fullscreen_games", "Fullscreen games"),
        I18n.tr("apps.settings.pages.system.compositor.option.disabled", "Disabled")
    ]
    readonly property var tripleBufferingOptions: [
        I18n.tr("apps.settings.pages.system.compositor.option.automatic", "Automatic"),
        I18n.tr("apps.settings.pages.system.compositor.option.enabled", "Enabled"),
        I18n.tr("apps.settings.pages.system.compositor.option.disabled", "Disabled")
    ]
    readonly property var hardwareCursorOptions: [
        I18n.tr("apps.settings.pages.system.compositor.option.automatic", "Automatic"),
        I18n.tr("apps.settings.pages.system.compositor.option.hardware", "Hardware"),
        I18n.tr("apps.settings.pages.system.compositor.option.software", "Software")
    ]

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.compositor.text.compositor", "COMPOSITOR")
        }

        Form.FormCard {
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.astrea_compositor", "Astrea compositor")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.configuration_preview", "Configuration preview. These controls are not connected to the compositor yet.")
                isLast: true

                Text {
                    text: I18n.tr("apps.settings.pages.system.compositor.status.preview", "Preview")
                    color: Components.Theme.accent
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    font.weight: Components.Theme.fontWeightMedium
                }
            }
        }

        Form.SectionHeader {
            Layout.topMargin: Components.Theme.spacingXLarge
            text: I18n.tr("apps.settings.pages.system.compositor.text.visual_effects", "VISUAL EFFECTS")
        }

        Form.FormCard {
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.animations", "Animations")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.animations", "Animate window movement, workspace transitions, and compositor effects")

                Form.ToggleSwitch {
                    checked: root.animationsEnabled
                    onToggled: targetChecked => root.animationsEnabled = targetChecked
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.background_blur", "Background blur")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.background_blur", "Blur transparent application and shell surfaces")

                Form.ToggleSwitch {
                    checked: root.blurEnabled
                    onToggled: targetChecked => root.blurEnabled = targetChecked
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.window_shadows", "Window shadows")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.window_shadows", "Draw shadows around floating and layered surfaces")
                isLast: true

                Form.ToggleSwitch {
                    checked: root.shadowsEnabled
                    onToggled: targetChecked => root.shadowsEnabled = targetChecked
                }
            }
        }

        Form.SectionHeader {
            Layout.topMargin: Components.Theme.spacingXLarge
            text: I18n.tr("apps.settings.pages.system.compositor.text.presentation", "PRESENTATION")
        }

        Form.FormCard {
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.variable_refresh_rate", "Variable refresh rate")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.variable_refresh_rate", "Control when compatible displays may use adaptive refresh")

                Form.SelectButton {
                    width: 180
                    label: root.vrrOptions[root.vrrModeIndex]
                    options: root.vrrOptions
                    selectedIndex: root.vrrModeIndex
                    onSelected: index => root.vrrModeIndex = index
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.tearing_policy", "Tearing policy")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.tearing_policy", "Allow immediate presentation for latency-sensitive fullscreen applications")

                Form.SelectButton {
                    width: 180
                    label: root.tearingPolicyOptions[root.tearingPolicyIndex]
                    options: root.tearingPolicyOptions
                    selectedIndex: root.tearingPolicyIndex
                    onSelected: index => root.tearingPolicyIndex = index
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.direct_scanout", "Direct scanout")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.direct_scanout", "Present eligible fullscreen surfaces directly when possible")

                Form.ToggleSwitch {
                    checked: root.directScanoutEnabled
                    onToggled: targetChecked => root.directScanoutEnabled = targetChecked
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.triple_buffering", "Triple buffering")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.triple_buffering", "Select the compositor frame-buffering policy")
                isLast: true

                Form.SelectButton {
                    width: 180
                    label: root.tripleBufferingOptions[root.tripleBufferingModeIndex]
                    options: root.tripleBufferingOptions
                    selectedIndex: root.tripleBufferingModeIndex
                    onSelected: index => root.tripleBufferingModeIndex = index
                }
            }
        }

        Form.SectionHeader {
            Layout.topMargin: Components.Theme.spacingXLarge
            text: I18n.tr("apps.settings.pages.system.compositor.text.compatibility", "COMPATIBILITY")
        }

        Form.FormCard {
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.xwayland", "XWayland")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.xwayland", "Allow legacy X11 applications to run through XWayland")

                Form.ToggleSwitch {
                    checked: root.xwaylandEnabled
                    onToggled: targetChecked => root.xwaylandEnabled = targetChecked
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.compositor.label.hardware_cursor", "Hardware cursor")
                sublabel: I18n.tr("apps.settings.pages.system.compositor.sublabel.hardware_cursor", "Choose whether the cursor uses a hardware plane or compositor rendering")
                isLast: true

                Form.SelectButton {
                    width: 180
                    label: root.hardwareCursorOptions[root.hardwareCursorModeIndex]
                    options: root.hardwareCursorOptions
                    selectedIndex: root.hardwareCursorModeIndex
                    onSelected: index => root.hardwareCursorModeIndex = index
                }
            }
        }
    }
}
