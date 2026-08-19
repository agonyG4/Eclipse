import QtQuick

QtObject {
    readonly property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property bool isLight: state && state.themeMode === 1
    readonly property bool isTransparent: !state || state.shellStyle === 0
    readonly property bool isDefault: state && state.shellStyle === 1
    readonly property bool isFrosted: state && state.shellStyle === 2

    readonly property color shellBackground: isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06))
    readonly property color shellSurface: isLight
        ? (isDefault ? Qt.rgba(1, 1, 1, 0.86)
            : isFrosted ? Qt.rgba(0.98, 0.99, 1, 0.38)
            : Qt.rgba(1, 1, 1, 0.22))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.06))
    readonly property color shellSurfaceElevated: isLight
        ? (isDefault ? Qt.rgba(0.98, 0.98, 0.99, 1)
            : Qt.rgba(0.98, 0.98, 0.99, 0.92))
        : (isDefault ? Qt.rgba(0.11, 0.11, 0.12, 1)
            : Qt.rgba(0.11, 0.11, 0.12, 0.92))
    readonly property color shellBorder: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14))
    readonly property color shellBorderHover: isLight
        ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(1, 1, 1, 0.28)
    readonly property color shellHover: isLight
        ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08)
    readonly property color shellPressed: isLight
        ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12)
    readonly property color shellActive: isLight
        ? Qt.rgba(0, 122, 255, 0.14) : Qt.rgba(1, 1, 1, 0.15)
    readonly property color shellTextMain: isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.94) : "#f5f5f7"
    readonly property color shellTextSecondary: isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.68) : Qt.rgba(1, 1, 1, 0.60)
    readonly property color shellTextLight: isLight ? Qt.rgba(0.08, 0.09, 0.11, 0.86) : "#e0e0e5"
    readonly property color shellTextDim: isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.54) : Qt.rgba(1, 1, 1, 0.65)
    readonly property color shellTextActive: isLight ? Qt.rgba(0.04, 0.05, 0.06, 0.96) : "#ffffff"
    readonly property color shellIconMain: isLight ? Qt.rgba(0.10, 0.11, 0.13, 0.68) : Qt.rgba(1, 1, 1, 0.65)
    readonly property color shellIconActive: isLight ? Qt.rgba(0.03, 0.04, 0.05, 0.96) : "#ffffff"
    readonly property color shellIconMuted: isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.32) : Qt.rgba(1, 1, 1, 0.25)
    readonly property color shellIconWarning: "#ff375f"
    readonly property color shellIconAccent: state && state.accentHex ? state.accentHex : "#60aaff"
    readonly property color shellSeparator: Qt.rgba(1, 1, 1, 0.08)

    readonly property real shellRadiusLarge: 14
    readonly property real shellRadiusMedium: 8
    readonly property real shellPillRadius: 999
    readonly property int workspaceDotSize: 10
    readonly property int workspaceActiveWidth: 32
    readonly property int pillHeight: 36
    readonly property string shellFontFamily: "Inter"
    readonly property int shellFontWeightNormal: Font.Normal
    readonly property int shellFontWeightMedium: Font.Medium
    readonly property real shellClockTracking: 0
    readonly property int animationQuick: 120
    readonly property int animationNormal: 200
    readonly property int animationPopover: 300
}
