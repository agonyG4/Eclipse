pragma Singleton
import QtQuick

QtObject {
    property bool dark: true
    property color accent: "#8AA7FF"

    readonly property color accentForeground: dark ? "#10131B" : "#FFFFFF"
    readonly property color textPrimary: dark ? "#F4F6FB" : "#17191F"
    readonly property color textSecondary: dark ? "#B8BECA" : "#555B66"
    readonly property color textTertiary: dark ? "#858C99" : "#777D87"

    readonly property color windowBackground: dark ? "#17191E" : "#F4F5F8"
    readonly property color windowWash: dark ? "#0AFFFFFF" : "#2EFFFFFF"
    readonly property color windowBorder: dark ? "#20FFFFFF" : "#1A000000"

    readonly property color sidebarBackground: dark ? "#15171C" : "#ECEEF3"
    readonly property color sidebarWash: dark ? "#08FFFFFF" : "#28FFFFFF"
    readonly property color sidebarBorder: dark ? "#16FFFFFF" : "#14000000"

    readonly property color surface: dark ? "#20FFFFFF" : "#FFFFFFFF"
    readonly property color surfaceHover: dark ? "#2CFFFFFF" : "#0D000000"
    readonly property color surfacePressed: dark ? "#38FFFFFF" : "#16000000"
    readonly property color surfaceSelected: dark ? "#2D8AA7FF" : "#248AA7FF"
    readonly property color cardBackground: dark ? "#18FFFFFF" : "#CFFFFFFF"
    readonly property color cardBorder: dark ? "#18FFFFFF" : "#14000000"
    readonly property color separator: dark ? "#18FFFFFF" : "#16000000"

    readonly property color focusRing: accent
    readonly property color danger: "#FF6B72"
    readonly property color warning: "#F2B84B"
    readonly property color success: "#54C888"
}
