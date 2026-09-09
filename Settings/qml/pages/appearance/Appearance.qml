import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared
import "../../components" as Components
import "../../components/form" as Form

Item {
    id: root
    objectName: "appearancePage"
    readonly property var wallpaperController: SettingsController.wallpaper

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
        property int styleValue: -1
        property string previewKind: "dark"
        readonly property bool selected: choiceGroup === "appearance"
            ? Components.Theme.themePreference === choiceId
            : choiceGroup === "iconAppearance"
                ? Components.Theme.iconAppearance === choiceId
                : Components.Theme.shellStyle === styleValue
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
                Components.Theme.setThemePreference(choiceCard.choiceId)
            else if (choiceCard.choiceGroup === "iconAppearance")
                Components.Theme.setIconAppearance(choiceCard.choiceId)
            else
                Components.Theme.setShellStyle(choiceCard.styleValue)
            Components.Theme.save()
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
                        objectName: "materialPreview-" + choiceCard.choiceGroup + "-"
                            + (choiceCard.choiceGroup === "interface"
                                ? choiceCard.previewKind : choiceCard.choiceId)
                        anchors.fill: parent
                        wallpaperSource: root.wallpaperController
                            ? root.wallpaperController.effectivePreviewUrl : ""
                        wallpaperFit: root.wallpaperController
                            ? root.wallpaperController.effectiveFit : "cover"
                        themeVariant: choiceCard.choiceGroup === "appearance"
                            ? choiceCard.previewKind
                            : (Components.Theme.isLight ? "light" : "dark")
                        materialId: choiceCard.choiceGroup === "interface"
                            ? choiceCard.previewKind : "default"
                    }

                    Row {
                        visible: choiceCard.choiceGroup === "iconAppearance"
                        anchors.centerIn: parent
                        spacing: 10

                        Shared.AstreaAppIcon {
                            objectName: "iconPreview-" + choiceCard.choiceId + "-display"
                            width: 38
                            height: 38
                            iconSize: 38
                            maximumPresentationLogicalSize: 38
                            iconUrl: "qrc:/Astrea/Settings/assets/icons/settings/display.svg"
                            iconRadius: 8
                            appearanceOverride: choiceCard.choiceId
                            hasTintColorOverride: true
                            tintColorOverride: Components.Theme.accent
                            showFallbackText: false
                        }

                        Shared.AstreaAppIcon {
                            objectName: "iconPreview-" + choiceCard.choiceId + "-network"
                            width: 38
                            height: 38
                            iconSize: 38
                            maximumPresentationLogicalSize: 38
                            iconUrl: "qrc:/Astrea/Settings/assets/icons/settings/network.svg"
                            iconRadius: 8
                            appearanceOverride: choiceCard.choiceId
                            hasTintColorOverride: true
                            tintColorOverride: Components.Theme.accent
                            showFallbackText: false
                        }

                        Shared.AstreaAppIcon {
                            objectName: "iconPreview-" + choiceCard.choiceId + "-sound"
                            width: 38
                            height: 38
                            iconSize: 38
                            maximumPresentationLogicalSize: 38
                            iconUrl: "qrc:/Astrea/Settings/assets/icons/settings/sound.svg"
                            iconRadius: 8
                            appearanceOverride: choiceCard.choiceId
                            hasTintColorOverride: true
                            tintColorOverride: Components.Theme.accent
                            showFallbackText: false
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

            Rectangle {
                visible: choiceCard.selected
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: 18
                height: 18
                radius: 9
                color: Components.Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "\uf00c"
                    color: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: 10
                    font.weight: Components.Theme.fontWeightBold
                }
            }

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
            Components.Theme.setAccentHex(accentSwatch.accentValue)
            Components.Theme.save()
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
                font.family: Components.Theme.monoFontFamily
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
        maxWidth: 760

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.appearance", "APPEARANCE")
            Layout.bottomMargin: 10
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 18
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
            text: I18n.tr("apps.settings.pages.appearance.text.interface_style", "INTERFACE STYLE")
            Layout.bottomMargin: 10
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            margins: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 10

                ChoiceCard {
                    objectName: "interfaceStyleOption-default"
                    choiceGroup: "interface"
                    styleValue: 1
                    label: I18n.tr("apps.settings.pages.appearance.option.default", "Default")
                    previewKind: "default"
                }

                ChoiceCard {
                    objectName: "interfaceStyleOption-transparent"
                    choiceGroup: "interface"
                    styleValue: 0
                    label: I18n.tr("apps.settings.pages.appearance.option.transparent", "Transparent")
                    previewKind: "transparent"
                }

                ChoiceCard {
                    objectName: "interfaceStyleOption-frosted"
                    choiceGroup: "interface"
                    styleValue: 2
                    label: I18n.tr("apps.settings.pages.appearance.option.frosted", "Frosted")
                    previewKind: "frosted"
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.text.accent_color", "ACCENT COLOR")
            Layout.bottomMargin: 10
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            margins: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: I18n.tr("apps.settings.pages.appearance.text.accent_color_label", "Accent color")
                    color: Components.Theme.textPrimary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                    font.weight: Components.Theme.fontWeightMedium
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
            Layout.bottomMargin: 10
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            margins: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 10

                ChoiceCard {
                    objectName: "iconAppearance-default"
                    choiceGroup: "iconAppearance"
                    choiceId: "default"
                    label: I18n.tr("apps.settings.pages.appearance.option.default", "Default")
                }

                ChoiceCard {
                    objectName: "iconAppearance-monochrome"
                    choiceGroup: "iconAppearance"
                    choiceId: "monochrome"
                    label: I18n.tr("apps.settings.pages.appearance.option.monochrome", "Monochrome")
                }

                ChoiceCard {
                    objectName: "iconAppearance-tinted"
                    choiceGroup: "iconAppearance"
                    choiceId: "tinted"
                    label: I18n.tr("apps.settings.pages.appearance.option.tinted", "Tinted")
                }
            }
        }
    }
}
