import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "themesPage"

    readonly property var controller: SettingsController.themes
    readonly property int iconThemeRevision: {
        if (typeof AstreaIconProvider !== "undefined" && AstreaIconProvider)
            return AstreaIconProvider.themeRevision
        return 0
    }
    property string searchQuery: ""
    readonly property var filteredThemes: {
        const query = root.searchQuery.trim().toLowerCase()
        if (query === "")
            return root.controller.themes
        return root.controller.themes.filter(theme =>
            String(theme.name || "").toLowerCase().includes(query)
            || String(theme.id || "").toLowerCase().includes(query)
            || String(theme.comment || "").toLowerCase().includes(query))
    }
    readonly property var cards: {
        const values = [{
            id: "",
            name: I18n.tr("apps.settings.pages.themes.system_default", "System Default"),
            comment: I18n.tr("apps.settings.pages.themes.system_default_comment", "Use the active desktop icon theme"),
            source: "system",
            previewUrls: ["image://astrea-icon/folder?revision=" + root.iconThemeRevision]
        }]
        for (let index = 0; index < root.filteredThemes.length; ++index)
            values.push(root.filteredThemes[index])
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
        objectName: "themeCard-" + (card.descriptor.id || "system-default")
        readonly property bool selected: String(card.descriptor.id || "")
            === String(root.controller.selectedIconTheme || "")
        readonly property bool hovered: cardMouse.containsMouse

        activeFocusOnTab: true
        implicitHeight: 198
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
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Repeater {
                    model: card.descriptor.previewUrls || []
                    delegate: Image {
                        required property var modelData
                        width: 28
                        height: 28
                        source: modelData || ""
                        sourceSize: Qt.size(56, 56)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: source === "" ? 0 : 1
                        visible: source !== ""
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: card.descriptor.name || card.descriptor.id
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeLarge
                font.weight: card.selected
                    ? Components.Theme.fontWeightDemiBold : Components.Theme.fontWeightMedium
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: card.descriptor.comment || (card.descriptor.source === "user"
                    ? I18n.tr("apps.settings.pages.themes.user_theme", "User theme")
                    : I18n.tr("apps.settings.pages.themes.system_theme", "System theme"))
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
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
            onClicked: card.activate()
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Text {
            text: I18n.tr("apps.settings.pages.themes.title", "Themes")
            color: Components.Theme.textPrimary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeHeader
            font.weight: Components.Theme.fontWeightDemiBold
            Layout.fillWidth: true
            Layout.bottomMargin: 18
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.themes.icon_theme", "ICON THEME")
            Layout.bottomMargin: 12
        }

        Controls.SearchField {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            placeholderText: I18n.tr("apps.settings.pages.themes.search", "Search installed icon themes")
            onTextEdited: text => root.searchQuery = text
        }

        Text {
            visible: root.controller.busy
            text: I18n.tr("apps.settings.pages.themes.loading", "Loading installed themes…")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            visible: !root.controller.busy && root.controller.lastError !== ""
            text: root.controller.lastError
            color: Components.Theme.errorColor
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            objectName: "themes-empty-installed-catalog"
            visible: !root.controller.busy && root.controller.lastError === ""
                && root.controller.themes.length === 0 && root.searchQuery.trim() === ""
            text: I18n.tr("apps.settings.pages.themes.empty_catalog", "No icon themes are installed.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        Text {
            objectName: "themes-empty-search-results"
            visible: !root.controller.busy && root.controller.lastError === ""
                && root.filteredThemes.length === 0 && root.searchQuery.trim() !== ""
            text: I18n.tr("apps.settings.pages.themes.empty_search", "No installed themes match your search.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            Layout.fillWidth: true
            Layout.bottomMargin: 12
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 660 ? 2 : 1
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
    }

    Component.onCompleted: root.controller.refresh()
}
