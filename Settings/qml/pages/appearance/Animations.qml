import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "animationsPage"

    readonly property var controller: SettingsController.animations
    readonly property var slotById: {
        const result = {}
        for (const slot of root.controller.slots)
            result[slot.id] = slot
        return result
    }

    function slot(id) { return root.slotById[id] || ({}) }
    function effectLabel(id) {
        const labels = {
            "none": I18n.tr("apps.settings.pages.animations.effect.none", "None"),
            "geometry.kde": I18n.tr("apps.settings.pages.animations.effect.geometry_kde", "KDE Geometry"),
            "geometry.macos": I18n.tr("apps.settings.pages.animations.effect.geometry_macos", "macOS Spring"),
            "minimize.lamp": I18n.tr("apps.settings.pages.animations.effect.lamp", "Lamp"),
            "window.scale": I18n.tr("apps.settings.pages.animations.effect.scale", "Scale"),
            "window.glide": I18n.tr("apps.settings.pages.animations.effect.glide", "Glide"),
            "minimize.squash": I18n.tr("apps.settings.pages.animations.effect.squash", "Squash"),
            "workspace.slide": I18n.tr("apps.settings.pages.animations.effect.slide", "Slide")
        }
        return labels[id] || id || I18n.tr("apps.settings.pages.animations.effect.unavailable", "Unavailable")
    }
    function plannedText(slot) {
        const requested = slot.requested || ""
        const planned = slot.plannedEffects || []
        if (!requested || planned.indexOf(requested) < 0)
            return ""
        return root.effectLabel(requested) + " — " + I18n.tr("apps.settings.pages.animations.planned", "Planned")
    }
    function availableOptions(slot) {
        return (slot.availableEffects || []).map(effect => root.effectLabel(effect))
    }
    function selectedIndex(slot) {
        const values = slot.availableEffects || []
        const selected = slot.override || slot.requested || "none"
        const index = values.indexOf(selected)
        return index >= 0 ? index : Math.max(0, values.indexOf("none"))
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.animations.general", "GENERAL")
            Layout.bottomMargin: 12
        }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.animations", "Animations")
                sublabel: root.controller.available
                    ? I18n.tr("apps.settings.pages.animations.sublabel.enabled", "Use compositor transitions for new window changes")
                    : I18n.tr("apps.settings.pages.animations.sublabel.unavailable", "Compositor unavailable")
                Controls.ToggleSwitch {
                    enabled: root.controller.available && !root.controller.busy
                    checked: root.controller.enabled
                    onToggled: targetChecked => root.controller.setEnabled(targetChecked)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.preset", "Preset")
                sublabel: root.controller.hasOverrides
                    ? I18n.tr("apps.settings.pages.animations.sublabel.customized", "Customized with slot overrides")
                    : I18n.tr("apps.settings.pages.animations.sublabel.preset", "Choose the base motion family")
                Controls.SelectButton {
                    width: 180
                    enabled: root.controller.available && !root.controller.busy
                    options: root.controller.presets.map(id => id === "astrea" ? I18n.tr("apps.settings.pages.animations.preset.astrea", "Astrea") : id === "kde" ? I18n.tr("apps.settings.pages.animations.preset.kde", "KDE") : I18n.tr("apps.settings.pages.animations.preset.macos", "macOS"))
                    selectedIndex: Math.max(0, root.controller.presets.indexOf(root.controller.preset))
                    label: selectedIndex >= 0 && selectedIndex < options.length ? options[selectedIndex] : I18n.tr("apps.settings.pages.animations.unavailable", "Unavailable")
                    onSelected: index => root.controller.setPreset(root.controller.presets[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.speed", "Animation speed")
                sublabel: I18n.tr("apps.settings.pages.animations.sublabel.speed", "Scale the duration and spring time domain")
                Controls.Slider {
                    width: 260
                    from: 0.5
                    to: 2.0
                    stepSize: 0.05
                    enabled: root.controller.available && !root.controller.busy
                    modelValueEnabled: root.controller.available && !root.controller.busy
                    modelValue: root.controller.speed
                    valueText: Number(displayedValue).toFixed(2) + "×"
                    detentEnabled: true
                    detentValue: 1.0
                    onValueEdited: value => root.controller.setSpeed(value)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.reset", "Reset customizations")
                sublabel: I18n.tr("apps.settings.pages.animations.sublabel.reset", "Clear slot overrides while keeping the selected preset")
                clickable: root.controller.available && !root.controller.busy
                onClicked: root.controller.resetOverrides()
                Text { text: I18n.tr("apps.settings.pages.animations.action.reset", "Reset"); color: Components.Theme.accent; font.pixelSize: Components.Theme.fontSizeSmall }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.windows", "WINDOWS"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Repeater {
                model: ["window.open", "window.close", "window.minimize", "window.restore"]
                delegate: Form.SettingRow {
                    required property string modelData
                    required property int index
                    readonly property var slotData: root.slot(modelData)
                    label: modelData === "window.open" ? I18n.tr("apps.settings.pages.animations.slot.open", "Open") : modelData === "window.close" ? I18n.tr("apps.settings.pages.animations.slot.close", "Close") : modelData === "window.minimize" ? I18n.tr("apps.settings.pages.animations.slot.minimize", "Minimize") : I18n.tr("apps.settings.pages.animations.slot.restore", "Restore")
                    sublabel: root.plannedText(slotData) || root.effectLabel(slotData.effective)
                    isLast: index === 3
                    Controls.SelectButton {
                        width: 180
                        enabled: root.controller.available && !root.controller.busy && slotData.availableEffects && slotData.availableEffects.length > 0
                        options: root.availableOptions(slotData)
                        selectedIndex: root.selectedIndex(slotData)
                        label: selectedIndex >= 0 && selectedIndex < options.length ? options[selectedIndex] : root.effectLabel(slotData.effective)
                        onSelected: index => root.controller.setSlotEffect(modelData, slotData.availableEffects[index])
                    }
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.window_management", "WINDOW MANAGEMENT"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Repeater {
                model: ["window.move", "window.resize", "layout.reflow", "window.maximize", "window.fullscreen"]
                delegate: Form.SettingRow {
                    required property string modelData
                    required property int index
                    readonly property var slotData: root.slot(modelData)
                    label: modelData === "window.move" ? I18n.tr("apps.settings.pages.animations.slot.movement", "Movement") : modelData === "window.resize" ? I18n.tr("apps.settings.pages.animations.slot.resize", "Resize") : modelData === "layout.reflow" ? I18n.tr("apps.settings.pages.animations.slot.reflow", "Tiled layout") : modelData === "window.maximize" ? I18n.tr("apps.settings.pages.animations.slot.maximize", "Maximize") : I18n.tr("apps.settings.pages.animations.slot.fullscreen", "Fullscreen")
                    sublabel: root.effectLabel(slotData.effective)
                    isLast: index === 4
                    Controls.SelectButton {
                        width: 180
                        enabled: root.controller.available && !root.controller.busy
                        options: root.availableOptions(slotData)
                        selectedIndex: root.selectedIndex(slotData)
                        label: selectedIndex >= 0 && selectedIndex < options.length ? options[selectedIndex] : root.effectLabel(slotData.effective)
                        onSelected: index => root.controller.setSlotEffect(modelData, slotData.availableEffects[index])
                    }
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.workspaces", "WORKSPACES"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.slot.switch_workspace", "Switch workspace")
                sublabel: root.effectLabel(root.slot("workspace.switch").effective)
                isLast: false
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.slot.move_workspace", "Move window between workspaces")
                sublabel: root.effectLabel(root.slot("workspace.window-move").effective)
                isLast: true
            }
        }
    }
}
