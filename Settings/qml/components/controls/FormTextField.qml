import QtQuick
import QtQuick.Controls
import ".." as Components

// Single-line text input sized to match SelectButton (36 px) so fields and
// dropdowns line up when they share a row.
TextField {
    id: field

    // Technical values (commands, paths) read better in the monospace face;
    // placeholders and everything else use the regular body face.
    property bool monospace: false

    implicitHeight: Math.max(36, Math.ceil(Components.Theme.typography.role("label").pixelSize
                                          * Components.Theme.typography.role("label").lineHeight) + 16)
    leftPadding: Components.Theme.spacingMedium
    rightPadding: Components.Theme.spacingMedium
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    color: Components.Theme.textPrimary
    selectionColor: Components.Theme.accent
    selectedTextColor: Components.Theme.accentForeground
    placeholderTextColor: Components.Theme.textTertiary
    font.family: field.monospace ? Components.Theme.monoFontFamily
                                 : Components.Theme.typography.role("body").family
    font.pixelSize: Components.Theme.typography.role("body").pixelSize
    opacity: enabled ? 1 : Components.Theme.opacityDisabled

    background: Rectangle {
        radius: Components.Theme.controlRadius
        color: Components.Theme.windowBackground
        border.width: 1
        border.color: field.activeFocus ? Components.Theme.accent : Components.Theme.cardBorder
        Behavior on border.color { ColorAnimation { duration: Components.Theme.animationFast } }
    }
}
