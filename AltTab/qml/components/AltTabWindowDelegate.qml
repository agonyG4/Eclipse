import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Astrea.Shared as Shared
import "."

Item {
    id: root
    objectName: "altTabWindowDelegate"
    Shared.ShellMaterialTheme { id: shellMaterial }
    readonly property Shared.ThemeTokens themeTokens: shellMaterial.tokens
    readonly property color accent: themeTokens.value("accent", shellMaterial.iconAccent)

    property int windowIndex: 0
    property bool windowSelected: false
    property bool windowActive: false
    property bool windowHovered: false
    property string windowIconUrl: ""
    property string windowIconName: ""
    property string windowIconPath: ""
    property bool windowIconPending: false
    property bool windowShowFallbackText: true
    property string windowDisplayName: ""

    width: 92
    height: 92

    Rectangle {
        id: tileBg
        anchors.fill: parent
        radius: themeTokens.value("altTabRadius", 22)
        color: "transparent"

        Rectangle {
            id: selectionHighlight
            objectName: "altTabSelectionHighlight"
            anchors.fill: parent
            radius: parent.radius
            color: windowSelected
                ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.25)
                : "transparent"
            border.color: windowSelected
                ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.40)
                : "transparent"
            border.width: 1
            scale: windowSelected ? 1.06 : 1.0

            Behavior on color { ColorAnimation { duration: themeTokens.value("animationInstant", 90) } }
            Behavior on scale { NumberAnimation { duration: themeTokens.value("animationInstant", 90); easing.type: Easing.OutCubic } }
        }

        AstreaAppIcon {
            anchors.centerIn: parent
            width: windowSelected ? 84 : 72
            height: width
            iconSource: windowIconUrl
            iconName: windowIconName
            iconPath: windowIconPath
            iconPending: windowIconPending
            showFallbackText: windowShowFallbackText
            appName: windowDisplayName
            iconSize: width
            maximumPresentationLogicalSize: 84
            iconRadius: windowSelected ? 18 : 15
            fallbackRadius: iconRadius
            fallbackColor: shellMaterial.surface
            fallbackFontSize: themeTokens.value("fontSizeIconLarge", 27)

            Behavior on width { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on height { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on iconRadius { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                root.windowHovered = true
                AltTabController.preview(windowIndex)
            }
            onExited: root.windowHovered = false
            onClicked: AltTabController.commitIndex(windowIndex)
        }
    }
}
