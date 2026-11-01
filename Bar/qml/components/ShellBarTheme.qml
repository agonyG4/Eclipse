import QtQuick
import Astrea.Shared as Shared

QtObject {
    readonly property QtObject material: Shared.ShellMaterialTheme { typographySurface: "topbar" }

    readonly property var state: material.state
    readonly property var packStyle: state && state.styleTokens ? state.styleTokens : ({})
    readonly property bool useAppleIcons: Boolean(state && state.navigationIconTheme === "apple")

    function iconKeyForGlyph(glyph) {
        const keys = {
            "󰖩": "wifi", "󰖪": "wifi-off", "󰈀": "wired",
            "󰂯": "bluetooth", "󰂲": "bluetooth-off",
            "󰕾": "volume-high", "󰖀": "volume-high",
            "󰕿": "volume-low", "󰝟": "volume-muted",
            "󰍉": "search", "󰋖": "about", "󰍜": "settings",
            "󰅙": "force-quit", "󰷛": "lock", "󰐥": "power",
            "󰇚": "download", "󰕒": "upload", "󰑐": "refresh",
            "󰅂": "chevron", "󰄲": "check", "󰡖": "check",
            "󰐕": "radio"
        }
        return keys[String(glyph)] || ""
    }
    readonly property bool isLight: material.isLight
    readonly property bool isTransparent: material.isTransparent
    readonly property bool isDefault: material.isDefault
    readonly property bool isFrosted: material.isFrosted

    readonly property color shellBackground: material.tokens.value("shellBackground", material.background)
    readonly property color shellSurface: material.tokens.value("shellSurface", material.surface)
    readonly property color shellSurfaceElevated: material.surfaceElevated
    readonly property color shellBorder: material.tokens.value("shellBorder", material.border)
    readonly property color shellBorderHover: material.borderHover
    readonly property color shellHover: material.hover
    readonly property color shellPressed: material.pressed
    readonly property color shellActive: material.active
    readonly property color shellTextMain: material.tokens.value("shellTextMain", isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.94) : "#f5f5f7")
    readonly property color shellTextSecondary: material.tokens.value("shellTextSecondary", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.68) : Qt.rgba(1, 1, 1, 0.60))
    readonly property color shellTextLight: material.tokens.value("shellTextLight", isLight ? Qt.rgba(0.08, 0.09, 0.11, 0.86) : "#e0e0e5")
    readonly property color shellTextDim: material.tokens.value("shellTextDim", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.54) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color shellTextActive: material.textActive
    readonly property color shellIconMain: material.tokens.value("shellIconMain", isLight ? Qt.rgba(0.10, 0.11, 0.13, 0.68) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color shellIconActive: material.tokens.value("shellIconActive", isLight ? Qt.rgba(0.03, 0.04, 0.05, 0.96) : "#ffffff")
    readonly property color shellIconMuted: material.tokens.value("shellIconMuted", isLight ? Qt.rgba(0.13, 0.15, 0.18, 0.32) : Qt.rgba(1, 1, 1, 0.25))
    readonly property color shellIconWarning: material.tokens.value("shellIconWarning", "#ff375f")
    readonly property color shellIconAccent: material.tokens.value("shellIconAccent", state && state.accentHex ? state.accentHex : "#60aaff")
    readonly property color shellSeparator: material.separator

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

    readonly property color popupBackground: background
    readonly property color popupBorder: border

    readonly property real shellRadiusLarge: material.radiusLarge
    readonly property real shellRadiusMedium: material.radiusMedium
    readonly property real shellRadiusSmall: material.tokens.value("shellRadiusSmall", 6)
    readonly property real shellControlRadius: material.tokens.value("shellControlRadius", 10)
    readonly property real shellTileRadius: material.tokens.value("shellTileRadius", 12)
    readonly property real shellPillRadius: material.tokens.value("shellPillRadius", 999)
    readonly property int workspaceDotSize: 10
    readonly property int workspaceActiveWidth: 32
    readonly property color workspaceActive: material.tokens.value("workspaceActive", isLight ? Qt.rgba(0.05, 0.06, 0.07, 0.88) : "#ffffff")
    readonly property color workspaceInactive: material.tokens.value("workspaceInactive", isLight
        ? Qt.rgba(0.05, 0.06, 0.07, 0.22) : Qt.rgba(1, 1, 1, 0.22))
    readonly property int pillHeight: 36
    readonly property var typography: material.typography
    readonly property string fontFamily: typography.role("label").family
    readonly property string fontFamilyDisplay: typography.role("display").family
    readonly property string fontFamilyText: typography.role("body").family
    readonly property string iconFontFamily: material.tokens.value("iconFontFamily", "JetBrainsMonoNL Nerd Font")
    readonly property string shellFontFamily: fontFamily
    readonly property int fontSizeLarge: typography.role("title").pixelSize
    readonly property int fontSizeTitle: typography.role("title").pixelSize
    readonly property int fontSizeBody: typography.role("body").pixelSize
    readonly property int fontSizeSmall: typography.role("caption").pixelSize
    readonly property int fontSizeCaption: material.fontSizeCaption
    readonly property int fontSizeExtraSmall: typography.role("caption").pixelSize
    readonly property int fontSizeMicro: typography.role("caption").pixelSize
    readonly property int fontSizeIcon: material.tokens.value("fontSizeIcon", 16)
    readonly property int fontSizeIconLarge: material.tokens.value("fontSizeIconLarge", 18)
    readonly property real spacingTiny: material.tokens.value("spacingTiny", 3)
    readonly property real spacingMicro: material.tokens.value("spacingMicro", 4)
    readonly property real spacingSmall: material.tokens.value("spacingSmall", 6)
    readonly property real spacingInset: 7
    readonly property real spacing: material.tokens.value("spacing", 8)
    readonly property real spacingMedium: material.tokens.value("spacingMedium", 10)
    readonly property real spacingControlGap: 9
    readonly property real spacingLarge: material.tokens.value("spacingLarge", 12)
    readonly property real spacingXLarge: material.tokens.value("spacingXLarge", 14)
    readonly property real spacingXXLarge: 20
    readonly property real spacingContainer: 16
    readonly property int shellFontWeightNormal: Font.Normal
    readonly property int shellFontWeightMedium: Font.Medium
    readonly property real shellClockTracking: 0
    readonly property int animationSlider: material.tokens.value("animationSlider", 60)
    readonly property int animationInstant: material.tokens.value("animationInstant", 90)
    readonly property int animationMicro: material.tokens.value("animationMicro", 100)
    readonly property int animationQuick: material.tokens.value("animationQuick", 120)
    readonly property int animationSubtle: material.tokens.value("animationSubtle", 130)
    readonly property int animationHover: material.tokens.value("animationHover", 140)
    readonly property int animationFast: material.tokens.value("animationFast", 150)
    readonly property int animationStandard: material.tokens.value("animationStandard", 160)
    readonly property int animationNormal: material.tokens.value("animationNormal", 200)
    readonly property int animationSlow: material.tokens.value("animationSlow", 400)
    readonly property int animationPulse: material.tokens.value("animationPulse", 900)
    readonly property int animationSpin: material.tokens.value("animationSpin", 1200)
    readonly property int animationPopover: material.tokens.value("animationPopover", 300)
    readonly property real opacityMuted: 0.80
    readonly property real opacitySecondary: 0.85
    readonly property real opacitySubtle: 0.86
    readonly property real opacityEmphasis: 0.90
    readonly property real opacityDragging: 0.94
}
