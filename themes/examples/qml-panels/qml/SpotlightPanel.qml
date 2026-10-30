import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared

Rectangle {
    id: root
    objectName: "themeExampleSpotlightPanel"
    readonly property int themeApiVersion: 1
    property var controller
    property real radius: 22
    Shared.ThemeTokens { id: tokens }

    width: 600
    height: 64
    color: tokens.value("shellSurface", "#25212b")
    border.color: tokens.value("shellBorder", "#55505c")
    border.width: 1

    function focusSearch() { search.forceActiveFocus(); search.selectAll() }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        Text { text: "⌕"; color: tokens.value("shellTextSecondary", "#c8c3ce"); font.pixelSize: 24 }
        TextInput {
            id: search
            objectName: "themeExampleSpotlightSearch"
            Layout.fillWidth: true
            color: tokens.value("shellTextMain", "#ffffff")
            font.family: tokens.value("fontFamily", "Inter")
            font.pixelSize: 20
            text: root.controller ? root.controller.query : ""
            onTextEdited: if (root.controller) root.controller.scheduleSearch(text)
            Keys.onEscapePressed: if (root.controller) root.controller.close()
            Keys.onReturnPressed: if (root.controller) root.controller.activateCurrent()
            Component.onCompleted: if (root.controller && text !== root.controller.query) text = root.controller.query
        }
    }
}
