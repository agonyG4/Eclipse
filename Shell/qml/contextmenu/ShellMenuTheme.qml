import QtQuick

QtObject {
    readonly property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property bool isLight: state && state.themeMode === 1
    readonly property bool isDefault: state && state.shellStyle === 1
    readonly property bool isFrosted: state && state.shellStyle === 2

    readonly property color background: isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06))
    readonly property color border: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14))
    readonly property color shellHover: isLight
        ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08)
    readonly property color shellPressed: isLight
        ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12)
    readonly property color shellTextActive: isLight
        ? Qt.rgba(0.04, 0.05, 0.06, 0.96) : "#ffffff"
    readonly property color shellTextSecondary: isLight
        ? Qt.rgba(0.13, 0.15, 0.18, 0.68) : Qt.rgba(1, 1, 1, 0.60)
    readonly property color shellIconMain: isLight
        ? Qt.rgba(0.10, 0.11, 0.13, 0.68) : Qt.rgba(1, 1, 1, 0.65)
    readonly property color shellIconWarning: "#ff375f"
    readonly property color shellSeparator: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(0, 0, 0, 0.065))
        : Qt.rgba(1, 1, 1, 0.08)

    readonly property real shellRadiusLarge: 14
    readonly property real shellRadiusMedium: 8
    readonly property string fontFamily: "Inter Variable"
    readonly property string iconFontFamily: "JetBrainsMonoNL Nerd Font"
    readonly property int fontSizeBody: 12
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeIcon: 16
    readonly property real spacingLarge: 12
    readonly property real opacityMuted: 0.80

    // Context menus use one model-driven geometry contract across Dock and
    // Desktop. Keep these values here so the C++ measurement and QML layout
    // receive the same policy inputs.
    readonly property int contextMenuMinimumWidth: 200
    readonly property int contextMenuMaximumWidth: 260
    readonly property int contextMenuNormalRowHeight: 36
    readonly property int contextMenuSeparatorHeight: 10
    readonly property int contextMenuRowHorizontalMargin: 10
    readonly property int contextMenuIconSlotWidth: 20
    readonly property int contextMenuRowSpacing: spacingLarge
    readonly property int contextMenuCardPadding: 10
    readonly property int contextMenuBorderWidth: 1
    readonly property int contextMenuEdgeMargin: 8
}
