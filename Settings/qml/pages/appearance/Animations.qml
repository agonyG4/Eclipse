import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "animationsPage"

    readonly property var controller: SettingsController.animations
    // Speed is a multiplier on duration: above 1x is faster. Fixed stops keep
    // the value readable and stop the thumb from wobbling between neighbours.
    readonly property var slotSpeedStops: [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0]
    readonly property var globalSpeedStops: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0]
    property string expandedSlot: ""
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
            "geometry.hyprland": I18n.tr("apps.settings.pages.animations.effect.geometry_hyprland", "Hyprland Geometry"),
            "minimize.lamp": I18n.tr("apps.settings.pages.animations.effect.lamp", "Lamp"),
            "window.popin": I18n.tr("apps.settings.pages.animations.effect.popin", "Pop In"),
            "window.scale": I18n.tr("apps.settings.pages.animations.effect.scale", "Scale"),
            "window.glide": I18n.tr("apps.settings.pages.animations.effect.glide", "Glide"),
            "minimize.squash": I18n.tr("apps.settings.pages.animations.effect.squash", "Squash"),
            "workspace.slide": I18n.tr("apps.settings.pages.animations.effect.slide", "Slide"),
            "special.slide": I18n.tr("apps.settings.pages.animations.effect.special_drop", "Drop"),
            "special.slide.hyprland": I18n.tr("apps.settings.pages.animations.effect.slide", "Slide")
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
    function unavailableText(slot) {
        const requested = slot.requested || ""
        const unavailable = slot.unavailableEffects || []
        if (!requested || unavailable.indexOf(requested) < 0)
            return ""
        return root.effectLabel(requested) + " — " + I18n.tr("apps.settings.pages.animations.unavailable", "Unavailable")
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

    function nearestStop(stops, speed) {
        let best = 0
        for (let i = 1; i < stops.length; ++i) {
            if (Math.abs(stops[i] - speed) < Math.abs(stops[best] - speed))
                best = i
        }
        return best
    }
    function speedText(speed) {
        if (Math.abs(speed - 1.0) < 0.001)
            return I18n.tr("apps.settings.pages.animations.speed.normal", "Normal")
        const digits = Number.isInteger(speed) ? 0 : Number.isInteger(Math.round(speed * 1000) / 100) ? 1 : 2
        return Number(speed).toLocaleString(Qt.locale(), "f", digits) + "×"
    }
    // The dropdown already names the effect, so the sublabel explains the slot
    // and only adds the speed when it differs from normal.
    function slotSummary(slot, description) {
        const status = root.plannedText(slot) || root.unavailableText(slot)
        if (status)
            return status
        const speed = slot.speed || 1.0
        if ((slot.effective || "none") === "none" || Math.abs(speed - 1.0) < 0.001)
            return description
        return description + " · " + root.speedText(speed)
    }

    component SpeedControl: ColumnLayout {
        id: speedControl
        required property var stops
        property real speed: 1.0
        property bool controlEnabled: true
        property string sliderName: ""
        signal speedEdited(real speed)
        signal released()
        spacing: 2

        Controls.Slider {
            id: speedSlider
            objectName: speedControl.sliderName
            Layout.fillWidth: true
            from: 0
            to: speedControl.stops.length - 1
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
            showEndpointGlyphs: false
            showValueTooltip: false
            enabled: speedControl.controlEnabled
            modelValueEnabled: true
            modelValue: root.nearestStop(speedControl.stops, speedControl.speed)
            onValueEdited: value => speedControl.speedEdited(speedControl.stops[Math.round(value)])
            onPressedChanged: if (!pressed) speedControl.released()
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 5
            Repeater {
                model: speedControl.stops.length
                Rectangle {
                    required property int index
                    readonly property bool isNormal: Math.abs(speedControl.stops[index] - 1.0) < 0.001
                    x: speedSlider.trackStart + speedSlider.trackWidth * index / (speedControl.stops.length - 1) - width / 2
                    width: isNormal ? 2 : 1
                    height: isNormal ? 5 : 4
                    radius: width / 2
                    color: Components.Theme.textTertiary
                    opacity: speedControl.controlEnabled ? (isNormal ? 0.9 : 0.55) : 0.3
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: I18n.tr("apps.settings.pages.animations.speed.slower", "Slower")
                color: Components.Theme.textTertiary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
            }
            Item { Layout.fillWidth: true }
            Text {
                text: I18n.tr("apps.settings.pages.animations.speed.faster", "Faster")
                color: Components.Theme.textTertiary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
            }
        }
    }

    component SlotRow: ColumnLayout {
        id: slotRow
        required property string slotId
        required property string title
        property string description: ""
        property bool last: false
        property int minimumChoices: 1
        property bool wallpaperOption: false
        readonly property var slotData: root.slot(slotId)
        readonly property bool animated: (slotData.effective || "none") !== "none"
        readonly property bool expanded: animated && root.expandedSlot === slotId
        function toggle() { root.expandedSlot = slotRow.expanded ? "" : slotRow.slotId }
        objectName: "animationSlotRow_" + slotId
        Layout.fillWidth: true
        spacing: 0

        Form.SettingRow {
            label: slotRow.title
            sublabel: root.slotSummary(slotRow.slotData, slotRow.description)
            isLast: slotRow.last && !slotRow.expanded
            clickable: slotRow.animated
            onClicked: slotRow.toggle()
            Row {
                spacing: 6
                Controls.SelectButton {
                    width: 180
                    enabled: root.controller.available && !root.controller.busy && (slotRow.slotData.availableEffects || []).length > slotRow.minimumChoices
                    options: root.availableOptions(slotRow.slotData)
                    selectedIndex: root.selectedIndex(slotRow.slotData)
                    label: selectedIndex >= 0 && selectedIndex < options.length ? options[selectedIndex] : root.effectLabel(slotRow.slotData.effective)
                    onSelected: index => root.controller.setSlotEffect(slotRow.slotId, slotRow.slotData.availableEffects[index])
                }
                Rectangle {
                    id: speedToggle
                    objectName: "animationSpeedToggle_" + slotRow.slotId
                    width: 32
                    height: 32
                    anchors.verticalCenter: parent.verticalCenter
                    radius: Components.Theme.controlRadius
                    color: speedToggleMouse.containsMouse && slotRow.animated
                        ? Components.Theme.controlHoverFill : Components.Theme.cardBg
                    border.width: 1
                    border.color: Components.Theme.cardBorder
                    opacity: slotRow.animated ? 1 : Components.Theme.opacityDisabled
                    Accessible.role: Accessible.Button
                    Accessible.name: I18n.tr("apps.settings.pages.animations.action.speed_settings", "Speed settings")

                    Text {
                        anchors.centerIn: parent
                        text: "\uf078"
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.iconFontFamily
                        font.pixelSize: 11
                        rotation: slotRow.expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic } }
                    }
                    MouseArea {
                        id: speedToggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: slotRow.animated
                        cursorShape: Qt.PointingHandCursor
                        onClicked: slotRow.toggle()
                    }
                    Controls.HoverTooltip {
                        targetItem: speedToggle
                        requested: speedToggleMouse.containsMouse && slotRow.animated
                        label: speedToggle.Accessible.name
                    }
                }
            }
        }

        Item {
            objectName: "animationSpeedPanel_" + slotRow.slotId
            Layout.fillWidth: true
            implicitHeight: slotRow.expanded ? panelColumn.implicitHeight + Components.Theme.spacingMedium * 2 : 0
            clip: true
            Behavior on implicitHeight { NumberAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: panelColumn
                anchors {
                    left: parent.left; right: parent.right; top: parent.top
                    leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge
                    topMargin: Components.Theme.spacingMedium
                }
                spacing: Components.Theme.spacingMedium
                opacity: slotRow.expanded ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Components.Theme.animationQuick } }

                RowLayout {
                    id: speedPanel
                    Layout.fillWidth: true
                    spacing: Components.Theme.spacingLarge

                    Text {
                        Layout.preferredWidth: 72
                        Layout.alignment: Qt.AlignTop
                        topPadding: 6
                        text: I18n.tr("apps.settings.pages.animations.label.slot_speed", "Speed")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("body").family
                        font.pixelSize: Components.Theme.typography.role("body").pixelSize
                        lineHeight: Components.Theme.typography.role("body").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                    }
                    SpeedControl {
                        Layout.fillWidth: true
                        sliderName: "animationSpeedSlider_" + slotRow.slotId
                        stops: root.slotSpeedStops
                        speed: slotRow.slotData.speed || 1.0
                        controlEnabled: root.controller.available
                        onSpeedEdited: speed => root.controller.setSlotSpeed(slotRow.slotId, speed)
                        onReleased: root.controller.flush()
                    }
                    Text {
                        Layout.preferredWidth: 64
                        Layout.alignment: Qt.AlignTop
                        topPadding: 6
                        horizontalAlignment: Text.AlignRight
                        text: root.speedText(slotRow.slotData.speed || 1.0)
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("body").family
                        font.pixelSize: Components.Theme.typography.role("body").pixelSize
                        lineHeight: Components.Theme.typography.role("body").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                        font.features: { "tnum": 1 }
                    }
                }

                RowLayout {
                    objectName: "animationWallpaperOption_" + slotRow.slotId
                    // Only the plain slide moves workspaces side by side, so only it can carry the wallpaper.
                    visible: slotRow.wallpaperOption && slotRow.slotData.effective === "workspace.slide"
                    Layout.fillWidth: true
                    spacing: Components.Theme.spacingLarge

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: I18n.tr("apps.settings.pages.animations.slide_wallpaper.label", "Slide wallpaper with workspaces")
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.typography.role("body").family
                            font.pixelSize: Components.Theme.typography.role("body").pixelSize
                            lineHeight: Components.Theme.typography.role("body").lineHeight
                            lineHeightMode: Text.ProportionalHeight
                            font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: I18n.tr("apps.settings.pages.animations.slide_wallpaper.description", "The wallpaper moves along with each workspace instead of staying still.")
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.typography.role("caption").family
                            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                            lineHeight: Components.Theme.typography.role("caption").lineHeight
                            lineHeightMode: Text.ProportionalHeight
                            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                            wrapMode: Text.WordWrap
                        }
                    }
                    Controls.ToggleSwitch {
                        objectName: "slideWallpaperToggle"
                        accessibleName: I18n.tr("apps.settings.pages.animations.slide_wallpaper.label", "Slide wallpaper with workspaces")
                        Layout.alignment: Qt.AlignVCenter
                        enabled: root.controller.available && !root.controller.busy
                        checked: root.controller.slideWallpaper
                        onToggled: targetChecked => root.controller.setSlideWallpaper(targetChecked)
                    }
                }
            }

            Rectangle {
                visible: !slotRow.last
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: Components.Theme.spacingXLarge }
                height: 1
                color: Components.Theme.cardBorder
            }
        }
    }

    Component.onCompleted: root.controller.refresh()

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("apps.settings.pages.animations.title", "Animations")
            subtitle: I18n.tr("apps.settings.pages.animations.subtitle", "Choose how windows and workspaces move.")
        }

        Text {
            objectName: "animationsError"
            visible: root.controller.lastError !== ""
            text: root.controller.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 10
        }

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
                    options: root.controller.presets.map(id => id === "astrea" ? I18n.tr("apps.settings.pages.animations.preset.astrea", "Astrea") : id === "kde" ? I18n.tr("apps.settings.pages.animations.preset.kde", "KDE") : id === "hyprland" ? I18n.tr("apps.settings.pages.animations.preset.hyprland", "Hyprland") : I18n.tr("apps.settings.pages.animations.preset.macos", "macOS"))
                    selectedIndex: Math.max(0, root.controller.presets.indexOf(root.controller.preset))
                    label: selectedIndex >= 0 && selectedIndex < options.length ? options[selectedIndex] : I18n.tr("apps.settings.pages.animations.unavailable", "Unavailable")
                    onSelected: index => root.controller.setPreset(root.controller.presets[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.speed", "Animation speed")
                sublabel: root.speedText(root.controller.speed) + " — " + I18n.tr("apps.settings.pages.animations.sublabel.speed_all", "Applies to every animation")
                SpeedControl {
                    width: 300
                    stops: root.globalSpeedStops
                    speed: root.controller.speed
                    controlEnabled: root.controller.available
                    onSpeedEdited: speed => root.controller.setSpeed(speed)
                    onReleased: root.controller.flush()
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.label.reset", "Reset customizations")
                sublabel: I18n.tr("apps.settings.pages.animations.sublabel.reset", "Restore the preset's effects and speeds")
                clickable: root.controller.available && !root.controller.busy
                onClicked: root.controller.resetOverrides()
                Controls.ButtonCapsule {
                    objectName: "animationsResetButton"
                    label: I18n.tr("apps.settings.pages.animations.action.reset", "Reset")
                    enabled: root.controller.available && !root.controller.busy && root.controller.hasOverrides
                    onClicked: root.controller.resetOverrides()
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.windows", "WINDOWS"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.animations.maximized_open.label", "Animate maximized window opening")
                sublabel: I18n.tr("apps.settings.pages.animations.maximized_open.description", "When disabled, windows that open maximized appear immediately at their final size instead of playing the Window Open effect.")
                Controls.ToggleSwitch {
                    objectName: "animateMaximizedWindowOpenToggle"
                    accessibleName: I18n.tr("apps.settings.pages.animations.maximized_open.label", "Animate maximized window opening")
                    enabled: root.controller.available && !root.controller.busy
                    checked: root.controller.animateMaximizedWindowOpen
                    onToggled: targetChecked => root.controller.setAnimateMaximizedWindowOpen(targetChecked)
                }
            }
            Repeater {
                model: [
                    { id: "window.open", description: I18n.tr("apps.settings.pages.animations.slot.open_description", "Effect when a window appears"), title: I18n.tr("apps.settings.pages.animations.slot.open", "Open") },
                    { id: "window.close", description: I18n.tr("apps.settings.pages.animations.slot.close_description", "Effect when a window closes"), title: I18n.tr("apps.settings.pages.animations.slot.close", "Close") },
                    { id: "window.minimize", description: I18n.tr("apps.settings.pages.animations.slot.minimize_description", "Effect when a window is sent to the Dock"), title: I18n.tr("apps.settings.pages.animations.slot.minimize", "Minimize") },
                    { id: "window.restore", description: I18n.tr("apps.settings.pages.animations.slot.restore_description", "Effect when a minimized window returns"), title: I18n.tr("apps.settings.pages.animations.slot.restore", "Restore") }
                ]
                delegate: SlotRow {
                    required property var modelData
                    required property int index
                    slotId: modelData.id
                    title: modelData.title
                    description: modelData.description || ""
                    last: index === 3
                    minimumChoices: 0
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.window_management", "WINDOW MANAGEMENT"); Layout.bottomMargin: 12 }
        Form.FormCard {
            Layout.bottomMargin: 24
            Repeater {
                model: [
                    { id: "window.move", description: I18n.tr("apps.settings.pages.animations.slot.movement_description", "How windows follow when you drag them"), title: I18n.tr("apps.settings.pages.animations.slot.movement", "Movement") },
                    { id: "window.resize", description: I18n.tr("apps.settings.pages.animations.slot.resize_description", "How windows change size"), title: I18n.tr("apps.settings.pages.animations.slot.resize", "Resize") },
                    { id: "layout.reflow", description: I18n.tr("apps.settings.pages.animations.slot.reflow_description", "How tiled windows rearrange themselves"), title: I18n.tr("apps.settings.pages.animations.slot.reflow", "Tiled layout") },
                    { id: "window.maximize", description: I18n.tr("apps.settings.pages.animations.slot.maximize_description", "Effect when a window fills the screen"), title: I18n.tr("apps.settings.pages.animations.slot.maximize", "Maximize") },
                    { id: "window.fullscreen", description: I18n.tr("apps.settings.pages.animations.slot.fullscreen_description", "Effect when a window enters or leaves fullscreen"), title: I18n.tr("apps.settings.pages.animations.slot.fullscreen", "Fullscreen") }
                ]
                delegate: SlotRow {
                    required property var modelData
                    required property int index
                    slotId: modelData.id
                    title: modelData.title
                    description: modelData.description || ""
                    last: index === 4
                    minimumChoices: 0
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("apps.settings.pages.animations.workspaces", "WORKSPACES"); Layout.bottomMargin: 12 }
        Form.FormCard {
            SlotRow {
                slotId: "workspace.switch"
                description: I18n.tr("apps.settings.pages.animations.slot.switch_workspace_description", "Effect when you move between workspaces")
                title: I18n.tr("apps.settings.pages.animations.slot.switch_workspace", "Switch workspace")
                minimumChoices: 1
                wallpaperOption: true
            }
            Repeater {
                model: [
                    { id: "workspace.special", description: I18n.tr("apps.settings.pages.animations.slot.special_workspace_description", "Effect when the special workspace slides in or out"), title: I18n.tr("apps.settings.pages.animations.slot.special_workspace", "Special workspace") },
                    { id: "workspace.window-move", description: I18n.tr("apps.settings.pages.animations.slot.move_workspace_description", "Effect when a window moves to another workspace"), title: I18n.tr("apps.settings.pages.animations.slot.move_workspace", "Move window between workspaces") }
                ]
                delegate: SlotRow {
                    required property var modelData
                    required property int index
                    slotId: modelData.id
                    title: modelData.title
                    description: modelData.description || ""
                    last: index === 1
                    minimumChoices: 1
                }
            }
        }
    }
}
