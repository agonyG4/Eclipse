import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/form" as Form

Item {
    id: root
    objectName: "dockPage"

    readonly property var controller: SettingsController.dock
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

    Form.ScrollPage {
        id: scrollPage
        objectName: "dockScrollPage"
        anchors.fill: parent
        contentMargins: 28
        maxWidth: 900

        Item {
            objectName: "dockPreview"
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            Layout.bottomMargin: 24

            Rectangle {
                anchors.fill: parent
                radius: Components.Theme.cardRadius
                color: Components.Theme.cardBg
                border.width: 1
                border.color: Components.Theme.cardBorder
            }

            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 20
                anchors.topMargin: 16
                text: I18n.tr("apps.settings.pages.appearance.dock.text.preview", "PREVIEW")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                font.weight: Components.Theme.fontWeightBold
                font.letterSpacing: Components.Theme.trackingHeader
            }

            Item {
                id: previewStage
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 38
                anchors.bottomMargin: 14

                Rectangle {
                    id: previewDock
                    readonly property bool vertical: root.controller.position !== "bottom"
                    readonly property real iconExtent: Math.min(34, root.controller.iconSize * 0.58)
                    readonly property real panelExtent: root.controller.panelPadding + iconExtent
                    width: vertical ? panelExtent : Math.min(parent.width - 40, panelExtent * 5 + root.controller.itemSpacing * 4)
                    height: vertical ? Math.min(parent.height, panelExtent * 5 + root.controller.itemSpacing * 4) : panelExtent
                    x: vertical && root.controller.position === "right"
                       ? parent.width - width - 20
                       : vertical ? 20 : (parent.width - width) / 2
                    y: vertical ? (parent.height - height) / 2 : parent.height - height
                    radius: root.controller.cornerRadius
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

                Slider {
                    objectName: "iconSizeSlider"
                    width: 180
                    from: 32
                    to: 64
                    stepSize: 1
                    value: root.controller.iconSize
                    onMoved: root.controller.setIconSize(Math.round(value))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "itemSpacingRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.icon_spacing", "Icon spacing")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.icon_spacing", "Set the distance between neighboring icons")

                Slider {
                    objectName: "itemSpacingSlider"
                    width: 180
                    from: 4
                    to: 24
                    stepSize: 1
                    value: root.controller.itemSpacing
                    onMoved: root.controller.setItemSpacing(Math.round(value))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "panelPaddingRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.panel_padding", "Panel padding")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.panel_padding", "Set the space between the panel edge and icons")

                Slider {
                    objectName: "panelPaddingSlider"
                    width: 180
                    from: 8
                    to: 32
                    stepSize: 1
                    value: root.controller.panelPadding
                    onMoved: root.controller.setPanelPadding(Math.round(value))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.position", "Position")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.position", "Choose which screen edge owns the Dock")

                Form.SelectButton {
                    width: 180
                    label: root.positionOptions[root.indexOf(root.positionOptions, root.controller.position)]
                    options: root.positionOptions
                    selectedIndex: root.indexOf(["bottom", "left", "right"], root.controller.position)
                    onSelected: index => root.controller.setPosition(["bottom", "left", "right"][index])
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.floating", "Floating Dock")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.floating", "Keep the configured edge distance when the Dock is floating")

                Form.ToggleSwitch {
                    checked: root.controller.floating
                    onToggled: targetChecked => { root.controller.setFloating(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                objectName: "edgeMarginRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.edge_margin", "Distance from screen edge")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.edge_margin", "Preserved while Floating Dock is disabled")

                Slider {
                    objectName: "edgeMarginSlider"
                    width: 180
                    from: 0
                    to: 48
                    stepSize: 1
                    value: root.controller.edgeMargin
                    onMoved: root.controller.setEdgeMargin(Math.round(value))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "cornerRadiusRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.corner_radius", "Corner radius")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.corner_radius", "Round the Dock panel corners")

                Slider {
                    objectName: "cornerRadiusSlider"
                    width: 180
                    from: 0
                    to: 48
                    stepSize: 1
                    value: root.controller.cornerRadius
                    onMoved: root.controller.setCornerRadius(Math.round(value))
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
                label: I18n.tr("apps.settings.pages.appearance.dock.label.hover_effect", "Hover effect")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.hover_effect", "Choose how icons respond to pointer movement")

                Form.SelectButton {
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

                Slider {
                    objectName: "magnificationScaleSlider"
                    width: 180
                    enabled: root.magnificationSelected
                    from: 1.0
                    to: 2.0
                    stepSize: 0.05
                    value: root.controller.magnificationScale
                    onMoved: root.controller.setMagnificationScale(value)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                objectName: "magnificationRadiusRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.magnification_radius", "Magnification radius")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.magnification_radius", "Set how far magnification reaches along the Dock")

                Slider {
                    objectName: "magnificationRadiusSlider"
                    width: 180
                    enabled: root.magnificationSelected
                    from: 1.0
                    to: 4.0
                    stepSize: 0.05
                    value: root.controller.magnificationRadius
                    onMoved: root.controller.setMagnificationRadius(value)
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.dock.label.auto_hide", "Auto-hide")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.auto_hide", "Choose when the Dock collapses while staying edge-revealable")

                Form.SelectButton {
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

                Form.ToggleSwitch {
                    checked: root.controller.animationsEnabled
                    onToggled: targetChecked => { root.controller.setAnimationsEnabled(targetChecked); root.controller.flush() }
                }
            }

            Form.SettingRow {
                objectName: "animationSpeedRow"
                label: I18n.tr("apps.settings.pages.appearance.dock.label.animation_speed", "Animation speed")
                sublabel: I18n.tr("apps.settings.pages.appearance.dock.sublabel.animation_speed", "Adjust how quickly Dock transitions complete")

                Slider {
                    objectName: "animationSpeedSlider"
                    width: 180
                    enabled: root.controller.animationsEnabled
                    from: 0.25
                    to: 4.0
                    stepSize: 0.05
                    value: root.controller.animationSpeed
                    onMoved: root.controller.setAnimationSpeed(value)
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

                Form.SelectButton {
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

                Slider {
                    objectName: "indicatorSizeSlider"
                    width: 180
                    enabled: root.controller.indicatorStyle !== "none"
                    from: 1
                    to: 12
                    stepSize: 1
                    value: root.controller.indicatorSize
                    onMoved: root.controller.setIndicatorSize(Math.round(value))
                    onPressedChanged: if (!pressed) root.controller.flush()
                }
            }
        }

        Text {
            visible: root.controller.lastError !== ""
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            text: root.controller.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }
}
