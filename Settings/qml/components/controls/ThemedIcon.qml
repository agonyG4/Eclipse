import QtQuick
import QtQuick.Window
import ".." as Components

// Shared DPI-aware monochrome SVG icon for Eclipse Settings.
// Preserves source vector geometry and rasterizes at the actual display DPR.
// Application icons belong to AstreaAppIcon; they are not monochrome symbols.
Item {
    id: root
    property string iconKey: ""
    property url iconSource: ""
    // The app always provides SettingsController; standalone hosts (component
    // tests, other shells) may not, and then get the untinted source.
    readonly property var controller:
        typeof SettingsController !== "undefined" ? SettingsController : null
    property string iconTheme: controller ? controller.navigationIconTheme : ""
    property string iconWeight: controller ? controller.navigationIconWeight : ""
    property color tint: Components.Theme.textPrimary
    property bool showFallback: false
    property string fallbackGlyph: "\uf293"
    property string fallbackFontFamily: "JetBrainsMono Nerd Font"
    readonly property real physicalDpr: Math.max(1, Screen.devicePixelRatio || 1)
    readonly property bool ready: symbolImage.status === Image.Ready
    readonly property alias imageSource: symbolImage.source

    Image {
        id: symbolImage
        objectName: "themedIconImage"
        anchors.fill: parent
        source: {
            if (!root.controller)
                return root.iconSource
            return root.iconSource.toString() !== ""
                ? root.controller.tintedSymbolSource(root.iconSource, root.tint.toString())
                : root.controller.tintedSymbolUrl(
                    root.iconKey, root.iconTheme, root.iconWeight, root.tint.toString())
        }
        sourceSize.width: Math.max(1, Math.ceil(root.width * root.physicalDpr))
        sourceSize.height: Math.max(1, Math.ceil(root.height * root.physicalDpr))
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        // Keep translucent theme colors without an additional texture layer.
        opacity: root.tint.a
        asynchronous: true
        cache: true
        visible: status === Image.Ready
        Accessible.ignored: true
    }

    Text {
        anchors.centerIn: parent
        visible: root.showFallback && !root.ready
        text: root.fallbackGlyph
        color: root.tint
        font.family: root.fallbackFontFamily
        font.pixelSize: Math.round(Math.min(root.width, root.height) * 0.85)
        Accessible.ignored: true
    }
}
