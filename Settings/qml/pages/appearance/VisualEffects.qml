import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "visualEffectsPage"

    readonly property var controller: SettingsController.visualEffects
    readonly property var wallpaperController: SettingsController.wallpaper
    property bool advancedExpanded: false

    Component.onCompleted: {
        if (root.controller)
            root.controller.refresh()
        if (root.wallpaperController && !root.wallpaperController.busy)
            root.wallpaperController.refresh()
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.visual_effects.material", "MATERIAL")
            Layout.bottomMargin: 12
        }

        MaterialPreview {
            objectName: "visualEffectsMaterialPreview"
            Layout.fillWidth: true
            Layout.preferredHeight: 270
            Layout.bottomMargin: 20
            controller: root.controller
            wallpaperSource: root.wallpaperController
                ? root.wallpaperController.effectivePreviewUrl : ""
            wallpaperFit: root.wallpaperController
                ? root.wallpaperController.effectiveFit : "cover"
            themeVariant: Components.Theme.isLight ? "light" : "dark"
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            margins: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: I18n.tr("apps.settings.pages.visual_effects.glass", "Glass")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: I18n.tr("apps.settings.pages.visual_effects.frosted", "Frosted")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                }

                Controls.Slider {
                    id: materialSlider
                    objectName: "materialPositionSlider"
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    stepSize: 0.01
                    showEndpointGlyphs: false
                    enabled: root.controller && root.controller.available
                    modelValueEnabled: root.controller && root.controller.available
                    modelValue: root.controller ? root.controller.materialPosition : 0.5
                    detentEnabled: true
                    detentValue: root.controller
                        ? root.controller.defaultMaterialPosition : 0.5
                    valueText: Number(displayedValue).toFixed(2)
                    Accessible.name: I18n.tr("apps.settings.pages.visual_effects.material_position", "Glass to Frosted material")
                    onValueEdited: value => {
                        if (root.controller)
                            root.controller.setMaterialPosition(value)
                    }
                    onPressedChanged: if (!pressed && root.controller) root.controller.flush()
                    Keys.onReleased: event => {
                        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                                || event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                            if (root.controller)
                                root.controller.flush()
                        }
                    }
                }
            }
        }

        QQC2.Button {
            id: advancedToggle
            objectName: "visualEffectsAdvancedToggle"
            Layout.fillWidth: true
            flat: true
            activeFocusOnTab: true
            text: root.advancedExpanded
                ? I18n.tr("apps.settings.pages.visual_effects.advanced_hide", "Advanced  −")
                : I18n.tr("apps.settings.pages.visual_effects.advanced_show", "Advanced  +")
            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.advanced", "Advanced")
            Accessible.checked: root.advancedExpanded
            onClicked: root.advancedExpanded = !root.advancedExpanded
        }

        Form.FormCard {
            objectName: "visualEffectsAdvancedCard"
            visible: root.advancedExpanded
            Layout.fillWidth: true
            Layout.bottomMargin: 14

            ColumnLayout {
                Layout.fillWidth: true

                Form.SettingRow {
                    objectName: "materialBlurRow"
                    label: I18n.tr("apps.settings.pages.visual_effects.blur", "Blur Strength")
                    sublabel: root.controller && root.controller.blurOverridden
                        ? I18n.tr("apps.settings.pages.visual_effects.overridden", "Override")
                        : I18n.tr("apps.settings.pages.visual_effects.automatic", "Automatic")
                    isLast: false
                    RowLayout {
                        Controls.Slider {
                            objectName: "materialBlurSlider"
                            width: 190
                            from: 0.0; to: 1.0; stepSize: 0.01
                            showEndpointGlyphs: false
                            enabled: root.controller && root.controller.available
                            modelValueEnabled: root.controller && root.controller.available
                            modelValue: root.controller ? root.controller.blurValue : 0.0
                            valueText: Number(displayedValue).toFixed(2)
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.blur", "Blur Strength")
                            onValueEdited: value => root.controller.setBlurOverride(value)
                            onPressedChanged: if (!pressed && root.controller) root.controller.flush()
                            Keys.onReleased: event => {
                                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                                        || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                                    root.controller.flush()
                            }
                        }
                        QQC2.Button {
                            visible: root.controller && root.controller.blurOverridden
                            enabled: root.controller && root.controller.available
                            text: I18n.tr("apps.settings.pages.visual_effects.reset", "Reset")
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.reset_blur", "Reset Blur Strength")
                            onClicked: root.controller.clearBlurOverride()
                        }
                    }
                }

                Form.SettingRow {
                    objectName: "materialSaturationRow"
                    label: I18n.tr("apps.settings.pages.visual_effects.saturation", "Saturation")
                    sublabel: root.controller && root.controller.saturationOverridden
                        ? I18n.tr("apps.settings.pages.visual_effects.overridden", "Override")
                        : I18n.tr("apps.settings.pages.visual_effects.automatic", "Automatic")
                    isLast: false
                    RowLayout {
                        Controls.Slider {
                            objectName: "materialSaturationSlider"
                            width: 190
                            from: 0.0; to: 1.0; stepSize: 0.01
                            showEndpointGlyphs: false
                            enabled: root.controller && root.controller.available
                            modelValueEnabled: root.controller && root.controller.available
                            modelValue: root.controller ? root.controller.saturationValue : 1.0
                            valueText: Number(displayedValue).toFixed(2)
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.saturation", "Saturation")
                            onValueEdited: value => root.controller.setSaturationOverride(value)
                            onPressedChanged: if (!pressed && root.controller) root.controller.flush()
                            Keys.onReleased: event => {
                                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                                        || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                                    root.controller.flush()
                            }
                        }
                        QQC2.Button {
                            visible: root.controller && root.controller.saturationOverridden
                            enabled: root.controller && root.controller.available
                            text: I18n.tr("apps.settings.pages.visual_effects.reset", "Reset")
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.reset_saturation", "Reset Saturation")
                            onClicked: root.controller.clearSaturationOverride()
                        }
                    }
                }

                Form.SettingRow {
                    objectName: "materialNoiseRow"
                    label: I18n.tr("apps.settings.pages.visual_effects.noise", "Noise")
                    sublabel: root.controller && root.controller.noiseOverridden
                        ? I18n.tr("apps.settings.pages.visual_effects.overridden", "Override")
                        : I18n.tr("apps.settings.pages.visual_effects.automatic", "Automatic")
                    isLast: true
                    RowLayout {
                        Controls.Slider {
                            objectName: "materialNoiseSlider"
                            width: 190
                            from: 0.0; to: 1.0; stepSize: 0.01
                            showEndpointGlyphs: false
                            enabled: root.controller && root.controller.available
                            modelValueEnabled: root.controller && root.controller.available
                            modelValue: root.controller ? root.controller.noiseValue : 0.0
                            valueText: Number(displayedValue).toFixed(2)
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.noise", "Noise")
                            onValueEdited: value => root.controller.setNoiseOverride(value)
                            onPressedChanged: if (!pressed && root.controller) root.controller.flush()
                            Keys.onReleased: event => {
                                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                        || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                        || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                                        || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                                    root.controller.flush()
                            }
                        }
                        QQC2.Button {
                            visible: root.controller && root.controller.noiseOverridden
                            enabled: root.controller && root.controller.available
                            text: I18n.tr("apps.settings.pages.visual_effects.reset", "Reset")
                            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.reset_noise", "Reset Noise")
                            onClicked: root.controller.clearNoiseOverride()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            QQC2.Button {
                objectName: "resetAdvancedCustomizations"
                text: I18n.tr("apps.settings.pages.visual_effects.reset_advanced", "Reset advanced customizations")
                enabled: root.controller && root.controller.available && root.controller.hasOverrides
                onClicked: root.controller.resetOverrides()
            }
            Item { Layout.fillWidth: true }
            QQC2.Button {
                text: I18n.tr("apps.settings.pages.visual_effects.restore_defaults", "Restore Defaults")
                enabled: root.controller && root.controller.available
                onClicked: root.controller.restoreDefaults()
            }
        }

        Form.FormCard {
            objectName: "visualEffectsUnavailable"
            visible: !root.controller || !root.controller.available
                || (root.controller && root.controller.lastError.length > 0)
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            margins: 12
            Text {
                objectName: "visualEffectsErrorMessage"
                Layout.fillWidth: true
                text: root.controller && root.controller.lastError.length > 0
                    ? root.controller.lastError
                    : I18n.tr("apps.settings.pages.visual_effects.unavailable", "Typhon is unavailable. Material settings are read only until the compositor reconnects.")
                color: Components.Theme.errorColor
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                Accessible.role: Accessible.AlertMessage
            }
        }
    }
}
