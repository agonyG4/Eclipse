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
    readonly property bool available: !!root.controller && root.controller.available
    readonly property bool hasAdvancedCapabilities: !!root.controller &&
        (root.controller.blurOverrideSupported || root.controller.saturationOverrideSupported
         || root.controller.noiseOverrideSupported)
    readonly property var materialProgramCatalog: root.controller && root.controller.materialProgramCatalog
        ? root.controller.materialProgramCatalog : []
    readonly property var materialProgramParameters: root.controller && root.controller.materialProgramParameters
        ? root.controller.materialProgramParameters : []
    readonly property var materialProgramOptions: materialProgramCatalog.map(entry => root.programLabel(entry.name))
    readonly property int selectedMaterialProgramIndex: {
        if (!root.controller)
            return -1
        const pending = String(root.controller.materialProgramPendingSelection || "")
        const effective = String(root.controller.materialProgramEffective || "")
        const selected = pending.length > 0 ? pending : effective
        return materialProgramCatalog.findIndex(entry => entry.name === selected)
    }
    readonly property bool programFallbackActive: !!root.controller
        && String(root.controller.materialProgramRequested || "").length > 0
        && root.controller.materialProgramRequested !== root.controller.materialProgramEffective
    readonly property string programError: root.controller ? String(root.controller.materialProgramError || "") : ""
    // A single program without parameters leaves nothing to choose, so the section stays out of the way.
    readonly property bool showProgramSection: !!root.controller
        && (root.materialProgramCatalog.length > 1 || root.materialProgramParameters.length > 0
            || root.programFallbackActive || root.programError.length > 0
            || (root.available && !root.controller.materialProgramAvailable))
    property bool advancedExpanded: false

    function percentText(value) {
        return Math.round(Number(value) * 100) + "%"
    }
    function programLabel(name) {
        const text = String(name || "")
        const short = text.indexOf("system.") === 0 ? text.slice(7) : text
        return short.replace(/[._-]+/g, " ").replace(/\b\w/g, letter => letter.toUpperCase())
    }
    function fineTuneSummary() {
        if (!root.controller || !root.controller.hasOverrides)
            return I18n.tr("apps.settings.pages.visual_effects.fine_tune.automatic", "Automatic — follows the material style")
        const parts = []
        if (root.controller.blurOverridden)
            parts.push(I18n.tr("apps.settings.pages.visual_effects.blur_short", "Blur") + " " + root.percentText(root.controller.blurValue))
        if (root.controller.saturationOverridden)
            parts.push(I18n.tr("apps.settings.pages.visual_effects.saturation", "Saturation") + " " + root.percentText(root.controller.saturationValue))
        if (root.controller.noiseOverridden)
            parts.push(I18n.tr("apps.settings.pages.visual_effects.noise", "Noise") + " " + root.percentText(root.controller.noiseValue))
        return parts.join(" · ")
    }
    function flushOnKey(event, flush) {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                || event.key === Qt.Key_Home || event.key === Qt.Key_End)
            flush()
    }

    component LinkButton: Text {
        id: link
        signal clicked()
        color: link.enabled ? Components.Theme.accent : Components.Theme.textTertiary
        font.family: Components.Theme.fontFamily
        font.pixelSize: Components.Theme.fontSizeSmall
        activeFocusOnTab: link.enabled && link.visible
        Accessible.role: Accessible.Button
        Accessible.name: link.text
        Keys.onSpacePressed: link.clicked()
        Keys.onReturnPressed: link.clicked()
        MouseArea {
            anchors { fill: parent; margins: -6 }
            enabled: link.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: link.clicked()
        }
    }

    component TuneRow: RowLayout {
        id: tuneRow
        property string title: ""
        property string sliderName: ""
        property real value: 0
        property bool overridden: false
        property bool controlEnabled: true
        signal edited(real value)
        signal reset()
        Layout.fillWidth: true
        spacing: Components.Theme.spacingLarge

        Text {
            Layout.preferredWidth: 88
            text: tuneRow.title
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            elide: Text.ElideRight
        }
        Controls.Slider {
            objectName: tuneRow.sliderName
            Layout.fillWidth: true
            from: 0.0; to: 1.0; stepSize: 0.01
            showEndpointGlyphs: false
            showValueTooltip: false
            enabled: tuneRow.controlEnabled
            modelValueEnabled: tuneRow.controlEnabled
            modelValue: tuneRow.value
            Accessible.name: tuneRow.title
            onValueEdited: value => tuneRow.edited(value)
            onPressedChanged: if (!pressed && root.controller) root.controller.flush()
            Keys.onReleased: event => root.flushOnKey(event, () => root.controller.flush())
        }
        Text {
            Layout.preferredWidth: 44
            horizontalAlignment: Text.AlignRight
            text: root.percentText(tuneRow.value)
            color: tuneRow.overridden ? Components.Theme.textPrimary : Components.Theme.textTertiary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            font.features: { "tnum": 1 }
        }
        // Keeps its width while hidden so the sliders stay aligned across rows.
        LinkButton {
            Layout.preferredWidth: 40
            text: tuneRow.overridden
                ? I18n.tr("apps.settings.pages.visual_effects.auto", "Auto")
                : ""
            enabled: tuneRow.overridden && tuneRow.controlEnabled
            Accessible.name: I18n.tr("apps.settings.pages.visual_effects.reset", "Reset") + " " + tuneRow.title
            onClicked: tuneRow.reset()
        }
    }

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

        Form.PageHeader {
            title: I18n.tr("apps.settings.pages.visual_effects.title", "Visual Effects")
            subtitle: I18n.tr("apps.settings.pages.visual_effects.subtitle", "Tune the material behind panels, menus and the Dock.")
        }

        Text {
            objectName: "visualEffectsUnavailable"
            visible: !root.available || (!!root.controller && root.controller.lastError.length > 0)
            Layout.fillWidth: true
            Layout.bottomMargin: 10
            text: root.controller && root.controller.lastError.length > 0
                ? root.controller.lastError
                : I18n.tr("apps.settings.pages.visual_effects.unavailable", "Typhon is unavailable. Material settings are read only until the compositor reconnects.")
            color: Components.Theme.errorColor
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.visual_effects.material", "MATERIAL")
            Layout.bottomMargin: 12
        }

        MaterialPreview {
            objectName: "visualEffectsMaterialPreview"
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            Layout.bottomMargin: 16
            controller: root.controller
            wallpaperSource: root.wallpaperController
                ? root.wallpaperController.effectivePreviewUrl : ""
            wallpaperFit: root.wallpaperController
                ? root.wallpaperController.effectiveFit : "cover"
            themeVariant: Components.Theme.isLight ? "light" : "dark"
        }

        Form.FormCard {
            objectName: "materialCard"
            Layout.bottomMargin: 8

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.visual_effects.style", "Style")
                sublabel: I18n.tr("apps.settings.pages.visual_effects.style_description", "How much of the wallpaper shows through panels and menus")
                isLast: !root.hasAdvancedCapabilities
                ColumnLayout {
                    width: 300
                    spacing: 2
                    Controls.Slider {
                        objectName: "materialPositionSlider"
                        Layout.fillWidth: true
                        from: 0.0; to: 1.0; stepSize: 0.01
                        showEndpointGlyphs: false
                        showValueTooltip: false
                        enabled: root.available
                        modelValueEnabled: root.available
                        modelValue: root.controller ? root.controller.materialPosition : 0.5
                        detentEnabled: true
                        detentValue: root.controller ? root.controller.defaultMaterialPosition : 0.5
                        Accessible.name: I18n.tr("apps.settings.pages.visual_effects.material_position", "Glass to Frosted material")
                        onValueEdited: value => {
                            if (root.controller)
                                root.controller.setMaterialPosition(value)
                        }
                        onPressedChanged: if (!pressed && root.controller) root.controller.flush()
                        Keys.onReleased: event => root.flushOnKey(event, () => root.controller.flush())
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: I18n.tr("apps.settings.pages.visual_effects.glass", "Glass")
                            color: Components.Theme.textTertiary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeTiny
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: I18n.tr("apps.settings.pages.visual_effects.frosted", "Frosted")
                            color: Components.Theme.textTertiary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeTiny
                        }
                    }
                }
            }

            Form.SettingRow {
                id: advancedToggle
                objectName: "visualEffectsAdvancedToggle"
                visible: root.hasAdvancedCapabilities
                label: I18n.tr("apps.settings.pages.visual_effects.fine_tune", "Fine-tune")
                sublabel: root.fineTuneSummary()
                isLast: !root.advancedExpanded
                clickable: true
                onClicked: root.advancedExpanded = !root.advancedExpanded
                Text {
                    width: 24
                    horizontalAlignment: Text.AlignHCenter
                    text: "›"
                    color: Components.Theme.textTertiary
                    font.pixelSize: Components.Theme.fontSizeLarge
                    rotation: root.advancedExpanded ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic } }
                }
            }

            Item {
                objectName: "visualEffectsAdvancedCard"
                visible: root.advancedExpanded && root.hasAdvancedCapabilities
                Layout.fillWidth: true
                implicitHeight: advancedColumn.implicitHeight + Components.Theme.spacingMedium * 2

                ColumnLayout {
                    id: advancedColumn
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge
                        topMargin: Components.Theme.spacingMedium
                    }
                    spacing: Components.Theme.spacingMedium

                    TuneRow {
                        objectName: "materialBlurRow"
                        visible: !!root.controller && root.controller.blurOverrideSupported
                        title: I18n.tr("apps.settings.pages.visual_effects.blur_short", "Blur")
                        sliderName: "materialBlurSlider"
                        value: root.controller ? root.controller.blurValue : 0.0
                        overridden: !!root.controller && root.controller.blurOverridden
                        controlEnabled: root.available
                        onEdited: value => root.controller.setBlurOverride(value)
                        onReset: root.controller.clearBlurOverride()
                    }
                    TuneRow {
                        objectName: "materialSaturationRow"
                        visible: !!root.controller && root.controller.saturationOverrideSupported
                        title: I18n.tr("apps.settings.pages.visual_effects.saturation", "Saturation")
                        sliderName: "materialSaturationSlider"
                        value: root.controller ? root.controller.saturationValue : 1.0
                        overridden: !!root.controller && root.controller.saturationOverridden
                        controlEnabled: root.available
                        onEdited: value => root.controller.setSaturationOverride(value)
                        onReset: root.controller.clearSaturationOverride()
                    }
                    TuneRow {
                        objectName: "materialNoiseRow"
                        visible: !!root.controller && root.controller.noiseOverrideSupported
                        title: I18n.tr("apps.settings.pages.visual_effects.noise", "Noise")
                        sliderName: "materialNoiseSlider"
                        value: root.controller ? root.controller.noiseValue : 0.0
                        overridden: !!root.controller && root.controller.noiseOverridden
                        controlEnabled: root.available
                        onEdited: value => root.controller.setNoiseOverride(value)
                        onReset: root.controller.clearNoiseOverride()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 2
                        LinkButton {
                            objectName: "resetAdvancedCustomizations"
                            text: I18n.tr("apps.settings.pages.visual_effects.use_automatic", "Use automatic values")
                            enabled: root.available && !!root.controller && root.controller.hasOverrides
                            onClicked: root.controller.resetOverrides()
                        }
                        Item { Layout.fillWidth: true }
                        LinkButton {
                            objectName: "restoreVisualEffectsDefaults"
                            text: I18n.tr("apps.settings.pages.visual_effects.restore_defaults", "Restore Defaults")
                            enabled: root.available
                            onClicked: root.controller.restoreDefaults()
                        }
                    }
                }
            }
        }

        Text {
            objectName: "trustedProgramBuiltInMaterialNote"
            visible: !!root.controller && root.controller.materialProgramEffectiveOrigin === "trusted_local"
            Layout.fillWidth: true
            Layout.leftMargin: Components.Theme.spacingXLarge
            Layout.rightMargin: Components.Theme.spacingXLarge
            text: I18n.tr("apps.settings.pages.visual_effects.built_in_material_note",
                          "Glass / Frosted, Blur Strength, Saturation, and Noise tune Typhon’s built-in fallback material. They do not change the active trusted Material Program.")
            color: Components.Theme.textTertiary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Item { Layout.preferredHeight: 16; visible: root.showProgramSection }

        Form.SectionHeader {
            visible: root.showProgramSection
            text: I18n.tr("apps.settings.pages.visual_effects.program_section", "MATERIAL PROGRAM")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "materialProgramCard"
            visible: root.showProgramSection
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.visual_effects.program", "Material Program")
                sublabel: root.controller && root.controller.materialProgramAvailable
                    ? I18n.tr("apps.settings.pages.visual_effects.program_description", "The shader that draws the material")
                    : I18n.tr("apps.settings.pages.visual_effects.program_unavailable_short", "Unavailable")
                isLast: root.materialProgramParameters.length === 0 && !programStatus.visible
                Controls.SelectButton {
                    objectName: "materialProgramSelector"
                    width: 220
                    label: root.controller && String(root.controller.materialProgramEffective || "").length > 0
                        ? root.programLabel(root.controller.materialProgramEffective)
                        : I18n.tr("apps.settings.pages.visual_effects.program_loading", "Loading Material Programs…")
                    options: root.materialProgramOptions
                    selectedIndex: root.selectedMaterialProgramIndex
                    enabled: !!root.controller && root.controller.materialProgramAvailable
                        && root.materialProgramCatalog.length > 1
                    Accessible.name: I18n.tr("apps.settings.pages.visual_effects.program", "Material Program")
                    onSelected: index => {
                        if (index >= 0 && root.controller)
                            root.controller.selectMaterialProgram(root.materialProgramCatalog[index].name)
                    }
                }
            }

            Item {
                id: programStatus
                visible: fallbackStatus.visible || renderingStatus.visible || unavailableStatus.visible
                    || programErrorText.visible || refreshLink.visible
                Layout.fillWidth: true
                implicitHeight: statusColumn.implicitHeight + Components.Theme.spacingMedium * 2

                ColumnLayout {
                    id: statusColumn
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge
                        topMargin: Components.Theme.spacingMedium
                    }
                    spacing: 6

                    Text {
                        id: fallbackStatus
                        objectName: "materialProgramFallbackStatus"
                        visible: root.programFallbackActive
                        Layout.fillWidth: true
                        text: visible
                            ? I18n.tr("apps.settings.pages.visual_effects.program_fallback_requested", "Requested")
                                + " " + root.programLabel(root.controller.materialProgramRequested) + ", "
                                + I18n.tr("apps.settings.pages.visual_effects.program_fallback_using", "using")
                                + " " + root.programLabel(root.controller.materialProgramEffective) + " ("
                                + root.controller.materialProgramFallbackReason + " "
                                + I18n.tr("apps.settings.pages.visual_effects.program_fallback_label", "fallback") + ")."
                            : ""
                        color: Components.Theme.warningColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.AlertMessage
                    }
                    Text {
                        id: unavailableStatus
                        objectName: "materialProgramUnavailableStatus"
                        visible: !!root.controller && !root.controller.materialProgramAvailable
                        Layout.fillWidth: true
                        text: visible
                            ? (root.programError.length > 0
                                ? root.programError
                                : I18n.tr("apps.settings.pages.visual_effects.program_unavailable",
                                          "Material Program controls are unavailable while Typhon reconnects."))
                            : ""
                        color: Components.Theme.errorColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.AlertMessage
                    }
                    Text {
                        id: renderingStatus
                        objectName: "materialProgramRenderingStatus"
                        visible: !!root.controller && root.controller.materialProgramAvailable
                            && !root.controller.materialProgramRenderingAvailable
                        Layout.fillWidth: true
                        text: visible
                            ? I18n.tr("apps.settings.pages.visual_effects.rendering_unavailable",
                                      "Typhon rendering is unavailable. Program selection can still be configured.")
                            : ""
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        id: programErrorText
                        visible: root.programError.length > 0 && !!root.controller
                            && root.controller.materialProgramAvailable
                        Layout.fillWidth: true
                        text: visible ? root.programError : ""
                        color: Components.Theme.errorColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                        Accessible.role: Accessible.AlertMessage
                    }
                    LinkButton {
                        id: refreshLink
                        objectName: "refreshMaterialPrograms"
                        visible: !!root.controller && (!root.controller.materialProgramAvailable
                            || root.programError.length > 0)
                        text: I18n.tr("apps.settings.pages.visual_effects.refresh_programs", "Refresh Material Programs")
                        onClicked: root.controller.refresh()
                    }
                }

                Rectangle {
                    visible: root.materialProgramParameters.length > 0
                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: Components.Theme.spacingXLarge }
                    height: 1
                    color: Components.Theme.cardBorder
                }
            }

            Item {
                visible: root.materialProgramParameters.length > 0
                Layout.fillWidth: true
                implicitHeight: parameterColumn.implicitHeight + Components.Theme.spacingMedium * 2

                ColumnLayout {
                    id: parameterColumn
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge
                        topMargin: Components.Theme.spacingMedium
                    }
                    spacing: Components.Theme.spacingMedium

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: I18n.tr("apps.settings.pages.visual_effects.parameters", "Program Parameters")
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeNormal
                            font.weight: Components.Theme.fontWeightMedium
                        }
                        LinkButton {
                            objectName: "resetMaterialProgramParameters"
                            visible: !!root.controller && root.controller.materialProgramHasOverrides
                            enabled: !!root.controller && root.controller.materialProgramAvailable
                            text: I18n.tr("apps.settings.pages.visual_effects.reset_program_parameters",
                                          "Reset Program Parameters")
                            onClicked: root.controller.resetMaterialProgramParameters()
                        }
                    }

                    MaterialProgramParameterEditor {
                        objectName: "materialProgramParameterEditor"
                        Layout.fillWidth: true
                        controller: root.controller
                        parameters: root.materialProgramParameters
                        enabled: !!root.controller && root.controller.materialProgramEditingAvailable
                    }
                }
            }
        }
    }
}
