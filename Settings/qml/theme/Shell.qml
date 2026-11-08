pragma Singleton
import QtQuick
import Astrea.Settings 1.0 as Borealis
import Astrea.Shared as Shared

// Settings view of the shared shell palette. Colors come from
// Shared.ShellMaterialTheme; only Settings-specific values live here.
QtObject {
    readonly property Shared.ShellMaterialTheme material: Shared.ShellMaterialTheme { state: Borealis.State }
    readonly property Shared.ThemeTokens themeTokens: material.tokens
    readonly property bool isLight: material.isLight
    readonly property bool isTransparent: material.isTransparent
    readonly property bool isDefault: material.isDefault
    readonly property bool isFrosted: material.isFrosted

    readonly property color background: material.background
    readonly property color surface: material.surface
    readonly property color border: material.border
    // Settings keeps the dark separator in both modes and a fixed accent
    // fallback; the shell material differs on both.
    readonly property color separator: themeTokens.value("shellSeparator", Qt.rgba(1, 1, 1, 0.08))
    readonly property color hover: material.hover
    readonly property color pressed: material.pressed
    readonly property color active: material.active
    readonly property color barBorderHover: material.borderHover
    readonly property color islandBackground: "#000000"

    readonly property color textMain: material.textMain
    readonly property color textSecondary: material.textSecondary
    readonly property color textLight: material.textLight
    readonly property color textDim: material.textDim
    readonly property color textActive: material.textActive

    readonly property color iconMain: material.iconMain
    readonly property color iconActive: material.iconActive
    readonly property color iconMuted: material.iconMuted
    readonly property color iconWarning: material.iconWarning
    readonly property color iconAccent: themeTokens.value("shellIconAccent", "#60aaff")

    readonly property real radiusLarge: material.radiusLarge
    readonly property real radiusMedium: material.radiusMedium
    readonly property real radiusSmall: material.radiusSmall
    readonly property real controlRadius: material.controlRadius
    readonly property real tileRadius: material.tileRadius
    readonly property real pillRadius: material.pillRadius

    readonly property int workspaceDotSize: material.workspaceDotSize
    readonly property int workspaceActiveWidth: material.workspaceActiveWidth
}
