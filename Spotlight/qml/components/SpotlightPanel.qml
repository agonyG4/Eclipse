import QtQuick
import Qt5Compat.GraphicalEffects as Legacy
import QtQuick.Layouts
import Astrea.Shared as Shared

Rectangle {
    id: root
    Shared.ThemeTokens { id: themeTokens; typographySurface: "spotlight" }

    readonly property color placeholderTextColor: themeTokens.value("shellTextSecondary", "#66FFFFFF")
    readonly property bool hasQuery: searchInput.text.length > 0
    readonly property var titleStyle: themeTokens.typography.role("title")
    readonly property string fontFamily: titleStyle.family


    width: 600
    height: hasQuery ? Math.min(contentColumn.implicitHeight + 28, 450) : Math.max(58, titleStyle.pixelSize * titleStyle.lineHeight + 28)
    radius: themeTokens.value("spotlightRadius", 24)
    color: themeTokens.value("shellBackground", "#80343434")
    border.color: themeTokens.value("shellBorder", "#33FFFFFF")
    border.width: 1
    clip: true

    scale: SpotlightController.open ? 1.0 : 0.98
    opacity: SpotlightController.open ? 1.0 : 0.0

    Behavior on height { NumberAnimation { duration: themeTokens.value("animationHover", 140); easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: themeTokens.value("animationStandard", 180); easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: themeTokens.value("animationQuick", 120) } }

    function focusSearch() {
        searchInput.forceActiveFocus()
        searchInput.selectAll()
    }

    ColumnLayout {
        id: contentColumn
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 14
        }
        spacing: themeTokens.value("spacingTiny", 0)

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(30, root.titleStyle.pixelSize * root.titleStyle.lineHeight)
            spacing: themeTokens.value("spacingMedium", 12)

            Text {
                visible: !appleSearch.visible
                text: "\u2315"
                font.family: root.fontFamily
                font.pixelSize: themeTokens.value("fontSizeIconLarge", 24)
                color: themeTokens.value("shellTextLight", "#99FFFFFF")
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignVCenter
            }

            Image {
                id: appleSearch
                readonly property bool appleSelected: typeof ThemeController !== "undefined"
                    && ThemeController
                source: {
                    if (!appleSelected)
                        return ""
                    const revision = ThemeController.navigationIconOverridesRevision
                    const theme = ThemeController.navigationIconTheme
                    return theme.length > 0 && ThemeController.navigationIconWeight.length > 0
                        && ThemeController.themedNavigationIcon
                        ? ThemeController.themedNavigationIcon("search") : ""
                }
                visible: source.toString() !== ""
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignVCenter
                sourceSize: Qt.size(24, 24)
                fillMode: Image.PreserveAspectFit
                opacity: 0.9
                mipmap: false
                // AppleOS symbols can have black paths. Always render
                // these navigation glyphs in the Spotlight foreground.
                layer.enabled: visible
                layer.effect: Legacy.ColorOverlay {
                    color: themeTokens.value("shellTextMain", "#ffffff")
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(30, root.titleStyle.pixelSize * root.titleStyle.lineHeight)
                Layout.alignment: Qt.AlignVCenter

                Text {
                    anchors.fill: parent
                    text: (AstreaI18n && AstreaI18n.messages && AstreaI18n.tr("spotlight.placeholder", "Spotlight Search")) || "Spotlight Search"
                    font: themeTokens.typography.font("title")
                    color: root.placeholderTextColor
                    verticalAlignment: Text.AlignVCenter
                    visible: searchInput.text.length === 0
                    renderType: Text.NativeRendering
                }

                TextInput {
                    id: searchInput
                    objectName: "spotlightSearchInput"
                    anchors.fill: parent
                    font: themeTokens.typography.font("title")
                    color: "white"
                    selectionColor: themeTokens.value("accent", "#407AFF")
                    selectedTextColor: themeTokens.value("accentForeground", "white")
                    clip: true
                    renderType: TextInput.NativeRendering
                    verticalAlignment: TextInput.AlignVCenter

                    onTextChanged: {
                        if (text !== SpotlightController.query)
                            SpotlightController.scheduleSearch(text)
                    }

                    Keys.onEscapePressed: SpotlightController.close()
                    Keys.onReturnPressed: resultsList.launchCurrent()
                    Keys.onDownPressed: resultsList.move(1)
                    Keys.onUpPressed: resultsList.move(-1)

                    Component.onCompleted: {
                        forceActiveFocus()
                        if (SpotlightController.query !== text)
                            text = SpotlightController.query
                    }

                    Connections {
                        target: SpotlightController
                        function onQueryChanged() {
                            if (searchInput.text !== SpotlightController.query)
                                searchInput.text = SpotlightController.query
                        }
                    }

                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus()
                            if (SpotlightController.query !== text)
                                text = SpotlightController.query
                        }
                    }
                }
            }

            SpotlightWeatherChip {
                id: weatherChip
                controller: SpotlightController
                textColor: root.placeholderTextColor
                fontFamily: themeTokens.typography.role("label").family
            }
        }

        SpotlightResultsList {
            id: resultsList
            queryText: searchInput.text
            fontFamily: themeTokens.typography.role("body").family
            onLaunchRequested: function(index) {
                SpotlightController.launch(index)
            }
        }
    }
}
