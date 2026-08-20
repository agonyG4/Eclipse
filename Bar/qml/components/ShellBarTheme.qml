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
    readonly property color shellSeparator: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(0, 0, 0, 0.065))
        : Qt.rgba(1, 1, 1, 0.08)

    // Borealis semantic aliases.  ThemeController remains the shared state
    // authority; these names keep the reference QML vocabulary intact.
    readonly property color background: shellBackground
    readonly property color surface: shellSurface
    readonly property color border: shellBorder
    readonly property color barBorderHover: shellBorderHover
    readonly property color separator: shellSeparator
    readonly property color hover: shellHover
    readonly property color pressed: shellPressed
    readonly property color active: shellActive
    readonly property color textMain: shellTextMain
    readonly property color textSecondary: shellTextSecondary
    readonly property color textLight: shellTextLight
    readonly property color textDim: shellTextDim
    readonly property color textActive: shellTextActive
    readonly property color iconMain: shellIconMain
    readonly property color iconActive: shellIconActive
    readonly property color iconMuted: shellIconMuted
    readonly property color iconWarning: shellIconWarning
    readonly property color iconAccent: shellIconAccent

    readonly property color popupBackground: shellSurfaceElevated
    readonly property color popupBorder: shellBorder

    readonly property real shellRadiusLarge: 14
    readonly property real shellRadiusMedium: 8
    readonly property real shellRadiusSmall: 6
    readonly property real shellControlRadius: 10
    readonly property real shellTileRadius: 12
    readonly property real shellPillRadius: 999
    readonly property int workspaceDotSize: 10
    readonly property int workspaceActiveWidth: 32
    readonly property color workspaceActive: isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.88) : "#ffffff"
    readonly property color workspaceInactive: isLight
        ? Qt.rgba(0.05, 0.06, 0.07, 0.22) : Qt.rgba(1, 1, 1, 0.22)
    readonly property int pillHeight: 36
    readonly property string fontFamily: "Inter Variable"
    readonly property string fontFamilyDisplay: "Inter Display"
    readonly property string fontFamilyText: "Inter Regular"
    readonly property string iconFontFamily: "JetBrainsMonoNL Nerd Font"
    readonly property string shellFontFamily: fontFamily
    readonly property int fontSizeLarge: 18
    readonly property int fontSizeTitle: 13
    readonly property int fontSizeBody: 12
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeExtraSmall: 10
    readonly property int fontSizeMicro: 9
    readonly property int fontSizeIcon: 16
    readonly property int fontSizeIconLarge: 18
    readonly property real spacingTiny: 3
    readonly property real spacingMicro: 4
    readonly property real spacingSmall: 6
    readonly property real spacingInset: 7
    readonly property real spacing: 8
    readonly property real spacingMedium: 10
    readonly property real spacingControlGap: 9
    readonly property real spacingLarge: 12
    readonly property real spacingXLarge: 14
    readonly property real spacingXXLarge: 20
    readonly property real spacingContainer: 16
    readonly property int shellFontWeightNormal: Font.Normal
    readonly property int shellFontWeightMedium: Font.Medium
    readonly property real shellClockTracking: 0
    readonly property int animationSlider: 60
    readonly property int animationInstant: 90
    readonly property int animationMicro: 100
    readonly property int animationQuick: 120
    readonly property int animationSubtle: 130
    readonly property int animationHover: 140
    readonly property int animationFast: 150
    readonly property int animationStandard: 160
    readonly property int animationNormal: 200
    readonly property int animationSlow: 400
    readonly property int animationPulse: 900
    readonly property int animationSpin: 1200
    readonly property int animationPopover: 300
    readonly property real opacityMuted: 0.80
    readonly property real opacitySecondary: 0.85
    readonly property real opacitySubtle: 0.86
    readonly property real opacityEmphasis: 0.90
    readonly property real opacityDragging: 0.94
}
