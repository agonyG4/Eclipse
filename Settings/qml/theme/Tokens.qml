pragma Singleton
import QtQuick
import Astrea.Settings 1.0 as Borealis
import Astrea.Shared as Shared

QtObject {
    readonly property Shared.ThemeTokens themeTokens: Shared.ThemeTokens { state: Borealis.State }
    readonly property string fontFamily: themeTokens.value("fontFamily", "Inter")
    readonly property string monoFontFamily: themeTokens.value("monoFontFamily", "JetBrains Mono")
    readonly property string fontFamilyDisplay: themeTokens.value("fontFamilyDisplay", fontFamily)
    readonly property string fontFamilyText: themeTokens.value("fontFamilyText", fontFamily)
    readonly property string iconFontFamily: themeTokens.value("iconFontFamily", monoFontFamily)

    readonly property int fontSizeHero: themeTokens.value("fontSizeHero", 20)
    readonly property int fontSizeHeader: themeTokens.value("fontSizeHeader", 24)
    readonly property int fontSizeIconLarge: themeTokens.value("fontSizeIconLarge", 22)
    readonly property int fontSizeAvatar: themeTokens.value("fontSizeAvatar", 20)
    readonly property int fontSizeSubtitle: themeTokens.value("fontSizeSubtitle", 16)
    readonly property int fontSizeTitle: themeTokens.value("fontSizeTitle", 15)
    readonly property int fontSizeLarge: themeTokens.value("fontSizeLarge", 13)
    readonly property int fontSizeNormal: themeTokens.value("fontSizeNormal", 12)
    readonly property int fontSizeSmall: themeTokens.value("fontSizeSmall", 11)
    readonly property int fontSizeTiny: themeTokens.value("fontSizeTiny", 10)
    readonly property int fontSizeMicro: themeTokens.value("fontSizeMicro", 9)
    readonly property int fontSizeBody: themeTokens.value("fontSizeBody", fontSizeNormal)
    readonly property int fontSizeCaption: themeTokens.value("fontSizeCaption", fontSizeSmall)

    readonly property int fontWeightLight: Font.Light
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightDemiBold: Font.Medium
    readonly property int fontWeightBold: Font.Bold

    readonly property real trackingHeader: 0

    readonly property real radiusSmall: themeTokens.value("radiusSmall", 8)
    readonly property real radiusMedium: themeTokens.value("radiusMedium", 10)
    readonly property real radiusLarge: themeTokens.value("radiusLarge", 12)
    readonly property real cornerRadiusSmall: themeTokens.value("cornerRadiusSmall", radiusSmall)
    readonly property real cornerRadius: themeTokens.value("cornerRadius", radiusMedium)
    readonly property real cornerRadiusLarge: themeTokens.value("cornerRadiusLarge", radiusLarge)
    readonly property real cardRadius: themeTokens.value("cardRadius", radiusLarge)
    readonly property real controlRadius: themeTokens.value("controlRadius", radiusMedium)

    readonly property real spacingTiny: themeTokens.value("spacingTiny", 2)
    readonly property real spacingMicro: themeTokens.value("spacingMicro", 4)
    readonly property real spacingSmall: themeTokens.value("spacingSmall", 6)
    readonly property real spacing: themeTokens.value("spacing", 8)
    readonly property real spacingMedium: themeTokens.value("spacingMedium", 12)
    readonly property real spacingLarge: themeTokens.value("spacingLarge", 16)
    readonly property real spacingXLarge: themeTokens.value("spacingXLarge", 18)
    readonly property real pageMargin: themeTokens.value("pageMargin", 28)

    readonly property int animationMicro: themeTokens.value("animationMicro", 100)
    readonly property int animationQuick: themeTokens.value("animationQuick", 120)
    readonly property int animationFast: themeTokens.value("animationFast", 150)
    readonly property int animationNormal: themeTokens.value("animationNormal", 200)
    readonly property int animationSlow: themeTokens.value("animationSlow", 250)
    readonly property int animationPopover: themeTokens.value("animationPopover", 300)

    readonly property real opacityDisabled: 0.3
    readonly property real opacitySecondary: 0.5
    readonly property real opacityMuted: 0.6
}
