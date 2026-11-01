import QtQuick
import Astrea.Shared as Shared

QtObject {
    readonly property Shared.ThemeTokens themeTokens: Shared.ThemeTokens { typographySurface: "menu" }
    readonly property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property bool isLight: state && state.themeMode === 1
    readonly property bool isDefault: state && state.shellStyle === 1
    readonly property bool isFrosted: state && state.shellStyle === 2

    readonly property color background: themeTokens.value("shellBackground", isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06)))
    readonly property color border: themeTokens.value("shellBorder", isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14)))
    readonly property color shellHover: themeTokens.value("shellHover", isLight
        ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08))
    readonly property color shellPressed: themeTokens.value("shellPressed", isLight
        ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12))
    readonly property color shellTextActive: themeTokens.value("shellTextActive", isLight
        ? Qt.rgba(0.04, 0.05, 0.06, 0.96) : "#ffffff")
    readonly property color shellTextSecondary: themeTokens.value("shellTextSecondary", isLight
        ? Qt.rgba(0.13, 0.15, 0.18, 0.68) : Qt.rgba(1, 1, 1, 0.60))
    readonly property color shellTextLight: themeTokens.value("shellTextLight", isLight
        ? Qt.rgba(0.08, 0.09, 0.11, 0.86) : "#e0e0e5")
    readonly property color shellTextDim: themeTokens.value("shellTextDim", isLight
        ? Qt.rgba(0.13, 0.15, 0.18, 0.54) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color shellIconMain: themeTokens.value("shellIconMain", isLight
        ? Qt.rgba(0.10, 0.11, 0.13, 0.68) : Qt.rgba(1, 1, 1, 0.65))
    readonly property color shellIconActive: themeTokens.value("shellIconActive", isLight
        ? Qt.rgba(0.03, 0.04, 0.05, 0.96) : "#ffffff")
    readonly property color shellIconMuted: themeTokens.value("shellIconMuted", isLight
        ? Qt.rgba(0.13, 0.15, 0.18, 0.32) : Qt.rgba(1, 1, 1, 0.25))
    readonly property color shellIconAccent: themeTokens.value("shellIconAccent", state && state.accentHex ? state.accentHex : "#60aaff")
    readonly property color shellIconWarning: themeTokens.value("shellIconWarning", "#ff375f")
    readonly property color shellSeparator: themeTokens.value("shellSeparator", isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(0, 0, 0, 0.065))
        : Qt.rgba(1, 1, 1, 0.08))

    readonly property real shellRadiusLarge: themeTokens.value("shellRadiusLarge", 14)
    readonly property real shellRadiusMedium: themeTokens.value("shellRadiusMedium", 8)
    readonly property var typography: themeTokens.typography
    readonly property string fontFamily: typography.role("label").family
    readonly property string iconFontFamily: themeTokens.value("iconFontFamily", "JetBrainsMonoNL Nerd Font")
    readonly property font labelFont: typography.font("label")
    readonly property font captionFont: typography.font("caption")
    readonly property int fontSizeBody: typography.role("label").pixelSize
    readonly property int fontSizeSmall: typography.role("caption").pixelSize
    readonly property int fontSizeIcon: themeTokens.value("fontSizeIcon", 16)
    readonly property real spacingLarge: themeTokens.value("spacingLarge", 12)
    readonly property real opacityMuted: 0.80

    // Context menus use one model-driven geometry contract across Dock and
    // Desktop. Keep these values here so the C++ measurement and QML layout
    // receive the same policy inputs.
    readonly property int contextMenuMinimumWidth: 168
    readonly property int contextMenuMaximumWidth: 228
    readonly property int contextMenuNormalRowHeight: Math.max(34, Math.ceil(Math.max(fontSizeBody * typography.role("label").lineHeight,
        fontSizeSmall * typography.role("caption").lineHeight)) + 16)
    readonly property int contextMenuSeparatorHeight: 6
    readonly property int contextMenuRowHorizontalMargin: 8
    readonly property int contextMenuIconSlotWidth: 18
    readonly property int contextMenuRowSpacing: 8
    readonly property int contextMenuCardPadding: 5
    readonly property int contextMenuBorderWidth: 1
    readonly property int contextMenuEdgeMargin: 8
    readonly property real contextMenuRadius: themeTokens.value("contextMenuRadius", 10)
}
