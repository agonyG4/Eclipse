pragma Singleton
import QtQuick

QtObject {
    readonly property bool dark: Palette.dark
    readonly property int themeMode: dark ? 0 : 1

    readonly property string fontFamily: Tokens.fontFamily
    readonly property string monoFontFamily: Tokens.monoFontFamily
    readonly property int fontSizeHero: Tokens.fontSizeHero
    readonly property int fontSizeHeader: Tokens.fontSizeHeader
    readonly property int fontSizeAvatar: Tokens.fontSizeAvatar
    readonly property int fontSizeSubtitle: Tokens.fontSizeSubtitle
    readonly property int fontSizeTitle: Tokens.fontSizeTitle
    readonly property int fontSizeLarge: Tokens.fontSizeLarge
    readonly property int fontSizeNormal: Tokens.fontSizeNormal
    readonly property int fontSizeSmall: Tokens.fontSizeSmall
    readonly property int fontSizeTiny: Tokens.fontSizeTiny
    readonly property int fontWeightNormal: Tokens.fontWeightNormal
    readonly property int fontWeightMedium: Tokens.fontWeightMedium
    readonly property int fontWeightDemiBold: Tokens.fontWeightDemiBold
    readonly property int fontWeightBold: Tokens.fontWeightBold
    readonly property real radiusSmall: Tokens.radiusSmall
    readonly property real radiusMedium: Tokens.radiusMedium
    readonly property real radiusLarge: Tokens.radiusLarge
    readonly property real radiusWindow: Tokens.radiusWindow
    readonly property real cardRadius: Tokens.cardRadius
    readonly property real controlRadius: Tokens.controlRadius
    readonly property real spacingTiny: Tokens.spacingTiny
    readonly property real spacingMicro: Tokens.spacingMicro
    readonly property real spacingSmall: Tokens.spacingSmall
    readonly property real spacing: Tokens.spacing
    readonly property real spacingMedium: Tokens.spacingMedium
    readonly property real spacingLarge: Tokens.spacingLarge
    readonly property real spacingXLarge: Tokens.spacingXLarge
    readonly property real pageMargin: Tokens.pageMargin
    readonly property int animationMicro: Tokens.animationMicro
    readonly property int animationQuick: Tokens.animationQuick
    readonly property int animationFast: Tokens.animationFast
    readonly property int animationNormal: Tokens.animationNormal
    readonly property int animationSlow: Tokens.animationSlow
    readonly property real opacityDisabled: Tokens.opacityDisabled
    readonly property real opacitySecondary: Tokens.opacitySecondary
    readonly property real opacityMuted: Tokens.opacityMuted

    readonly property color accent: Palette.accent
    readonly property color accentForeground: Palette.accentForeground
    readonly property color textPrimary: Palette.textPrimary
    readonly property color textSecondary: Palette.textSecondary
    readonly property color textTertiary: Palette.textTertiary
    readonly property color windowBackground: Palette.windowBackground
    readonly property color windowWash: Palette.windowWash
    readonly property color windowBorder: Palette.windowBorder
    readonly property color sidebarBackground: Palette.sidebarBackground
    readonly property color sidebarWash: Palette.sidebarWash
    readonly property color sidebarBorder: Palette.sidebarBorder
    readonly property color surface: Palette.surface
    readonly property color surfaceHover: Palette.surfaceHover
    readonly property color surfacePressed: Palette.surfacePressed
    readonly property color surfaceSelected: Palette.surfaceSelected
    readonly property color cardBackground: Palette.cardBackground
    readonly property color cardBorder: Palette.cardBorder
    readonly property color separator: Palette.separator
    readonly property color focusRing: Palette.focusRing
    readonly property color danger: Palette.danger
    readonly property color warning: Palette.warning
    readonly property color success: Palette.success
}
