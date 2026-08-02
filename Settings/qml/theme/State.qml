pragma Singleton
import QtQuick

QtObject {
    readonly property string configPath: ThemeController.configPath
    readonly property bool loaded: ThemeController.loaded
    property int themeMode: ThemeController.themeMode
    property int shellStyle: ThemeController.shellStyle
    property int iconStyle: ThemeController.iconStyle
    property string iconTheme: ThemeController.iconTheme
    property string accentHex: ThemeController.accentHex
    property int audioOsdStyle: ThemeController.audioOsdStyle
    property int persistedShellStyle: ThemeController.shellStyle

    onThemeModeChanged: if (ThemeController.themeMode !== themeMode) ThemeController.themeMode = themeMode
    onShellStyleChanged: if (ThemeController.shellStyle !== shellStyle) ThemeController.shellStyle = shellStyle
    onIconStyleChanged: if (ThemeController.iconStyle !== iconStyle) ThemeController.iconStyle = iconStyle
    onIconThemeChanged: if (ThemeController.iconTheme !== iconTheme) ThemeController.iconTheme = iconTheme
    onAccentHexChanged: if (ThemeController.accentHex !== accentHex) ThemeController.accentHex = accentHex
    onAudioOsdStyleChanged: if (ThemeController.audioOsdStyle !== audioOsdStyle) ThemeController.audioOsdStyle = audioOsdStyle

    function applyConfig(config) {
        ThemeController.applyConfig(config)
    }

    function save() {
        ThemeController.save()
    }
}
