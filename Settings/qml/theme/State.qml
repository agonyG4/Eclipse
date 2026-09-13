pragma Singleton
import QtQuick

QtObject {
    readonly property string configPath: ThemeController.configPath
    readonly property bool loaded: ThemeController.loaded
    property int themeMode: ThemeController.themeMode
    property string themePreference: ThemeController.themePreference
    property int shellStyle: ThemeController.shellStyle
    property int iconStyle: ThemeController.iconStyle
    property string iconTheme: ThemeController.iconTheme
    property string iconAppearance: ThemeController.iconAppearance
    property string accentHex: ThemeController.accentHex
    property int audioOsdStyle: ThemeController.audioOsdStyle
    property int persistedShellStyle: ThemeController.shellStyle

    onThemeModeChanged: if (ThemeController.themeMode !== themeMode) ThemeController.themeMode = themeMode
    onThemePreferenceChanged: if (ThemeController.themePreference !== themePreference) ThemeController.themePreference = themePreference
    onShellStyleChanged: if (ThemeController.shellStyle !== shellStyle) ThemeController.shellStyle = shellStyle
    onIconStyleChanged: if (ThemeController.iconStyle !== iconStyle) ThemeController.iconStyle = iconStyle
    onIconThemeChanged: if (ThemeController.iconTheme !== iconTheme) ThemeController.iconTheme = iconTheme
    onIconAppearanceChanged: if (ThemeController.iconAppearance !== iconAppearance) ThemeController.iconAppearance = iconAppearance
    onAccentHexChanged: if (ThemeController.accentHex !== accentHex) ThemeController.accentHex = accentHex
    onAudioOsdStyleChanged: if (ThemeController.audioOsdStyle !== audioOsdStyle) ThemeController.audioOsdStyle = audioOsdStyle

    function applyConfig(config) {
        ThemeController.applyConfig(config)
    }

    function setThemePreference(value) {
        ThemeController.themePreference = value
    }

    function setShellStyle(value) {
        ThemeController.shellStyle = value
    }

    function setAccentHex(value) {
        ThemeController.accentHex = value
    }

    function setIconAppearance(value) {
        ThemeController.iconAppearance = value
    }

    function save() {
        ThemeController.save()
    }
}
