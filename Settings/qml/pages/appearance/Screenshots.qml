import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "screenshotsPage"

    readonly property var configuration: SettingsController.screenshots
    readonly property var positionIds: ["bottom-right", "bottom-left", "top-right", "top-left"]
    readonly property var positionOptions: [
        I18n.tr("apps.settings.pages.appearance.screenshots.option.bottom_right", "Bottom right"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.bottom_left", "Bottom left"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.top_right", "Top right"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.top_left", "Top left")
    ]
    readonly property var animationIds: ["fade", "scale-fade", "slide-fade"]
    readonly property var animationOptions: [
        I18n.tr("apps.settings.pages.appearance.screenshots.option.fade", "Fade"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.scale_fade", "Scale and fade"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.slide_fade", "Slide and fade")
    ]
    readonly property var easingIds: ["linear", "in-quad", "out-cubic", "in-out-cubic"]
    readonly property var easingOptions: [
        I18n.tr("apps.settings.pages.appearance.screenshots.option.linear", "Linear"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.in_quad", "Ease in"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.out_cubic", "Ease out"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.in_out_cubic", "Ease in and out")
    ]
    readonly property var clickIds: ["preview", "copy", "dismiss"]
    readonly property var clickOptions: [
        I18n.tr("apps.settings.pages.appearance.screenshots.option.preview", "Open preview"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.copy", "Copy image"),
        I18n.tr("apps.settings.pages.appearance.screenshots.option.dismiss", "Dismiss thumbnail")
    ]

    function indexOf(values, value) {
        return Math.max(0, values.indexOf(value))
    }

    function pixels(value) {
        return Math.round(value) + " "
            + I18n.tr("apps.settings.pages.appearance.screenshots.unit.pixels", "px")
    }

    function duration(value) {
        return (value / 1000).toFixed(value % 1000 === 0 ? 0 : 2) + " "
            + I18n.tr("apps.settings.pages.appearance.screenshots.unit.seconds", "s")
    }


    Component.onCompleted: SettingsController.screenshotDemoCommand("start")
    Component.onDestruction: SettingsController.screenshotDemoCommand("stop")
    Timer {
        interval: 4000
        running: true
        repeat: true
        onTriggered: SettingsController.screenshotDemoCommand("renew")
    }

    Form.ScrollPage {
        id: scrollPage
        objectName: "screenshotsScrollPage"
        anchors.fill: parent
        contentMargins: 28
        maxWidth: 900

        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.text.live_preview", "Live desktop preview")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.preview.hint", "Adjust the real screenshot thumbnail on your desktop. It remains visible while this page is open.")
                Controls.Button {
                    objectName: "screenshotAnimationReplay"
                    label: I18n.tr("apps.settings.pages.appearance.screenshots.action.replay", "Replay animation")
                    flat: true
                    onClicked: SettingsController.screenshotDemoCommand("replay")
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.screenshots.text.appearance", "APPEARANCE")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 22

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.corner_radius", "Corner radius")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.corner_radius", "Round the screenshot image and border together")
                Controls.Slider {
                    objectName: "screenshotCornerRadiusSlider"
                    from: 0; to: 64; stepSize: 1
                    modelValueEnabled: true; modelValue: root.configuration.cornerRadius
                    detentEnabled: true
                    detentValue: 10
                    valueText: root.pixels(displayedValue)
                    onValueEdited: value => root.configuration.setCornerRadius(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.border", "Border")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.border", "Show a fine edge around the screenshot")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.border", "Border")
                    checked: root.configuration.borderEnabled
                    onToggled: value => root.configuration.setBorderEnabled(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.border_width", "Border width")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.border_width", "Set the edge thickness")
                Controls.Slider {
                    from: 0; to: 8; stepSize: 1
                    modelValueEnabled: true; modelValue: root.configuration.borderWidth
                    detentEnabled: true
                    detentValue: 1
                    valueText: root.pixels(displayedValue)
                    enabled: root.configuration.borderEnabled
                    onValueEdited: value => root.configuration.setBorderWidth(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.border_color", "Border color")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.border_color", "Choose a color from the current desktop theme")
                RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        radius: 6
                        color: root.configuration.borderColor
                        border.width: 1
                        border.color: Components.Theme.cardBorder
                    }
                    Controls.Button {
                        label: I18n.tr("apps.settings.pages.appearance.screenshots.action.choose_color", "Choose color")
                        controlWidth: 132
                        enabled: root.configuration.borderEnabled
                        onClicked: borderColorDialog.open()
                    }
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.shadow", "Shadow")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.shadow", "Add depth without changing the saved image")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.shadow", "Shadow")
                    checked: root.configuration.shadowEnabled
                    onToggled: value => root.configuration.setShadowEnabled(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.shadow_strength", "Shadow strength")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.shadow_strength", "Adjust the shadow opacity and spread")
                Controls.Slider {
                    from: 0; to: 1; stepSize: 0.05
                    modelValueEnabled: true; modelValue: root.configuration.shadowStrength
                    detentEnabled: true
                    detentValue: 0.55
                    valueText: Math.round(displayedValue * 100) + "%"
                    enabled: root.configuration.shadowEnabled
                    onValueEdited: value => root.configuration.setShadowStrength(value)
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.screenshots.text.size_position", "SIZE AND POSITION")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.thumbnail_width", "Thumbnail width")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.thumbnail_width", "Maximum width while preserving the screenshot aspect ratio")
                Controls.Slider {
                    from: 120; to: 480; stepSize: 8
                    modelValueEnabled: true; modelValue: root.configuration.thumbnailWidth
                    detentEnabled: true
                    detentValue: 240
                    valueText: root.pixels(displayedValue)
                    onValueEdited: value => root.configuration.setThumbnailWidth(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.thumbnail_height", "Thumbnail height")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.thumbnail_height", "Maximum height while preserving the screenshot aspect ratio")
                Controls.Slider {
                    from: 80; to: 320; stepSize: 8
                    modelValueEnabled: true; modelValue: root.configuration.thumbnailHeight
                    detentEnabled: true
                    detentValue: 160
                    valueText: root.pixels(displayedValue)
                    onValueEdited: value => root.configuration.setThumbnailHeight(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.preview_size", "Maximum preview size")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.preview_size", "Limit preview size relative to the display")
                Controls.Slider {
                    from: 0.5; to: 0.9; stepSize: 0.01
                    modelValueEnabled: true; modelValue: root.configuration.previewMaximumScale
                    detentEnabled: true
                    detentValue: 0.78
                    valueText: Math.round(displayedValue * 100) + "%"
                    onValueEdited: value => root.configuration.setPreviewMaximumScale(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.position", "Position")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.position", "Choose a screen corner for the thumbnail")
                Controls.SelectButton {
                    width: 198
                    label: root.positionOptions[root.indexOf(root.positionIds, root.configuration.position)]
                    options: root.positionOptions
                    selectedIndex: root.indexOf(root.positionIds, root.configuration.position)
                    onSelected: index => root.configuration.setPosition(root.positionIds[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.edge_margin", "Edge margin")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.edge_margin", "Keep the thumbnail and shadow away from the display edge")
                Controls.Slider {
                    from: 0; to: 120; stepSize: 2
                    modelValueEnabled: true; modelValue: root.configuration.edgeMargin
                    detentEnabled: true
                    detentValue: 20
                    valueText: root.pixels(displayedValue)
                    onValueEdited: value => root.configuration.setEdgeMargin(Math.round(value))
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.screenshots.text.animation", "ANIMATION")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.animations", "Animations")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.animations", "Animate screenshot arrival and dismissal")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.animations", "Animations")
                    checked: root.configuration.animationsEnabled
                    onToggled: value => root.configuration.setAnimationsEnabled(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.entry_style", "Entry style")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.entry_style", "Choose how the thumbnail appears")
                Controls.SelectButton {
                    width: 198
                    label: root.animationOptions[root.indexOf(root.animationIds, root.configuration.entryAnimation)]
                    options: root.animationOptions
                    selectedIndex: root.indexOf(root.animationIds, root.configuration.entryAnimation)
                    enabled: root.configuration.animationsEnabled
                    onSelected: index => root.configuration.setEntryAnimation(root.animationIds[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.entry_duration", "Entry duration")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.entry_duration", "Set the appearance transition length")
                Controls.Slider {
                    from: 0; to: 1500; stepSize: 25
                    modelValueEnabled: true; modelValue: root.configuration.entryDurationMs
                    detentEnabled: true
                    detentValue: 160
                    valueText: root.duration(displayedValue)
                    enabled: root.configuration.animationsEnabled
                    onValueEdited: value => root.configuration.setEntryDurationMs(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.entry_easing", "Entry easing")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.entry_easing", "Choose the entry motion curve")
                Controls.SelectButton {
                    width: 198
                    label: root.easingOptions[root.indexOf(root.easingIds, root.configuration.entryEasing)]
                    options: root.easingOptions
                    selectedIndex: root.indexOf(root.easingIds, root.configuration.entryEasing)
                    enabled: root.configuration.animationsEnabled
                    onSelected: index => root.configuration.setEntryEasing(root.easingIds[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.exit_style", "Exit style")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.exit_style", "Choose how the thumbnail leaves the screen")
                Controls.SelectButton {
                    width: 198
                    label: root.animationOptions[root.indexOf(root.animationIds, root.configuration.exitAnimation)]
                    options: root.animationOptions
                    selectedIndex: root.indexOf(root.animationIds, root.configuration.exitAnimation)
                    enabled: root.configuration.animationsEnabled
                    onSelected: index => root.configuration.setExitAnimation(root.animationIds[index])
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.exit_duration", "Exit duration")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.exit_duration", "Set the dismissal transition length")
                Controls.Slider {
                    from: 0; to: 1500; stepSize: 25
                    modelValueEnabled: true; modelValue: root.configuration.exitDurationMs
                    detentEnabled: true
                    detentValue: 220
                    valueText: root.duration(displayedValue)
                    enabled: root.configuration.animationsEnabled
                    onValueEdited: value => root.configuration.setExitDurationMs(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.exit_easing", "Exit easing")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.exit_easing", "Choose the exit motion curve")
                Controls.SelectButton {
                    width: 198
                    label: root.easingOptions[root.indexOf(root.easingIds, root.configuration.exitEasing)]
                    options: root.easingOptions
                    selectedIndex: root.indexOf(root.easingIds, root.configuration.exitEasing)
                    enabled: root.configuration.animationsEnabled
                    onSelected: index => root.configuration.setExitEasing(root.easingIds[index])
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.screenshots.text.behavior", "BEHAVIOR")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.show_thumbnail", "Show thumbnail")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.show_thumbnail", "Show the saved image after a capture")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.show_thumbnail", "Show thumbnail")
                    checked: root.configuration.showThumbnail
                    onToggled: value => root.configuration.setShowThumbnail(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.auto_copy", "Copy automatically")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.auto_copy", "Copy each capture to the clipboard")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.auto_copy", "Copy automatically")
                    checked: root.configuration.autoCopy
                    onToggled: value => root.configuration.setAutoCopy(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.drag", "Drag and drop")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.drag", "Allow dragging the PNG into other applications")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("apps.settings.pages.appearance.screenshots.label.drag", "Drag and drop")
                    checked: root.configuration.dragEnabled
                    onToggled: value => root.configuration.setDragEnabled(value)
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.display_duration", "Display duration")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.display_duration", "How long the thumbnail stays visible while idle")
                Controls.Slider {
                    from: 500; to: 30000; stepSize: 500
                    modelValueEnabled: true; modelValue: root.configuration.displayDurationMs
                    detentEnabled: true
                    detentValue: 5000
                    valueText: root.duration(displayedValue)
                    onValueEdited: value => root.configuration.setDisplayDurationMs(Math.round(value))
                }
            }
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.click_action", "Click action")
                sublabel: I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.click_action", "Choose what a click on the thumbnail does")
                Controls.SelectButton {
                    width: 198
                    label: root.clickOptions[root.indexOf(root.clickIds, root.configuration.clickAction)]
                    options: root.clickOptions
                    selectedIndex: root.indexOf(root.clickIds, root.configuration.clickAction)
                    onSelected: index => root.configuration.setClickAction(root.clickIds[index])
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.screenshots.label.status", "Application status")
                sublabel: root.configuration.status === "error"
                    ? root.configuration.lastError
                    : I18n.tr("apps.settings.pages.appearance.screenshots.sublabel.status", "Settings are saved atomically and reload in the Shell")
                Controls.Button {
                    label: I18n.tr("apps.settings.pages.appearance.screenshots.action.reset", "Reset to defaults")
                    flat: true
                    onClicked: root.configuration.resetToDefaults()
                }
            }
        }
    }

    ColorDialog {
        id: borderColorDialog
        title: I18n.tr("apps.settings.pages.appearance.screenshots.dialog.border_color", "Screenshot border color")
        selectedColor: root.configuration.borderColor
        onAccepted: root.configuration.setBorderColor(selectedColor)
    }

}
