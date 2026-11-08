import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared
import "../../components" as Components
import "../../components/form" as Form
import "../../components/controls" as Controls
import "../../components/feedback" as Feedback

Item {
    id: root
    objectName: "appearancePage"
    readonly property var wallpaperController: SettingsController.wallpaper
    readonly property var visualThemeChoices: SettingsController.availableVisualThemes()

    Component.onCompleted: {
        if (root.wallpaperController
            && !root.wallpaperController.busy)
            root.wallpaperController.refresh()
    }

    component ChoiceCard: FocusScope {
        id: choiceCard

        property string choiceGroup: "appearance"
        property string choiceId: ""
        property string label: ""
        property string previewKind: "dark"
        readonly property bool selected: choiceGroup === "appearance"
            ? Components.Theme.themePreference === choiceId
            : Components.Theme.iconAppearance === choiceId
        readonly property bool hovered: cardMouse.containsMouse
        readonly property bool compactPreview: choiceGroup === "iconAppearance"

        activeFocusOnTab: true
        implicitHeight: compactPreview ? 142 : 168
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        Accessible.role: Accessible.Button
        Accessible.name: choiceCard.label

        function activate() {
            choiceCard.forceActiveFocus()
            if (choiceCard.choiceGroup === "appearance")
                SettingsController.appearance.setThemePreference(choiceCard.choiceId)
            else
                SettingsController.appearance.setIconAppearance(choiceCard.choiceId)
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                choiceCard.activate()
                event.accepted = true
            }
        }

        Rectangle {
            id: cardSurface
            anchors.fill: parent
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: choiceCard.selected || choiceCard.activeFocus ? 2 : 1
            border.color: choiceCard.selected || choiceCard.activeFocus
                ? Components.Theme.accent
                : Components.Theme.cardBorder
            scale: cardMouse.pressed ? 0.985 : 1.0

            Behavior on border.color {
                ColorAnimation {
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            id: selectionWash
            anchors.fill: parent
            anchors.margins: 2
            radius: Components.Theme.cardRadius
            color: choiceCard.selected
                ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g,
                          Components.Theme.accent.b, choiceCard.hovered ? 0.06 : 0.045)
                : choiceCard.hovered
                    ? (Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.035)
                                                : Qt.rgba(1, 1, 1, 0.055))
                    : "transparent"

            Behavior on color {
                ColorAnimation {
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.OutCubic
                }
            }
        }

        ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Item {
                    id: previewFrame
                    Layout.fillWidth: true
                    Layout.preferredHeight: choiceCard.compactPreview ? 72 : 112
                    clip: true

                    MaterialPreview {
                        visible: choiceCard.choiceGroup !== "iconAppearance"
                        objectName: "materialPreview-appearance-" + choiceCard.choiceId
                        anchors.fill: parent
                        wallpaperSource: root.wallpaperController
                            ? root.wallpaperController.effectivePreviewUrl : ""
                        wallpaperFit: root.wallpaperController
                            ? root.wallpaperController.effectiveFit : "cover"
                        controller: SettingsController.visualEffects
                        liveEffectEnabled: false
                        themeVariant: choiceCard.choiceGroup === "appearance"
                            ? choiceCard.previewKind
                            : (Components.Theme.isLight ? "light" : "dark")
                    }

                    Row {
                        visible: choiceCard.choiceGroup === "iconAppearance"
                        anchors.centerIn: parent
                        spacing: 6

                        // Real app icons, so each card shows the style the
                        // shell draws (the provider restyles app icons).
                        Repeater {
                            model: [
                                {asset: "display", icon: "org.gnome.Settings"},
                                {asset: "network", icon: "firefox"},
                                {asset: "sound", icon: "org.gnome.Nautilus"}
                            ]

                            delegate: Shared.AstreaAppIcon {
                                required property var modelData
                                objectName: "iconPreview-" + choiceCard.choiceId + "-" + modelData.asset
                                width: 32
                                height: 32
                                iconSize: 32
                                maximumPresentationLogicalSize: 32
                                iconName: modelData.icon
                                iconRadius: 0
                                appearanceOverride: choiceCard.choiceId
                                hasTintColorOverride: true
                                tintColorOverride: Components.Theme.accent
                                showFallbackText: false
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: choiceCard.label
                    color: Components.Theme.textPrimary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    font.weight: choiceCard.selected
                        ? Components.Theme.fontWeightDemiBold
                        : Components.Theme.fontWeightMedium
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }

            Feedback.SelectionBadge { selected: choiceCard.selected }

            MouseArea {
                id: cardMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: choiceCard.activate()
            }
        }
    }

    component AccentSwatch: FocusScope {
        id: accentSwatch

        property string accentValue: ""
        property string label: ""
        property color swatchColor: "transparent"
        readonly property bool selected: String(Components.Theme.accentHex).toLowerCase()
            === accentSwatch.accentValue.toLowerCase()
        readonly property bool hovered: swatchMouse.containsMouse

        activeFocusOnTab: true
        implicitWidth: 32
        implicitHeight: 34
        Layout.preferredWidth: 32
        Layout.preferredHeight: 34

        Accessible.role: Accessible.Button
        Accessible.name: accentSwatch.label

        function activate() {
            accentSwatch.forceActiveFocus()
            SettingsController.appearance.setAccentHex(accentSwatch.accentValue)
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                accentSwatch.activate()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 30
            height: 30
            radius: 15
            color: "transparent"
            border.width: accentSwatch.selected || accentSwatch.activeFocus ? 2
                : accentSwatch.hovered ? 1 : 0
            border.color: accentSwatch.selected || accentSwatch.activeFocus
                ? Components.Theme.accent
                : Components.Theme.cardBorder

            Behavior on border.color {
                ColorAnimation {
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.OutCubic
                }
            }
        }

        Rectangle {
            id: swatchSurface
            anchors.centerIn: parent
            width: 22
            height: 22
            radius: 11
            color: accentSwatch.swatchColor
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.18)
            scale: swatchMouse.pressed ? 0.9 : accentSwatch.hovered ? 1.06 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.OutCubic
                }
            }
        }

        Rectangle {
            visible: accentSwatch.selected
            anchors.centerIn: swatchSurface
            width: 16
            height: 16
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.16)

            Text {
                anchors.centerIn: parent
                text: "\uf00c"
                color: "white"
                font.family: Components.Theme.iconFontFamily
                font.pixelSize: 9
                font.weight: Components.Theme.fontWeightBold
            }
        }

        MouseArea {
            id: swatchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: accentSwatch.activate()
        }
    }

    Form.ScrollPage {
        id: scrollPage
        objectName: "appearanceScrollPage"
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.appearance.title", "Appearance")
            subtitle: I18n.tr("settings.appearance.subtitle", "Light or dark mode, accent color, icons and theme.")
        }

        Text {
            objectName: "appearancePersistenceError"
            visible: SettingsController.appearance.lastError !== ""
            text: SettingsController.appearance.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 10
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.appearance", "APPEARANCE")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            margins: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 10

                ChoiceCard {
                    objectName: "appearanceOption-auto"
                    choiceId: "auto"
                    label: I18n.tr("apps.settings.pages.appearance.option.automatic", "Automatic")
                    previewKind: "auto"
                }

                ChoiceCard {
                    objectName: "appearanceOption-light"
                    choiceId: "light"
                    label: I18n.tr("apps.settings.pages.appearance.option.light", "Light")
                    previewKind: "light"
                }

                ChoiceCard {
                    objectName: "appearanceOption-dark"
                    choiceId: "dark"
                    label: I18n.tr("apps.settings.pages.appearance.option.dark", "Dark")
                    previewKind: "dark"
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.accent_color", "ACCENT COLOR")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            margins: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingXLarge
                Layout.topMargin: 14
                Layout.bottomMargin: 14
                spacing: 16

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: I18n.tr("apps.settings.pages.appearance.text.accent_color_help", "Used for selections, switches and highlights")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    AccentSwatch {
                        objectName: "accentOption-blue"
                        accentValue: "#0a84ff"
                        label: I18n.tr("apps.settings.pages.appearance.option.blue", "Blue")
                        swatchColor: "#0a84ff"
                    }

                    AccentSwatch {
                        objectName: "accentOption-purple"
                        accentValue: "#bf5af2"
                        label: I18n.tr("apps.settings.pages.appearance.option.purple", "Purple")
                        swatchColor: "#bf5af2"
                    }

                    AccentSwatch {
                        objectName: "accentOption-red"
                        accentValue: "#ff453a"
                        label: I18n.tr("apps.settings.pages.appearance.option.red", "Red")
                        swatchColor: "#ff453a"
                    }

                    AccentSwatch {
                        objectName: "accentOption-orange"
                        accentValue: "#ff9f0a"
                        label: I18n.tr("apps.settings.pages.appearance.option.orange", "Orange")
                        swatchColor: "#ff9f0a"
                    }

                    AccentSwatch {
                        objectName: "accentOption-yellow"
                        accentValue: "#ffd60a"
                        label: I18n.tr("apps.settings.pages.appearance.option.yellow", "Yellow")
                        swatchColor: "#ffd60a"
                    }

                    AccentSwatch {
                        objectName: "accentOption-green"
                        accentValue: "#30d158"
                        label: I18n.tr("apps.settings.pages.appearance.option.green", "Green")
                        swatchColor: "#30d158"
                    }

                    AccentSwatch {
                        objectName: "accentOption-teal"
                        accentValue: "#40c8e0"
                        label: I18n.tr("apps.settings.pages.appearance.option.teal", "Teal")
                        swatchColor: "#40c8e0"
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.app_icons", "APP ICONS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            margins: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 5
                columnSpacing: 8
                rowSpacing: 10

                ChoiceCard {
                    objectName: "iconAppearance-default"
                    choiceGroup: "iconAppearance"
                    choiceId: "default"
                    label: I18n.tr("apps.settings.pages.appearance.option.default", "Default")
                }

                ChoiceCard {
                    objectName: "iconAppearance-dark"
                    choiceGroup: "iconAppearance"
                    choiceId: "dark"
                    label: I18n.tr("apps.settings.pages.appearance.option.dark", "Dark")
                }

                ChoiceCard {
                    objectName: "iconAppearance-clear"
                    choiceGroup: "iconAppearance"
                    choiceId: "clear"
                    label: I18n.tr("apps.settings.pages.appearance.option.clear", "Clear")
                }

                ChoiceCard {
                    objectName: "iconAppearance-tinted"
                    choiceGroup: "iconAppearance"
                    choiceId: "tinted"
                    label: I18n.tr("apps.settings.pages.appearance.option.tinted", "Tinted")
                }

                ChoiceCard {
                    objectName: "iconAppearance-monochrome"
                    choiceGroup: "iconAppearance"
                    choiceId: "monochrome"
                    label: I18n.tr("apps.settings.pages.appearance.option.monochrome", "Monochrome")
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.theme", "THEME")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            RowLayout {
                objectName: "systemTheme-astrea"
                readonly property bool selected: true
                Layout.fillWidth: true
                Layout.margins: 10
                spacing: 16

                Item {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 96

                    MaterialPreview {
                        objectName: "materialPreview-system-theme-astrea"
                        anchors.fill: parent
                        wallpaperSource: root.wallpaperController
                            ? root.wallpaperController.effectivePreviewUrl : ""
                        wallpaperFit: root.wallpaperController
                            ? root.wallpaperController.effectiveFit : "cover"
                        controller: SettingsController.visualEffects
                        liveEffectEnabled: false
                        themeVariant: Components.Theme.isLight ? "light" : "dark"
                    }

                    // Selected tile: accent border plus the shared corner badge.
                    Rectangle {
                        anchors.fill: parent
                        radius: Components.Theme.controlRadius
                        color: "transparent"
                        border.width: 2
                        border.color: Components.Theme.accent
                    }

                    Feedback.SelectionBadge { selected: true }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: I18n.tr("apps.settings.pages.appearance.system_theme.astrea", "Astrea")
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        font.weight: Components.Theme.fontWeightDemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: I18n.tr("apps.settings.pages.appearance.system_theme.astrea_description",
                                      "The built-in Astrea system theme")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingXLarge
                Layout.preferredHeight: 1
                color: Components.Theme.cardBorder
            }

            Form.SettingRow {
                label: I18n.tr("settings.appearance.theme_package.label", "Theme package")
                sublabel: I18n.tr("settings.appearance.theme_package_help",
                    "Custom themes can override colors, typography and icons. Icon packs remain independently selectable.")
                isLast: true
                Controls.SelectButton {
                    id: visualThemePicker
                    objectName: "visualThemePicker"
                    width: 220
                    options: root.visualThemeChoices.map(theme => theme.name)
                    selectedIndex: Math.max(0, root.visualThemeChoices.findIndex(
                        theme => theme.id === SettingsController.visualTheme))
                    label: options[selectedIndex] || "Astrea Default"
                    onSelected: index => SettingsController.setVisualTheme(
                        root.visualThemeChoices[index].id)
                }
            }
        }
    }
}
