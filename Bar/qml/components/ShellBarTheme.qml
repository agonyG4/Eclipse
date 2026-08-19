import QtQuick

QtObject {
    readonly property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property bool isLight: state && state.themeMode === 1
    readonly property bool isTransparent: !state || state.shellStyle === 0
    readonly property bool isFrosted: state && state.shellStyle === 2

    readonly property color shellBackground: isLight
        ? (isTransparent ? Qt.rgba(1, 1, 1, 0.72) : Qt.rgba(0.98, 0.98, 0.99, 0.96))
        : (isTransparent ? Qt.rgba(0.08, 0.08, 0.09, 0.92) : Qt.rgba(0.11, 0.11, 0.12, 0.98))
    readonly property color shellSurface: isLight
        ? (isFrosted ? Qt.rgba(0.98, 0.99, 1, 0.36) : Qt.rgba(0, 0, 0, 0.08))
        : (isFrosted ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.07))
    readonly property color shellBorder: isLight
        ? Qt.rgba(0, 0, 0, isFrosted ? 0.10 : 0.08)
        : Qt.rgba(1, 1, 1, isFrosted ? 0.16 : 0.10)
    readonly property color shellHover: isLight
        ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.10)
    readonly property color shellPressed: isLight
        ? Qt.rgba(0, 0, 0, 0.14) : Qt.rgba(1, 1, 1, 0.15)
    readonly property color shellActive: isLight
        ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(1, 1, 1, 0.18)
    readonly property color shellTextMain: isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.94) : "#f5f5f7"
    readonly property color shellTextSecondary: isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.76) : Qt.rgba(1, 1, 1, 0.60)
    readonly property color shellTextDim: isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.50) : Qt.rgba(1, 1, 1, 0.65)
    readonly property color shellTextActive: isLight ? "#151619" : "#ffffff"
    readonly property color shellIconMain: isLight ? Qt.rgba(0.10, 0.12, 0.15, 0.72) : Qt.rgba(1, 1, 1, 0.65)
    readonly property color shellIconActive: isLight ? "#111214" : "#ffffff"
    readonly property color shellIconMuted: isLight ? Qt.rgba(0.10, 0.12, 0.15, 0.35) : Qt.rgba(1, 1, 1, 0.25)
    readonly property color shellIconAccent: state && state.accentHex ? state.accentHex : "#60aaff"
    readonly property color shellSeparator: isLight ? Qt.rgba(0.10, 0.12, 0.15, 0.22) : Qt.rgba(1, 1, 1, 0.30)

    readonly property real shellRadiusLarge: 14
    readonly property real shellRadiusMedium: 8
    readonly property real shellPillRadius: 999
    readonly property int workspaceDotSize: 10
    readonly property int workspaceActiveWidth: 32
    readonly property int pillHeight: 36
    readonly property int animationQuick: 140
    readonly property int animationPopover: 220
}
