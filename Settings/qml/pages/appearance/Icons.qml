import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "iconsPage"

    readonly property var controller: SettingsController.icons
    readonly property var appleIconWeights: [
        { id: "ultralight", name: "Ultralight" },
        { id: "thin", name: "Thin" },
        { id: "light", name: "Light" },
        { id: "regular", name: "Regular" },
        { id: "medium", name: "Medium" },
        { id: "semibold", name: "Semibold" },
        { id: "bold", name: "Bold" },
        { id: "heavy", name: "Heavy" },
        { id: "black", name: "Black" }
    ]
    readonly property bool catalogLoading: root.controller.busy && root.controller.refreshing
    readonly property int iconThemeRevision: {
        if (typeof AstreaIconProvider !== "undefined" && AstreaIconProvider)
            return AstreaIconProvider.themeRevision
        return 0
    }
    property string searchQuery: ""
    readonly property var filteredIconThemes: {
        const query = root.searchQuery.trim().toLowerCase()
        if (query === "")
            return root.controller.iconThemes
        return root.controller.iconThemes.filter(theme =>
            String(theme.name || "").toLowerCase().includes(query)
            || String(theme.id || "").toLowerCase().includes(query)
            || String(theme.comment || "").toLowerCase().includes(query))
    }
    readonly property var systemDefaultCard: ({
        id: "",
        name: I18n.tr("apps.settings.pages.icons.system_default", "System Default"),
        comment: I18n.tr("apps.settings.pages.icons.system_default_comment", "Use the active desktop icon theme"),
        source: "system",
        previewUrls: ["image://astrea-icon/folder?revision=" + root.iconThemeRevision]
    })
    readonly property var currentTheme: {
        const selected = String(root.controller.selectedIconTheme || "")
        if (selected === "")
            return root.systemDefaultCard
        return root.controller.iconThemes.find(theme => theme.id === selected) || root.systemDefaultCard
    }
    readonly property var cards: {
        const values = [root.systemDefaultCard]
        for (let index = 0; index < root.filteredIconThemes.length; ++index)
            values.push(root.filteredIconThemes[index])
        return values
    }

    function activateTheme(descriptor) {
        if (descriptor.id === "")
            root.controller.useSystemDefault()
        else
            root.controller.setIconTheme(descriptor.id)
    }

    component ThemeCard: FocusScope {
        id: card
        required property var descriptor
        objectName: "iconThemeCard-" + (card.descriptor.id || "system-default")
        readonly property bool selected: String(card.descriptor.id || "")
            === String(root.controller.selectedIconTheme || "")
        readonly property bool hovered: cardMouse.containsMouse

        readonly property bool hasPreview: (card.descriptor.previewUrls || []).some(url => url !== "")

        activeFocusOnTab: true
        implicitHeight: 128
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        Accessible.role: Accessible.Button
        Accessible.name: card.descriptor.name || card.descriptor.id

        function activate() {
            card.forceActiveFocus()
            root.activateTheme(card.descriptor)
        }

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                card.activate()
                event.accepted = true
            }
        }

        Rectangle {
            id: cardSurface
            anchors.fill: parent
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: card.selected || card.activeFocus ? 2 : 1
            border.color: card.selected || card.activeFocus
                ? Components.Theme.accent : Components.Theme.cardBorder
            scale: cardMouse.pressed ? 0.985 : 1

            Behavior on border.color {
                ColorAnimation { duration: Components.Theme.animationQuick }
            }
            Behavior on scale {
                NumberAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.fill: cardSurface
            anchors.margins: 2
            radius: Components.Theme.cardRadius
            color: card.selected
                ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g,
                          Components.Theme.accent.b, card.hovered ? 0.07 : 0.045)
                : card.hovered
                    ? (Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.035)
                                                : Qt.rgba(1, 1, 1, 0.055))
                    : "transparent"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Row {
                visible: card.hasPreview
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                spacing: 10
                Repeater {
                    model: card.descriptor.previewUrls || []
                    delegate: Image {
                        required property var modelData
                        width: 32
                        height: 32
                        source: modelData || ""
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: source === "" ? 0 : 1
                        visible: source !== ""
                    }
                }
            }

            Text {
                visible: !card.hasPreview
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 12
                text: I18n.tr("apps.settings.pages.icons.no_preview", "No preview available")
                color: Components.Theme.textTertiary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: card.descriptor.name || card.descriptor.id
                color: Components.Theme.textPrimary
                font.family: Components.Theme.typography.role("body").family
                font.pixelSize: Components.Theme.typography.role("body").pixelSize
                lineHeight: Components.Theme.typography.role("body").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                font.weight: card.selected
                    ? Components.Theme.fontWeightDemiBold : Components.Theme.fontWeightMedium
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: card.descriptor.comment || (card.descriptor.source === "user"
                    ? I18n.tr("apps.settings.pages.icons.user_theme", "User theme")
                    : I18n.tr("apps.settings.pages.icons.system_theme", "System theme"))
                color: Components.Theme.textSecondary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                opacity: Components.Theme.opacitySecondary
            }
        }

        Rectangle {
            visible: card.selected
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
                font.family: Components.Theme.iconFontFamily
                font.pixelSize: 10
                font.weight: Components.Theme.fontWeightBold
            }
        }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: card.activate()
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("apps.settings.pages.icons.title", "Icons")
            subtitle: I18n.tr("apps.settings.pages.icons.subtitle", "Icons for Settings and for your apps.")
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.icons.icon_theme", "ICON THEME")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "currentIconThemeCard"
            Layout.bottomMargin: 16

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingXLarge
                Layout.topMargin: 18
                Layout.bottomMargin: 18
                spacing: Components.Theme.spacingXLarge

                Row {
                    spacing: 12
                    Repeater {
                        model: (root.currentTheme.previewUrls || []).filter(url => url !== "")
                        delegate: Image {
                            required property var modelData
                            width: 44
                            height: 44
                            source: modelData
                            sourceSize: Qt.size(88, 88)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 4
                    Text {
                        objectName: "currentIconThemeName"
                        Layout.fillWidth: true
                        text: root.currentTheme.name || root.currentTheme.id
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.typography.role("label").family
                        font.pixelSize: Components.Theme.typography.role("label").pixelSize
                        lineHeight: Components.Theme.typography.role("label").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
                        font.weight: Components.Theme.typography.role("label").weight
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.currentTheme.comment || ""
                        visible: text !== ""
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.typography.role("caption").family
                        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                        lineHeight: Components.Theme.typography.role("caption").lineHeight
                        lineHeightMode: Text.ProportionalHeight
                        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: I18n.tr("apps.settings.pages.icons.in_use", "In use")
                    color: Components.Theme.accent
                    font.family: Components.Theme.typography.role("caption").family
                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                    font.weight: Components.Theme.typography.role("caption").weight
                }
            }
        }

        Controls.SearchField {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            placeholderText: I18n.tr("apps.settings.pages.icons.search", "Search installed icon themes")
            onTextEdited: text => root.searchQuery = text
        }

        Text {
            visible: root.catalogLoading
            text: I18n.tr("apps.settings.pages.icons.loading", "Loading installed themes…")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("body").family
            font.pixelSize: Components.Theme.typography.role("body").pixelSize
            lineHeight: Components.Theme.typography.role("body").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            visible: !root.catalogLoading && root.controller.lastError !== ""
            text: root.controller.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.typography.role("body").family
            font.pixelSize: Components.Theme.typography.role("body").pixelSize
            lineHeight: Components.Theme.typography.role("body").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            objectName: "icons-empty-installed-catalog"
            visible: !root.catalogLoading && root.controller.lastError === ""
                && root.controller.iconThemes.length === 0 && root.searchQuery.trim() === ""
            text: I18n.tr("apps.settings.pages.icons.empty_catalog", "No icon themes are installed.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("body").family
            font.pixelSize: Components.Theme.typography.role("body").pixelSize
            lineHeight: Components.Theme.typography.role("body").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            objectName: "icons-empty-search-results"
            visible: !root.catalogLoading && root.controller.lastError === ""
                && root.filteredIconThemes.length === 0 && root.searchQuery.trim() !== ""
            text: I18n.tr("apps.settings.pages.icons.empty_search", "No installed themes match your search.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("body").family
            font.pixelSize: Components.Theme.typography.role("body").pixelSize
            lineHeight: Components.Theme.typography.role("body").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            columns: width >= 780 ? 3 : width >= 500 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: root.cards
                delegate: ThemeCard {
                    required property var modelData
                    descriptor: modelData
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.icons.interface_theme", "SETTINGS ICONS")
            Layout.bottomMargin: 12
        }

        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            text: I18n.tr("settings.icons.interface_theme_help",
                          "Choose the icons used in Settings navigation. This does not change application icons.")
            wrapMode: Text.WordWrap
            color: Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            columns: width >= 540 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: SettingsController.availableNavigationIconPacks().map(pack => ({
                    id: pack.id, title: pack.name, detail: pack.description
                }))
                delegate: FocusScope {
                    id: navigationThemeOption
                    required property var modelData
                    objectName: "navigationIconTheme-" + modelData.id
                    readonly property bool active:
                        SettingsController.navigationIconTheme === modelData.id
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.title
                    Keys.onReturnPressed: SettingsController.setNavigationIconTheme(modelData.id)
                    Keys.onSpacePressed: SettingsController.setNavigationIconTheme(modelData.id)

                    Rectangle {
                        anchors.fill: parent
                        radius: Components.Theme.cardRadius
                        color: Components.Theme.cardBg
                        border.width: navigationThemeOption.active || navigationThemeOption.activeFocus ? 2 : 1
                        border.color: navigationThemeOption.active || navigationThemeOption.activeFocus
                            ? Components.Theme.accent : Components.Theme.cardBorder
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 14

                        Item {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42

                            Controls.ThemedIcon {
                                anchors.centerIn: parent
                                width: 34
                                height: 34
                                iconKey: "system"
                                iconTheme: navigationThemeOption.modelData.id
                                tint: Components.Theme.textPrimary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: navigationThemeOption.modelData.title
                                font.family: Components.Theme.typography.role("body").family
                                font.pixelSize: Components.Theme.typography.role("body").pixelSize
                                lineHeight: Components.Theme.typography.role("body").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                                font.weight: Components.Theme.typography.role("body").weight
                                color: Components.Theme.textPrimary
                            }

                            Text {
                                text: navigationThemeOption.modelData.detail
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                color: Components.Theme.textSecondary
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SettingsController.setNavigationIconTheme(
                                       navigationThemeOption.modelData.id)
                    }
                }
            }
        }

        Form.SectionHeader {
            visible: SettingsController.navigationIconTheme === "lucide"
            text: I18n.tr("settings.icons.stroke_weight", "LUCIDE STROKE WEIGHT")
            Layout.bottomMargin: 12
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            spacing: Components.Theme.spacingMedium

            visible: SettingsController.navigationIconTheme === "lucide"

            Controls.ThemedIcon {
                id: appleWeightPreview
                objectName: "appleIconWeightPreview"
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                iconKey: "appearance"
                tint: Components.Theme.textPrimary
            }

            Controls.SelectButton {
                id: appleWeightSelector
                objectName: "appleIconWeightSelector"
                Layout.fillWidth: true
                Layout.maximumWidth: 220
                enabled: SettingsController.navigationIconTheme === "lucide"
                options: root.appleIconWeights.map(weight => weight.name)
                selectedIndex: Math.max(0, root.appleIconWeights.findIndex(
                    weight => weight.id === SettingsController.navigationIconWeight))
                label: options[selectedIndex]
                onSelected: index => SettingsController.setNavigationIconWeight(
                    root.appleIconWeights[index].id)
            }

            Text {
                Layout.fillWidth: true
                text: I18n.tr("settings.icons.weight_help",
                    "Adjusts the stroke weight of the default Lucide icons. AppleOS uses its original SVG geometry.")
                wrapMode: Text.WordWrap
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                color: Components.Theme.textSecondary
            }
        }

    }

    Component.onCompleted: root.controller.refresh()
}
