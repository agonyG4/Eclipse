pragma Singleton
import QtQuick

QtObject {
    property bool dark: true
    property color accent: "#8AA7FF"

    readonly property color accentForeground: dark ? "#10131B" : "#FFFFFF"
    readonly property color textPrimary: dark ? "#F4F6FB" : "#17191F"
    readonly property color textSecondary: dark ? "#B8BECA" : "#555B66"
    readonly property color textTertiary: dark ? "#858C99" : "#777D87"

    readonly property color windowBackground: dark ? "#10243F" : "#F3F6FB"
    readonly property color windowGradientStart: dark ? "#1D466F" : "#FFFFFF"
    readonly property color windowGradientEnd: dark ? "#0A172B" : "#E6EBF4"
    readonly property color windowGlow: dark ? "#3F76AD" : "#DCEBFF"
    readonly property color windowWash: dark ? "#18FFFFFF" : "#38FFFFFF"
    readonly property color windowBorder: dark ? "#38FFFFFF" : "#24000000"
    readonly property color windowHighlight: dark ? "#24FFFFFF" : "#A0FFFFFF"
    readonly property color titleBarBackground: dark ? "#183555" : "#F7F9FD"
    readonly property color titleBarGradientEnd: dark ? "#132944" : "#E8EDF6"

    readonly property color sidebarBackground: dark ? "#4B5059" : "#EDF1F8"
    readonly property color sidebarGradientStart: dark ? "#626873" : "#FFFFFF"
    readonly property color sidebarGradientEnd: dark ? "#363B44" : "#E4EAF3"
    readonly property color sidebarWash: dark ? "#12FFFFFF" : "#30FFFFFF"
    readonly property color sidebarBorder: dark ? "#48FFFFFF" : "#22000000"
    readonly property color accountBackground: dark ? "#626872" : "#F9FBFF"
    readonly property color accountGradientEnd: dark ? "#454A53" : "#E9EFF8"
    readonly property color accountBorder: dark ? "#2CFFFFFF" : "#22000000"
    readonly property color panelBackground: dark ? "#0E2038" : "#F5F7FB"
    readonly property color panelGradientStart: dark ? "#183B61" : "#FFFFFF"
    readonly property color panelGradientEnd: dark ? "#0A192E" : "#E9EEF6"
    readonly property color panelBorder: dark ? "#20FFFFFF" : "#1A000000"

    readonly property color surface: dark ? "#20FFFFFF" : "#FFFFFFFF"
    readonly property color surfaceHover: dark ? "#2CFFFFFF" : "#0D000000"
    readonly property color surfacePressed: dark ? "#38FFFFFF" : "#16000000"
    readonly property color surfaceSelected: dark ? "#2D8AA7FF" : "#248AA7FF"
    readonly property color cardBackground: dark ? "#22FFFFFF" : "#DFFFFFFF"
    readonly property color cardBorder: dark ? "#22FFFFFF" : "#18000000"
    readonly property color separator: dark ? "#18FFFFFF" : "#16000000"

    readonly property color focusRing: accent
    readonly property color danger: "#FF6B72"
    readonly property color warning: "#F2B84B"
    readonly property color success: "#54C888"
}
