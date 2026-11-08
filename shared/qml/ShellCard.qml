import QtQuick

// Card surface for shell popups, menus and launcher panels. The fill and the
// hairline border come from a ShellMaterialTheme so every card follows the
// shell palette (light/dark and shell style). Callers choose the corner radius
// and may pass their own material, for example one bound to a typography
// surface.
Rectangle {
    id: root

    ShellMaterialTheme { id: ownMaterial }

    property ShellMaterialTheme material: ownMaterial
    property real cardRadius: material.radiusLarge

    radius: cardRadius
    color: material.background
    border.color: material.border
    border.width: 1
}
