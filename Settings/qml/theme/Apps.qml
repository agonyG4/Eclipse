pragma Singleton
import QtQuick
import Astrea.Settings 1.0 as Borealis
import Astrea.Shared as Shared

QtObject {
    readonly property Shared.ThemeTokens themeTokens: Shared.ThemeTokens { state: Borealis.State }
    readonly property color accent: themeTokens.value("accent", Borealis.State.accentHex)
    readonly property color accentForeground: themeTokens.value("accentForeground",
        (accent.r * 0.299 + accent.g * 0.587 + accent.b * 0.114) > 0.62 ? "#111111" : "#ffffff")
    readonly property color textPrimary: themeTokens.value("textPrimary", Borealis.State.themeMode === 1 ? Qt.rgba(0.05, 0.06, 0.07, 0.94) : Qt.rgba(0.96, 0.96, 0.98, 0.94))
    readonly property color textSecondary: themeTokens.value("textSecondary", Borealis.State.themeMode === 1 ? Qt.rgba(0.13, 0.15, 0.18, 0.76) : Qt.rgba(0.92, 0.94, 0.96, 0.72))
    readonly property color textTertiary: themeTokens.value("textTertiary", Borealis.State.themeMode === 1 ? Qt.rgba(0.13, 0.15, 0.18, 0.50) : Qt.rgba(0.92, 0.94, 0.96, 0.48))
    function cardBgFallback() {
        if (Borealis.State.shellStyle === 0)
            return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.035)
        if (Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0.98, 0.99, 1, 0.36) : Qt.rgba(1, 1, 1, 0.035)
        return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.72) : Qt.rgba(1, 1, 1, 0.05)
    }
    readonly property color cardBg: themeTokens.value("cardBg", cardBgFallback())
    function cardBorderFallback() {
        if (Borealis.State.shellStyle === 0)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(1, 1, 1, 0.06)
        if (Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.06)
        return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.08)
    }
    readonly property color cardBorder: themeTokens.value("cardBorder", cardBorderFallback())
    function popupBgFallback() {
        if (Borealis.State.shellStyle === 0 || Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0.98, 0.98, 0.99, 0.92) : Qt.rgba(0.11, 0.11, 0.12, 0.92)
        return Borealis.State.themeMode === 1 ? Qt.rgba(0.98, 0.98, 0.99, 1) : Qt.rgba(0.11, 0.11, 0.12, 1)
    }
    readonly property color popupBg: themeTokens.value("popupBg", popupBgFallback())
    function windowBackgroundFallback() {
        if (Borealis.State.shellStyle === 0)
            return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.06)
        if (Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0.96, 0.985, 1, 0.24) : Qt.rgba(0, 0, 0, 0.06)
        return Borealis.State.themeMode === 1 ? Qt.rgba(0.965, 0.968, 0.98, 1.0) : Qt.rgba(0.11, 0.11, 0.12, 1.0)
    }
    readonly property color windowBackground: themeTokens.value("windowBackground", windowBackgroundFallback())
    function windowBorderFallback() {
        if (Borealis.State.shellStyle === 0)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(1, 1, 1, 0.06)
        if (Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.06)
        return Borealis.State.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.08)
    }
    readonly property color windowBorder: themeTokens.value("windowBorder", windowBorderFallback())
    function windowWashFallback() {
        if (Borealis.State.shellStyle === 0)
            return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
        if (Borealis.State.shellStyle === 2)
            return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.10) : "transparent"
        return Borealis.State.themeMode === 1 ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(1, 1, 1, 0.02)
    }
    readonly property color windowWash: themeTokens.value("windowWash", windowWashFallback())

    readonly property color errorColor: themeTokens.value("errorColor", "#ff453a")
    readonly property color warningColor: themeTokens.value("warningColor", "#ff9f0a")
    readonly property color successColor: themeTokens.value("successColor", "#30d158")
}
