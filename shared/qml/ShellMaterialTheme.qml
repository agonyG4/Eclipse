import QtQuick

QtObject {
    id: root

    property string typographySurface: ""
    readonly property ThemeTokens tokens: ThemeTokens { typographySurface: root.typographySurface }
    readonly property var typography: tokens.typography
    readonly property var state: tokens.state
    readonly property bool isLight: !!state && state.themeMode === 1
    readonly property bool isTransparent: !state || state.shellStyle === 0
    readonly property bool isDefault: !!state && state.shellStyle === 1
    readonly property bool isFrosted: !!state && state.shellStyle === 2

    // Keep these values identical to the existing ShellBarTheme Borealis
    // policy. This object is the shared owner for Shell material only.
    readonly property color background: tokens.value("shellBackground", isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06)))
    readonly property color surface: tokens.value("shellSurface", isLight
        ? (isDefault ? Qt.rgba(1, 1, 1, 0.86)
            : isFrosted ? Qt.rgba(0.98, 0.99, 1, 0.38)
            : Qt.rgba(1, 1, 1, 0.22))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.06)))
    readonly property color surfaceElevated: tokens.value("shellSurfaceElevated", isLight
        ? (isDefault ? Qt.rgba(0.98, 0.98, 0.99, 1)
            : Qt.rgba(0.98, 0.98, 0.99, 0.92))
        : (isDefault ? Qt.rgba(0.11, 0.11, 0.12, 1)
            : Qt.rgba(0.11, 0.11, 0.12, 0.92)))
    // Tooltip materials are intentionally separate from the existing Shell
    // surfaces: Frosted keeps a visible backdrop contribution, while
    // Transparent follows the lighter existing Shell surface tint.
    readonly property color tooltipSurface: tokens.value("tooltipSurface", isDefault ? surfaceElevated
        : isFrosted
            ? (isLight ? Qt.rgba(0.985, 0.99, 1, 0.44)
                       : Qt.rgba(0.055, 0.065, 0.08, 0.46))
            : surface)
    readonly property color border: tokens.value("shellBorder", isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14)))
    readonly property color tooltipBorder: tokens.value("tooltipBorder", isDefault ? border
        : Qt.rgba(border.r, border.g, border.b, border.a * 0.72))
    readonly property color borderHover: tokens.value("shellBorderHover", isLight
        ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(1, 1, 1, 0.28))
    readonly property color textActive: tokens.value("shellTextActive", isLight
        ? Qt.rgba(0.04, 0.05, 0.06, 0.96) : "#ffffff")
    readonly property color shadow: tokens.value("shellShadow", isLight
        ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(0, 0, 0, 0.48))
    readonly property color tooltipShadow: tokens.value("tooltipShadow",
        Qt.rgba(shadow.r, shadow.g, shadow.b, shadow.a * 0.55))
    readonly property color hover: tokens.value("shellHover", isLight
        ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08))
    readonly property color pressed: tokens.value("shellPressed", isLight
        ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12))
    readonly property color active: tokens.value("shellActive", isLight
        ? Qt.rgba(0, 122, 255, 0.14) : Qt.rgba(1, 1, 1, 0.15))
    readonly property color separator: tokens.value("shellSeparator", isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(0, 0, 0, 0.065))
        : Qt.rgba(1, 1, 1, 0.08))

    readonly property real radiusLarge: tokens.value("shellRadiusLarge", 14)
    readonly property real radiusMedium: tokens.value("shellRadiusMedium", 8)
    readonly property string fontFamily: typography.role("label").family
    readonly property int fontSizeCaption: typography.role("caption").pixelSize
    readonly property int tooltipFontSize: typography.role("label").pixelSize
}
