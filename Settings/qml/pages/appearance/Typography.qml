import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "typographyPage"
    property bool advancedExpanded: false
    property string systemStatus: ""
    readonly property var preferences: ThemeController.fontPreferences
    readonly property var families: ThemeController.availableFontFamilies()
    property string searchText: ""
    readonly property var fontOptions: {
        const term = searchText.trim().toLowerCase()
        return [I18n.tr("settings.typography.automatic", "Automatic (theme / system)")]
            .concat(families.filter(name => term === "" || name.toLowerCase().includes(term)))
    }
    readonly property var roleNames: ["display", "title", "body", "label", "caption", "mono"]
    readonly property var roleLabels: [
        I18n.tr("settings.typography.role_display", "Display"),
        I18n.tr("settings.typography.role_title", "Title"),
        I18n.tr("settings.typography.role_body", "Body"),
        I18n.tr("settings.typography.role_label", "Label"),
        I18n.tr("settings.typography.role_caption", "Caption"),
        I18n.tr("settings.typography.mono", "Monospaced")]

    function applySystemFonts(restore) {
        const saved = ThemeController.applySystemFontConfig(restore)
        systemStatus = saved
            ? (restore ? I18n.tr("settings.typography.system_restored", "Previous system font rules restored. Reopen applications to apply.")
                       : I18n.tr("settings.typography.system_applied", "System fonts applied. Reopen applications to apply."))
            : ThemeController.systemFontConfigStatus
    }

    function overrideValue(role, field) {
        const roles = preferences.roles || ({})
        return roles[role] && roles[role][field] !== undefined ? roles[role][field] : undefined
    }

    component FontRow: Form.SettingRow {
        id: row
        required property string preferenceKey
        property string roleName: ""
        property string effectiveFamily: ""
        readonly property var chosen: roleName === "" ? root.preferences[preferenceKey]
            : root.overrideValue(roleName, "family")
        Controls.SelectButton {
            objectName: row.roleName === "" ? "typographyPicker-" + row.preferenceKey
                : "typographyRole-" + row.roleName + "-family"
            width: Math.min(240, root.width * 0.34)
            implicitWidth: width
            options: root.fontOptions
            selectedIndex: row.chosen === undefined ? 0 : root.fontOptions.indexOf(row.chosen)
            label: row.chosen === undefined
                ? I18n.tr("settings.typography.automatic_short", "Automatic") + " · " + row.effectiveFamily
                : String(row.chosen)
            onSelected: index => {
                if (index < 0 || index >= root.fontOptions.length)
                    return
                const family = index === 0 ? "" : root.fontOptions[index]
                if (row.roleName === "")
                    ThemeController.setFontFamilyPreference(row.preferenceKey, family)
                else
                    ThemeController.setTypographyPreference(row.roleName, "family", family)
            }
        }
    }

    component MetricRow: Form.SettingRow {
        id: row
        required property string roleName
        required property string metric
        required property var values
        property string suffix: ""
        readonly property var chosen: root.overrideValue(roleName, metric)
        readonly property var effective: {
            const value = Components.Theme.typography.role(roleName)[metric]
            return metric === "pixelSize" ? Math.round(value / ThemeController.textScale) : value
        }
        Controls.SelectButton {
            objectName: "typographyRole-" + row.roleName + "-" + row.metric
            width: Math.min(190, root.width * 0.3)
            implicitWidth: width
            options: [I18n.tr("settings.typography.automatic_theme", "Automatic (theme)")]
                .concat(row.values.map(value => String(value) + row.suffix))
            selectedIndex: row.chosen === undefined ? 0 : row.values.indexOf(Number(row.chosen)) + 1
            label: row.chosen === undefined
                ? I18n.tr("settings.typography.automatic_short", "Automatic") + " · " + row.effective + row.suffix
                : String(row.chosen) + row.suffix
            onSelected: index => {
                if (index >= 0 && index <= row.values.length)
                    ThemeController.setTypographyPreference(row.roleName, row.metric,
                        index === 0 ? null : row.values[index - 1])
            }
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        maxWidth: 850
        contentMargins: 32

        Form.PageHeader {
            title: I18n.tr("settings.typography.title", "Typography")
            subtitle: I18n.tr("settings.typography.subtitle", "Customize the typography of your Astrea desktop.")
        }
        Form.SectionHeader { text: I18n.tr("settings.typography.preview", "LIVE PREVIEW"); Layout.bottomMargin: 12 }
        Rectangle {
            objectName: "typographyPreview"
            Layout.fillWidth: true
            implicitHeight: previewColumn.implicitHeight + 44
            Layout.bottomMargin: 24
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder

            ColumnLayout {
                id: previewColumn
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 22 }
                spacing: 10
                Shared.DisplayText {
                    objectName: "typographyPreviewTitle"
                    Layout.fillWidth: true
                    text: I18n.tr("settings.typography.preview_title", "The quick brown fox")
                    color: Components.Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Shared.BodyText {
                    objectName: "typographyPreviewBody"
                    Layout.fillWidth: true
                    text: I18n.tr("settings.typography.preview_body", "A consistent interface starts with typography.")
                    color: Components.Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Shared.CaptionText {
                    Layout.fillWidth: true
                    text: I18n.tr("settings.typography.preview_status", "Interface · %1 · %2% scale")
                        .replace("%1", Components.Theme.fontFamily)
                        .replace("%2", Math.round(ThemeController.textScale * 100))
                    color: Components.Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
                Shared.LabelText {
                    Layout.fillWidth: true
                    text: "Aa Bb Cc 0123456789 — ÁÉÍÓÚ ç ã õ"
                    color: Components.Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Shared.MonoText {
                    Layout.fillWidth: true
                    text: "const font = 'Hello, Astrea';"
                    color: Components.Theme.accent
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            FontRow {
                preferenceKey: "fontFamily"
                effectiveFamily: Components.Theme.fontFamily
                label: I18n.tr("settings.typography.primary", "Primary font")
                sublabel: I18n.tr("settings.typography.primary_help", "Inherited by text, titles and controls unless overridden.")
            }
            FontRow {
                preferenceKey: "monoFontFamily"
                effectiveFamily: Components.Theme.monoFontFamily
                label: I18n.tr("settings.typography.mono", "Monospaced")
                sublabel: I18n.tr("settings.typography.mono_help", "Code, numerical data and technical values")
            }
            Form.SettingRow {
                label: I18n.tr("settings.typography.scale", "Text scale")
                sublabel: I18n.tr("settings.typography.scale_help", "Adjust text without enlarging icons or interface spacing.")
                isLast: true
                Controls.Slider {
                    objectName: "typographyScale"
                    width: Math.min(260, root.width * 0.36)
                    implicitWidth: width
                    from: 75; to: 200; stepSize: 5
                    modelValueEnabled: true
                    modelValue: ThemeController.textScale * 100
                    showEndpointGlyphs: false
                    showValueLabel: true
                    valueText: Math.round(value) + "%"
                    detentEnabled: true
                    detentValue: 100
                    Accessible.name: I18n.tr("settings.typography.scale", "Text scale")
                    onValueEdited: value => ThemeController.setTextScale(value / 100)
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            Form.SettingRow {
                label: I18n.tr("settings.typography.system", "System fonts (Fontconfig)")
                sublabel: I18n.tr("settings.typography.system_help", "Apply the current primary and monospaced families to other applications. Reapply after changing fonts or themes.")
                Row {
                    spacing: 8
                    Controls.SelectButton {
                        objectName: "typographySystemApply"
                        width: 150
                        label: I18n.tr("settings.typography.system_apply", "Apply to system")
                        isButton: true
                        showButtonIcon: false
                        onSelected: root.applySystemFonts(false)
                    }
                    Controls.SelectButton {
                        objectName: "typographySystemRestore"
                        width: 100
                        label: I18n.tr("settings.typography.system_restore", "Restore")
                        isButton: true
                        showButtonIcon: false
                        onSelected: root.applySystemFonts(true)
                    }
                }
            }
            Form.SettingRow {
                visible: root.systemStatus !== ""
                label: "Fontconfig"
                sublabel: root.systemStatus
                isLast: true
            }
        }

        Form.FormCard {
            Layout.bottomMargin: root.advancedExpanded ? 12 : 24
            Form.SettingRow {
                label: I18n.tr("settings.typography.advanced", "Advanced typography")
                sublabel: I18n.tr("settings.typography.advanced_help", "Override individual text roles and metrics.")
                isLast: true
                Controls.SelectButton {
                    objectName: "typographyAdvancedToggle"
                    width: 134
                    label: root.advancedExpanded
                        ? I18n.tr("settings.typography.hide", "Hide") : I18n.tr("settings.typography.show", "Show")
                    isButton: true
                    showButtonIcon: false
                    onSelected: root.advancedExpanded = !root.advancedExpanded
                }
            }
        }
        ColumnLayout {
            visible: root.advancedExpanded
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            spacing: 12
            Controls.SearchField {
                Layout.fillWidth: true
                objectName: "typographySearch"
                placeholderText: I18n.tr("settings.typography.search", "Search installed fonts…")
                onTextEdited: text => root.searchText = text
                onCleared: root.searchText = ""
            }
            Form.FormCard {
                FontRow {
                    preferenceKey: "fontFamilyDisplay"
                    effectiveFamily: Components.Theme.fontFamilyDisplay
                    label: I18n.tr("settings.typography.display", "Display")
                }
                FontRow {
                    preferenceKey: "fontFamilyText"
                    effectiveFamily: Components.Theme.fontFamilyText
                    label: I18n.tr("settings.typography.text", "Text")
                    isLast: true
                }
            }
            Repeater {
                model: root.roleNames
                delegate: ColumnLayout {
                    id: roleSection
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 8
                    Form.SectionHeader { text: root.roleLabels[roleSection.index] }
                    Form.FormCard {
                        FontRow {
                            preferenceKey: ""
                            roleName: roleSection.modelData
                            effectiveFamily: Components.Theme.typography.role(roleName).family
                            label: I18n.tr("settings.typography.family", "Font family")
                        }
                        MetricRow {
                            roleName: roleSection.modelData; metric: "pixelSize"
                            label: I18n.tr("settings.typography.base_size", "Base size")
                            sublabel: I18n.tr("settings.typography.base_size_help", "Before text scale is applied")
                            values: Array.from({length: 88}, (_, index) => index + 9)
                            suffix: " px"
                        }
                        MetricRow {
                            roleName: roleSection.modelData; metric: "weight"
                            label: I18n.tr("settings.typography.weight", "Weight")
                            values: [100, 200, 300, 400, 500, 600, 700, 800, 900]
                        }
                        MetricRow {
                            roleName: roleSection.modelData; metric: "letterSpacing"
                            label: I18n.tr("settings.typography.tracking", "Letter spacing")
                            values: [-2, -1, 0, 0.5, 1, 1.5, 2, 3, 4]
                            suffix: " px"
                        }
                        MetricRow {
                            roleName: roleSection.modelData; metric: "lineHeight"
                            label: I18n.tr("settings.typography.line_height", "Line height")
                            values: [1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.8, 2, 2.5, 3]
                            suffix: "×"
                            isLast: true
                        }
                    }
                }
            }
        }
        Shared.CaptionText {
            objectName: "typographyError"
            visible: ThemeController.typographyError !== ""
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            text: I18n.tr("settings.typography.save_error", "Could not save typography preferences. Check the preferences file and try again.")
            color: Components.Theme.errorColor
            wrapMode: Text.WordWrap
        }
        Form.FormCard {
            Form.SettingRow {
                isLast: true
                label: I18n.tr("settings.typography.restore", "Restore theme defaults")
                sublabel: I18n.tr("settings.typography.restore_help", "Remove typography overrides without changing the original theme.")
                Controls.SelectButton {
                    objectName: "typographyReset"
                    width: 134
                    label: I18n.tr("settings.typography.reset", "Reset")
                    isButton: true
                    showButtonIcon: false
                    onSelected: ThemeController.resetFontPreferences()
                }
            }
        }
    }
}
