import QtQuick
import Qt5Compat.GraphicalEffects as Legacy

Item {
    id: root
    property string iconKey: ""
    property color foreground: "#ffffff"
    readonly property bool themeAvailable: typeof ThemeController !== "undefined"
        && ThemeController !== null
    readonly property string iconWeight: themeAvailable ? ThemeController.navigationIconWeight : "regular"
    readonly property url resolvedUrl: {
        const weight = root.iconWeight
        const overrideRevision = themeAvailable ? ThemeController.navigationIconOverridesRevision : 0
        const theme = themeAvailable ? ThemeController.navigationIconTheme : "lucide"
        return themeAvailable && theme.length > 0 && ThemeController.themedNavigationIcon
            ? ThemeController.themedNavigationIcon(iconKey) : ""
    }
    readonly property bool hasIcon: resolvedUrl.toString() !== ""
    readonly property bool artworkReady: artwork.status === Image.Ready
    visible: hasIcon
    width: 16
    height: 16

    Image {
        id: artwork
        objectName: "barThemedIconArtwork"
        readonly property color resolvedTint: root.foreground
        anchors.fill: parent
        source: root.resolvedUrl
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        asynchronous: false
        cache: true
        visible: root.hasIcon
        layer.enabled: root.hasIcon
        layer.effect: Legacy.ColorOverlay {
            color: artwork.resolvedTint
        }
    }
}
