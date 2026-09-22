pragma Singleton
import QtQuick

QtObject {
    readonly property string configPath: ThemeController.configPath
    readonly property bool loaded: ThemeController.loaded
    readonly property int themeMode: ThemeController.themeMode
    readonly property string themePreference: ThemeController.themePreference
    property int shellStyle: ThemeController.shellStyle
    property int iconStyle: ThemeController.iconStyle
    property string iconTheme: ThemeController.iconTheme
    readonly property string iconAppearance: ThemeController.iconAppearance
    readonly property string accentHex: ThemeController.accentHex
    property int audioOsdStyle: ThemeController.audioOsdStyle
    property int persistedShellStyle: ThemeController.shellStyle

    onShellStyleChanged: if (ThemeController.shellStyle !== shellStyle) ThemeController.shellStyle = shellStyle
    onIconStyleChanged: if (ThemeController.iconStyle !== iconStyle) ThemeController.iconStyle = iconStyle
    onIconThemeChanged: if (ThemeController.iconTheme !== iconTheme) ThemeController.iconTheme = iconTheme
    onAudioOsdStyleChanged: if (ThemeController.audioOsdStyle !== audioOsdStyle) ThemeController.audioOsdStyle = audioOsdStyle

    function setShellStyle(value) {
        ThemeController.shellStyle = value
    }

    function save() {
        ThemeController.save()
    }
}
