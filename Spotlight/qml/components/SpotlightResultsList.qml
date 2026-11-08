import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared

ColumnLayout {
    id: root
    Shared.ShellMaterialTheme { id: shellMaterial; typographySurface: "spotlight" }
    readonly property Shared.ThemeTokens themeTokens: shellMaterial.tokens

    property string queryText: ""
    property string fontFamily: themeTokens.typography.role("body").family
    readonly property int resultRowHeight: Math.max(50, themeTokens.typography.role("body").pixelSize * themeTokens.typography.role("body").lineHeight + 16)
    readonly property int resultIconSize: 40
    readonly property int count: resultList.count

    signal launchRequested(int index)

    Layout.fillWidth: true
    visible: queryText.length > 0 && resultList.count > 0
    spacing: 0

    function move(delta) {
        if (resultList.count <= 0)
            return
        var next = (resultList.currentIndex + delta + resultList.count) % resultList.count
        SpotlightController.selectedIndex = next
    }

    function launchCurrent() {
        if (resultList.count > 0)
            root.launchRequested(SpotlightController.selectedIndex)
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: shellMaterial.surface
        Layout.topMargin: 12
        Layout.bottomMargin: 8
    }

    ListView {
        id: resultList

        Layout.fillWidth: true
        implicitHeight: Math.min(count, 6) * root.resultRowHeight
        model: SpotlightController.resultsModel
        currentIndex: SpotlightController.selectedIndex
        interactive: false
        clip: true

        delegate: Rectangle {
            id: resultDelegate

            required property int index
            required property string name
            required property string iconName
            required property string entryId
            required property string startupWmClass

            width: resultList.width
            height: root.resultRowHeight
            // Concentric with the panel corner across the 14 px content inset.
            radius: Math.max(6, themeTokens.value("spotlightRadius", 24) - 14)
            color: resultList.currentIndex === resultDelegate.index
                ? themeTokens.value("accent", shellMaterial.iconAccent) : "transparent"

            RowLayout {
                anchors {
                    fill: parent
                    leftMargin: 12
                    rightMargin: 12
                }
                spacing: themeTokens.value("spacingLarge", 15)

                AstreaAppIcon {
                    Layout.preferredWidth: root.resultIconSize
                    Layout.preferredHeight: root.resultIconSize
                    astreaIconName: resultDelegate.iconName
                    appName: resultDelegate.name
                    className: resultDelegate.startupWmClass
                    title: resultDelegate.name
                    iconRadius: 7
                    maximumPresentationLogicalSize: root.resultIconSize
                }

                Text {
                    text: resultDelegate.name
                    font: themeTokens.typography.font("body", root.fontFamily)
                    lineHeight: themeTokens.typography.role("body").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    renderType: Text.NativeRendering
                    color: shellMaterial.textMain
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: SpotlightController.selectedIndex = resultDelegate.index
                onClicked: root.launchRequested(resultDelegate.index)
            }
        }
    }
}
