pragma Singleton
import QtQuick
import Astrea.Settings 1.0 as Borealis
import Astrea.Shared as Shared

QtObject {
    readonly property Shared.ThemeTokens themeTokens: Shared.ThemeTokens { state: Borealis.State }
    readonly property bool isLight: Borealis.State.themeMode === 1
    readonly property bool isTransparent: Borealis.State.shellStyle === 0
    readonly property bool isDefault: Borealis.State.shellStyle === 1
    readonly property bool isFrosted: Borealis.State.shellStyle === 2

    readonly property color background: themeTokens.value("shellBackground", isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06)))
    readonly property color surface: themeTokens.value("shellSurface", isLight
        ? (isDefault ? Qt.rgba(1, 1, 1, 0.86)
            : isFrosted ? Qt.rgba(0.98, 0.99, 1, 0.38)
            : Qt.rgba(1, 1, 1, 0.22))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.06)))
    readonly property color border: themeTokens.value("shellBorder", isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14)))
    readonly property color separator: themeTokens.value("shellSeparator", Qt.rgba(1, 1, 1, 0.08))
    readonly property color hover: themeTokens.value("shellHover", isLight ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08))
    readonly property color pressed: themeTokens.value("shellPressed", isLight ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12))
    readonly property color active: themeTokens.value("shellActive", isLight ? Qt.rgba(0, 122, 255, 0.14) : Qt.rgba(1, 1, 1, 0.15))
    readonly property color barBorderHover: themeTokens.value("shellBorderHover", isLight ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(1, 1, 1, 0.28))
    readonly property color islandBackground: "#000000"

    readonly property color textMain: themeTokens.value("shellTextMain", isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.94) : "#f5f5f7")
    readonly property color textSecondary: themeTokens.value("shellTextSecondary", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.68) : Qt.rgba(1, 1, 1, 0.60))
    readonly property color textLight: themeTokens.value("shellTextLight", isLight ? Qt.rgba(0.08, 0.09, 0.11, 0.86) : "#e0e0e5")
    readonly property color textDim: themeTokens.value("shellTextDim", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.54) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color textActive: themeTokens.value("shellTextActive", isLight ? Qt.rgba(0.04, 0.05, 0.06, 0.96) : "#ffffff")

    readonly property color iconMain: themeTokens.value("shellIconMain", isLight ? Qt.rgba(0.10, 0.11, 0.13, 0.68) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color iconActive: themeTokens.value("shellIconActive", isLight ? Qt.rgba(0.03, 0.04, 0.05, 0.96) : "#ffffff")
    readonly property color iconMuted: themeTokens.value("shellIconMuted", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.32) : Qt.rgba(1, 1, 1, 0.25))
    readonly property color iconWarning: themeTokens.value("shellIconWarning", "#ff375f")
    readonly property color iconAccent: themeTokens.value("shellIconAccent", Borealis.State.accentHex)

    readonly property real radiusLarge: themeTokens.value("shellRadiusLarge", 14)
    readonly property real radiusMedium: themeTokens.value("shellRadiusMedium", 8)
    readonly property real radiusSmall: themeTokens.value("shellRadiusSmall", 6)
    readonly property real controlRadius: themeTokens.value("shellControlRadius", 10)
    readonly property real tileRadius: themeTokens.value("shellTileRadius", 12)
    readonly property real pillRadius: themeTokens.value("shellPillRadius", 999)

    readonly property int workspaceDotSize: 10
    readonly property int workspaceActiveWidth: 32
}
