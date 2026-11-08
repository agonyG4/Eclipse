import QtQuick
import QtQuick.Layouts
import Astrea.System
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "dockPage"

    readonly property var controller: SettingsController.dock
    readonly property var hoverIndicatorValues: ["plate", "none"]
    readonly property var hoverIndicatorOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.hover_plate", "Plate"),
        I18n.tr("apps.settings.pages.appearance.dock.option.none", "None")
    ]
    readonly property var pressEffectValues: ["darken", "shrink", "frosted", "none"]
    readonly property var pressEffectOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.press_darken", "Darken"),
        I18n.tr("apps.settings.pages.appearance.dock.option.press_shrink", "Shrink"),
        I18n.tr("apps.settings.pages.appearance.dock.option.press_frosted", "Frosted glass"),
        I18n.tr("apps.settings.pages.appearance.dock.option.none", "None")
    ]
    readonly property var popupStyleValues: ["floating", "attached"]
    readonly property var popupStyleOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.floating", "Floating"),
        I18n.tr("apps.settings.pages.appearance.dock.option.attached", "Attached to bar")
    ]

    TopbarPreferences { id: topbarPreferences }
    readonly property bool magnificationSelected: controller.hoverEffect === "magnification"
    readonly property var positionOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.bottom", "Bottom"),
        I18n.tr("apps.settings.pages.appearance.dock.option.left", "Left"),
        I18n.tr("apps.settings.pages.appearance.dock.option.right", "Right")
    ]
    readonly property var hoverOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.none", "None"),
        I18n.tr("apps.settings.pages.appearance.dock.option.lift", "Lift"),
        I18n.tr("apps.settings.pages.appearance.dock.option.magnification", "Magnification")
    ]
    readonly property var autoHideOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.never", "Never"),
        I18n.tr("apps.settings.pages.appearance.dock.option.intelligent", "Intelligent"),
        I18n.tr("apps.settings.pages.appearance.dock.option.always", "Always")
    ]
    readonly property var indicatorOptions: [
        I18n.tr("apps.settings.pages.appearance.dock.option.line", "Line"),
        I18n.tr("apps.settings.pages.appearance.dock.option.dot", "Dot"),
        I18n.tr("apps.settings.pages.appearance.dock.option.none", "None")
    ]

    function indexOf(values, value) {
        return Math.max(0, values.indexOf(value))
    }

    function unit(value) {
        return Math.round(value) + " "
            + I18n.tr("apps.settings.pages.appearance.dock.unit.pixels", "px")
    }

    function compactNumber(value, decimals) {
        var text = Number(value).toFixed(decimals)
        while (text.indexOf(".") >= 0 && (text.endsWith("0") || text.endsWith(".")))
            text = text.slice(0, -1)
        return text
    }

    function scaleUnit(value) {
        return compactNumber(value, 2) + "×"
    }

    Form.ScrollPage {
        id: scrollPage
        objectName: "dockScrollPage"
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.nav.dock", "Dock")
            subtitle: I18n.tr("apps.settings.pages.appearance.dock.subtitle", "Size, position and behavior of the Dock.")
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.preview", "PREVIEW")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "dockPreview"
            Layout.bottomMargin: 24
            clip: true

            Item {
                id: previewStage
                Layout.fillWidth: true
                Layout.preferredHeight: 128

                Rectangle {
                    id: previewDock
                    readonly property bool vertical: root.controller.position !== "bottom"
                    readonly property real iconExtent: Math.min(34, root.controller.iconSize * 0.58)
                    readonly property real contentExtent: iconExtent * 5 + root.controller.itemSpacing * 4
                        + root.controller.panelPadding * 2
                    readonly property real crossExtent: iconExtent + root.controller.panelPadding * 2
                    readonly property real inset: Components.Theme.spacingXLarge
                    width: vertical ? crossExtent : Math.min(parent.width - inset * 2, contentExtent)
                    height: vertical ? Math.min(parent.height - inset, contentExtent) : crossExtent
                    x: vertical && root.controller.position === "right"
                       ? parent.width - width - inset
                       : vertical ? inset : (parent.width - width) / 2
                    y: vertical ? (parent.height - height) / 2 : parent.height - height - inset
                    radius: Math.min(root.controller.cornerRadius, Math.min(width, height) / 2)
                    color: Components.Theme.windowBackground
                    border.width: 1
                    border.color: Components.Theme.cardBorder

                    Repeater {
                        model: 5

                        Rectangle {
                            readonly property real extent: previewDock.iconExtent
                            width: extent
                            height: extent
                            radius: extent * 0.24
                            x: previewDock.vertical ? (previewDock.width - width) / 2
                                                      : root.controller.panelPadding + index * (width + root.controller.itemSpacing)
                            y: previewDock.vertical ? root.controller.panelPadding + index * (height + root.controller.itemSpacing)
                                                      : (previewDock.height - height) / 2
                            color: index === 2 ? Components.Theme.accent : Components.Theme.textSecondary
                            opacity: index === 2 ? 0.95 : 0.52
                            scale: root.magnificationSelected && index === 2 ? 1.12 : 1.0
                            Behavior on scale {
                                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                            }

                            Rectangle {
                                visible: root.controller.indicatorStyle !== "none" && index === 2
                                width: root.controller.indicatorStyle === "dot"
                                       ? root.controller.indicatorSize
                                       : (previewDock.vertical ? root.controller.indicatorSize : Math.min(18, width * 0.7))
                                height: root.controller.indicatorStyle === "dot"
                                        ? root.controller.indicatorSize
                                        : (previewDock.vertical ? Math.min(18, height * 0.7) : root.controller.indicatorSize)
                                radius: root.controller.indicatorStyle === "dot" ? width / 2 : height / 2
                                anchors.horizontalCenter: previewDock.vertical ? undefined : parent.horizontalCenter
                                anchors.verticalCenter: previewDock.vertical ? parent.verticalCenter : undefined
                                anchors.bottom: previewDock.vertical ? undefined : parent.bottom
                                anchors.left: previewDock.vertical && root.controller.position === "left" ? parent.left : undefined
                                anchors.right: previewDock.vertical && root.controller.position === "right" ? parent.right : undefined
                                color: Components.Theme.accent
                            }
                        }
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.layout", "LAYOUT")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Form.SettingRow {
                objectName: "iconSizeRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.icon_size", "Icon size")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.icon_size", "Choose the resting size of Dock icons")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "iconSizeSlider"
                    from: 32
                    to: 64
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.iconSize
                    detentEnabled: true
                    detentValue: root.controller.defaultIconSize
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setIconSize(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "itemSpacingRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.icon_spacing", "Icon spacing")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.icon_spacing", "Set the distance between neighboring icons")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "itemSpacingSlider"
                    from: 4
                    to: 24
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.itemSpacing
                    detentEnabled: true
                    detentValue: root.controller.defaultItemSpacing
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setItemSpacing(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "panelPaddingRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.panel_padding", "Panel padding")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.panel_padding", "Set the space between the panel edge and icons")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "panelPaddingSlider"
                    from: 8
                    to: 32
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.panelPadding
                    detentEnabled: true
                    detentValue: root.controller.defaultPanelPadding
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setPanelPadding(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.position", "Position")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.position", "Choose which screen edge owns the Dock")

                Controls.SelectButton {
                    width: 180
                    label: root.positionOptions[root.indexOf(["bottom", "left", "right"], root.controller.position)]
                    options: root.positionOptions
                    selectedIndex: root.indexOf(["bottom", "left", "right"], root.controller.position)
                    onSelected: index => root.controller.setPosition(["bottom", "left", "right"][index])
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.floating", "Floating Dock")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.floating", "Keep the configured edge distance when the Dock is floating")

                Controls.ToggleSwitch {
                    checked: root.controller.floating
                    onToggled: targetChecked => { root.controller.setFloating(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                objectName: "edgeMarginRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.edge_margin", "Distance from screen edge")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.edge_margin", "Preserved while Floating Dock is disabled")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "edgeMarginSlider"
                    from: 0
                    to: 48
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.edgeMargin
                    detentEnabled: true
                    detentValue: root.controller.defaultEdgeMargin
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setEdgeMargin(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "cornerRadiusRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.corner_radius", "Corner radius")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.corner_radius", "Round the Dock panel corners")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "cornerRadiusSlider"
                    from: 0
                    to: 48
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.cornerRadius
                    detentEnabled: true
                    detentValue: root.controller.defaultCornerRadius
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setCornerRadius(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.behavior", "BEHAVIOR")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.minimized_thumbnails", "Show minimized windows")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.minimized_thumbnails", "Show minimized windows as thumbnails at the end of the Dock")

                Controls.ToggleSwitch {
                    objectName: "minimizedThumbnailsToggle"
                    checked: root.controller.minimizedThumbnailsEnabled
                    onToggled: targetChecked => { root.controller.setMinimizedThumbnailsEnabled(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.window_previews", "Window previews on hover")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.window_previews", "Show all windows when hovering over an app with multiple windows")

                Controls.ToggleSwitch {
                    objectName: "windowPreviewsToggle"
                    checked: root.controller.windowPreviewsEnabled
                    onToggled: targetChecked => { root.controller.setWindowPreviewsEnabled(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.hover_effect", "Hover effect")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.hover_effect", "Choose how icons respond to pointer movement")

                Controls.SelectButton {
                    width: 180
                    label: root.hoverOptions[root.indexOf(["none", "lift", "magnification"], root.controller.hoverEffect)]
                    options: root.hoverOptions
                    selectedIndex: root.indexOf(["none", "lift", "magnification"], root.controller.hoverEffect)
                    onSelected: index => root.controller.setHoverEffect(["none", "lift", "magnification"][index])
                }
            }

            Form.SettingRow {
                objectName: "magnificationScaleRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.magnification_strength", "Magnification strength")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.magnification_strength", "Set how large hovered icons become")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "magnificationScaleSlider"
                    enabled: root.magnificationSelected
                    from: 1.0
                    to: 2.0
                    stepSize: 0.05
                    modelValueEnabled: true
                    modelValue: root.controller.magnificationScale
                    detentEnabled: true
                    detentValue: root.controller.defaultMagnificationScale
                    valueText: root.scaleUnit(displayedValue)
                    onValueEdited: editedValue => root.controller.setMagnificationScale(editedValue)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "magnificationRadiusRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.magnification_radius", "Magnification radius")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.magnification_radius", "Set how far magnification reaches along the Dock")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "magnificationRadiusSlider"
                    enabled: root.magnificationSelected
                    from: 1.0
                    to: 4.0
                    stepSize: 0.05
                    modelValueEnabled: true
                    modelValue: root.controller.magnificationRadius
                    detentEnabled: true
                    detentValue: root.controller.defaultMagnificationRadius
                    valueText: root.scaleUnit(displayedValue)
                    onValueEdited: editedValue => root.controller.setMagnificationRadius(editedValue)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.auto_hide", "Auto-hide")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.auto_hide", "Choose when the Dock collapses while staying edge-revealable")

                Controls.SelectButton {
                    width: 180
                    label: root.autoHideOptions[root.indexOf(["never", "intelligent", "always"], root.controller.autoHide)]
                    options: root.autoHideOptions
                    selectedIndex: root.indexOf(["never", "intelligent", "always"], root.controller.autoHide)
                    onSelected: index => root.controller.setAutoHide(["never", "intelligent", "always"][index])
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.animations", "Animations")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.animations", "Animate Dock movement and hover transitions")

                Controls.ToggleSwitch {
                    checked: root.controller.animationsEnabled
                    onToggled: targetChecked => { root.controller.setAnimationsEnabled(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                objectName: "animationSpeedRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.animation_speed", "Animation speed")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.animation_speed", "Adjust how quickly Dock transitions complete")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "animationSpeedSlider"
                    enabled: root.controller.animationsEnabled
                    from: 0.25
                    to: 4.0
                    stepSize: 0.05
                    modelValueEnabled: true
                    modelValue: root.controller.animationSpeed
                    detentEnabled: true
                    detentValue: root.controller.defaultAnimationSpeed
                    valueText: root.scaleUnit(displayedValue)
                    onValueEdited: editedValue => root.controller.setAnimationSpeed(editedValue)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.indicators", "INDICATORS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.indicator_style", "Indicator style")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.indicator_style", "Show which running applications are active")

                Controls.SelectButton {
                    width: 180
                    label: root.indicatorOptions[root.indexOf(["line", "dot", "none"], root.controller.indicatorStyle)]
                    options: root.indicatorOptions
                    selectedIndex: root.indexOf(["line", "dot", "none"], root.controller.indicatorStyle)
                    onSelected: index => root.controller.setIndicatorStyle(["line", "dot", "none"][index])
                }
            }

            Form.SettingRow {
                objectName: "indicatorSizeRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.indicator_size", "Indicator size")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.indicator_size", "Set line thickness or dot diameter")
                isLast: true

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "indicatorSizeSlider"
                    enabled: root.controller.indicatorStyle !== "none"
                    from: 1
                    to: 12
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.indicatorSize
                    detentEnabled: true
                    detentValue: root.controller.defaultIndicatorSize
                    valueText: root.unit(displayedValue)
                    onValueEdited: editedValue => root.controller.setIndicatorSize(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.icon_feedback", "ICON FEEDBACK")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "iconFeedbackCard"
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.hover_indicator", "Hover indicator")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.hover_indicator", "Show a plate behind the icon under the pointer")

                Controls.SelectButton {
                    objectName: "hoverIndicatorSelect"
                    width: 180
                    label: root.hoverIndicatorOptions[root.indexOf(root.hoverIndicatorValues, root.controller.hoverIndicator)]
                    options: root.hoverIndicatorOptions
                    selectedIndex: root.indexOf(root.hoverIndicatorValues, root.controller.hoverIndicator)
                    onSelected: index => root.controller.setHoverIndicator(root.hoverIndicatorValues[index])
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.hover_strength", "Hover strength")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.hover_strength", "Set how visible the plate is")

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "hoverStrengthSlider"
                    enabled: root.controller.hoverIndicator !== "none"
                    from: 4
                    to: 40
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.hoverIndicatorStrength
                    detentEnabled: true
                    detentValue: root.controller.defaultHoverIndicatorStrength
                    valueText: Math.round(displayedValue) + "%"
                    onValueEdited: editedValue => root.controller.setHoverIndicatorStrength(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.press_effect", "Press effect")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.press_effect", "Choose what an icon does while you hold it")

                Controls.SelectButton {
                    objectName: "pressEffectSelect"
                    width: 180
                    label: root.pressEffectOptions[root.indexOf(root.pressEffectValues, root.controller.pressEffect)]
                    options: root.pressEffectOptions
                    selectedIndex: root.indexOf(root.pressEffectValues, root.controller.pressEffect)
                    onSelected: index => root.controller.setPressEffect(root.pressEffectValues[index])
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.press_strength", "Press strength")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.press_strength", "Set how far the icon darkens or shrinks")
                isLast: true

                Controls.Slider {
                    showEndpointGlyphs: false
                    showValueLabel: true
                    showValueTooltip: false
                    objectName: "pressStrengthSlider"
                    enabled: root.controller.pressEffect !== "none"
                    from: 10
                    to: 60
                    stepSize: 1
                    modelValueEnabled: true
                    modelValue: root.controller.pressEffectStrength
                    detentEnabled: true
                    detentValue: root.controller.defaultPressEffectStrength
                    valueText: Math.round(displayedValue) + "%"
                    onValueEdited: editedValue => root.controller.setPressEffectStrength(Math.round(editedValue))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.dock.text.menu_bar", "MENU BAR")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "menuBarCard"
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.popup_style", "Popover style")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.popup_style",
                                  "Float menus below the bar, or slide them out from under it")
                isLast: true

                Controls.SelectButton {
                    objectName: "popupStyleSelect"
                    width: 180
                    label: root.popupStyleOptions[root.indexOf(root.popupStyleValues, topbarPreferences.popupStyle)]
                    options: root.popupStyleOptions
                    selectedIndex: root.indexOf(root.popupStyleValues, topbarPreferences.popupStyle)
                    onSelected: index => topbarPreferences.popupStyle = root.popupStyleValues[index]
                }
            }
        }

        Text {
            visible: root.controller.lastError !== ""
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            text: root.controller.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
            wrapMode: Text.WordWrap
        }

        Form.FormCard {
            objectName: "dockRestoreFooter"
            Layout.bottomMargin: 32

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.restore_defaults", "Restore defaults")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.restore_defaults", "Reset every Dock setting except your pinned apps")
                isLast: true

                Controls.ButtonCapsule {
                    objectName: "restoreDefaultsButton"
                    label: I18n.tr("apps.settings.pages.appearance.dock.action.restore", "Restore")
                    accessibleName: I18n.tr(
                        "apps.settings.pages.appearance.dock.action.restore_defaults",
                        "Restore Defaults")
                    enabled: !root.controller.isDefault
                    onClicked: root.controller.restoreDefaults()
                }
            }
        }
    }

    Item {
        id: restoreToast
        objectName: "dockRestoreToast"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 32, 360)
        height: 44
        visible: root.controller.canUndoRestore
        enabled: visible
        z: 20
        opacity: visible ? 1 : 0

        Rectangle {
            anchors.fill: parent
            radius: Components.Theme.controlRadius
            color: Components.Theme.popupBg
            border.width: 1
            border.color: Components.Theme.cardBorder
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            spacing: 12

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - undoButton.implicitWidth - parent.spacing)
                text: I18n.tr(
                    "apps.settings.pages.appearance.dock.status.restored",
                    "Dock settings restored")
                color: Components.Theme.textPrimary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Controls.Button {
                id: undoButton
                objectName: "dockRestoreUndoButton"
                anchors.verticalCenter: parent.verticalCenter
                label: I18n.tr(
                    "apps.settings.pages.appearance.dock.action.undo",
                    "Undo")
                flat: true
                enabled: root.controller.canUndoRestore
                onClicked: root.controller.undoRestore()
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Components.Theme.animationQuick
                easing.type: Easing.OutCubic
            }
        }
    }
}
