import QtQuick

// Shared semantic contract. ThemeController publishes the effective roles;
// defaults keep SDK components usable without a running desktop controller.
QtObject {
    id: root
    property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property var tokens: state && state.styleTokens ? state.styleTokens : ({})
    property string surface: ""
    readonly property var surfaces: tokens.typographySurfaces || ({})
    readonly property var roles: surfaces[surface] || tokens.typographyRoles || ({})
    readonly property real textScale: tokens.textScale === undefined ? 1 : tokens.textScale
    readonly property string primaryFamily: state && state.styleTokens
        ? (state.styleTokens.fontFamily || "Inter") : "Inter"

    function role(name) {
        if (roles[name] !== undefined)
            return roles[name]
        const sizes = { display: 26, title: 15, body: 13, label: 13, caption: 11, mono: 12 }
        const family = name === "mono" ? (tokens.monoFontFamily || "monospace")
            : (name === "display" || name === "title") ? (tokens.fontFamilyDisplay || primaryFamily)
            : name === "body" ? (tokens.fontFamilyText || primaryFamily) : primaryFamily
        return { family: family,
            pixelSize: Math.round((sizes[name] || 14) * textScale),
            weight: name === "display" ? Font.Bold : name === "title" ? Font.DemiBold : Font.Normal,
            letterSpacing: 0, lineHeight: 1 }
    }

    function font(name, familyOverride) {
        const style = role(name)
        return Qt.font({ family: familyOverride || style.family, styleName: "", pixelSize: style.pixelSize,
            weight: style.weight, letterSpacing: style.letterSpacing,
            hintingPreference: Font.PreferVerticalHinting, kerning: true })
    }

}
