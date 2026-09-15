import QtQuick

QtObject {
    id: root

    readonly property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property bool isLight: !!state && state.themeMode === 1
    readonly property bool isTransparent: !state || state.shellStyle === 0
    readonly property bool isDefault: !!state && state.shellStyle === 1
    readonly property bool isFrosted: !!state && state.shellStyle === 2

    // Keep these values identical to the existing ShellBarTheme Borealis
    // policy. This object is the shared owner for Shell material only.
    readonly property color background: isLight
        ? (isDefault ? Qt.rgba(0.985, 0.987, 0.994, 0.92)
            : isFrosted ? Qt.rgba(0.96, 0.985, 1, 0.30)
            : Qt.rgba(1, 1, 1, 0.16))
        : (isDefault ? Qt.rgba(0.10, 0.10, 0.11, 0.96)
            : Qt.rgba(0, 0, 0, 0.06))
    readonly property color surface: isLight
        ? (isDefault ? Qt.rgba(1, 1, 1, 0.86)
            : isFrosted ? Qt.rgba(0.98, 0.99, 1, 0.38)
            : Qt.rgba(1, 1, 1, 0.22))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.06))
    readonly property color surfaceElevated: isLight
        ? (isDefault ? Qt.rgba(0.98, 0.98, 0.99, 1)
            : Qt.rgba(0.98, 0.98, 0.99, 0.92))
        : (isDefault ? Qt.rgba(0.11, 0.11, 0.12, 1)
            : Qt.rgba(0.11, 0.11, 0.12, 0.92))
    readonly property color border: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.12)
            : isFrosted ? Qt.rgba(0, 0, 0, 0.10)
            : Qt.rgba(0, 0, 0, 0.08))
        : (isDefault ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.14))
    readonly property color borderHover: isLight
        ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(1, 1, 1, 0.28)
    readonly property color hover: isLight
        ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08)
    readonly property color pressed: isLight
        ? Qt.rgba(0, 0, 0, 0.085) : Qt.rgba(1, 1, 1, 0.12)
    readonly property color active: isLight
        ? Qt.rgba(0, 122, 255, 0.14) : Qt.rgba(1, 1, 1, 0.15)
    readonly property color separator: isLight
        ? (isDefault ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(0, 0, 0, 0.065))
        : Qt.rgba(1, 1, 1, 0.08)

    readonly property real radiusLarge: 14
    readonly property real radiusMedium: 8
}
