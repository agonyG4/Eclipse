import QtQuick
import Astrea.Shared as Shared

QtObject {
    readonly property Shared.ShellMaterialTheme material: Shared.ShellMaterialTheme { typographySurface: "menu" }
    readonly property Shared.ThemeTokens themeTokens: material.tokens
    readonly property var state: material.state
    readonly property bool isLight: material.isLight
    readonly property bool isDefault: material.isDefault
    readonly property bool isFrosted: material.isFrosted

    readonly property color background: material.background
    readonly property color border: material.border
    readonly property color shellHover: material.hover
    readonly property color shellPressed: material.pressed
    readonly property color shellTextActive: material.textActive
    readonly property color shellTextSecondary: material.textSecondary
    readonly property color shellTextLight: material.textLight
    readonly property color shellTextDim: material.textDim
    readonly property color shellIconMain: material.iconMain
    readonly property color shellIconActive: material.iconActive
    readonly property color shellIconMuted: material.iconMuted
    readonly property color shellIconAccent: material.iconAccent
    readonly property color shellIconWarning: material.iconWarning
    readonly property color shellSeparator: material.separator

    readonly property real shellRadiusLarge: material.radiusLarge
    readonly property real shellRadiusMedium: material.radiusMedium
    readonly property var typography: material.typography
    readonly property string fontFamily: material.fontFamily
    readonly property string iconFontFamily: material.tokens.value("iconFontFamily", "JetBrainsMonoNL Nerd Font")
    readonly property font labelFont: typography.font("label")
    readonly property font captionFont: typography.font("caption")
    readonly property int fontSizeBody: typography.role("label").pixelSize
    readonly property int fontSizeSmall: typography.role("caption").pixelSize
    readonly property int fontSizeIcon: material.tokens.value("fontSizeIcon", 16)
    readonly property real spacingLarge: material.tokens.value("spacingLarge", 12)
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
    readonly property real contextMenuRadius: material.tokens.value("contextMenuRadius", 10)
}
