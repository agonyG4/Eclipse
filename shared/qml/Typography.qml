import QtQuick

// Shared semantic contract. ThemeController publishes the effective roles;
// defaults keep SDK components usable without a running desktop controller.
QtObject {
    id: root
    property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property var tokens: state && state.styleTokens ? state.styleTokens : ({})
    readonly property var roles: tokens.typographyRoles || ({})
    readonly property real textScale: tokens.textScale === undefined ? 1 : tokens.textScale
    readonly property string primaryFamily: state && state.styleTokens
        ? (state.styleTokens.fontFamily || "Inter") : "Inter"

    function role(name) {
        if (roles[name] !== undefined)
            return roles[name]
        const sizes = { display: 24, title: 18, body: 14, label: 13, caption: 11, mono: 13 }
        return { family: name === "mono" ? "monospace" : primaryFamily,
            pixelSize: Math.round((sizes[name] || 14) * textScale),
            weight: name === "display" ? Font.DemiBold : (name === "title" || name === "label") ? Font.Medium : Font.Normal,
            letterSpacing: 0, lineHeight: name === "body" ? 1.4 : 1.2 }
    }
}
