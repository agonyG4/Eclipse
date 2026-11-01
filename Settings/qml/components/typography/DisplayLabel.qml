import QtQuick
import QtQuick.Controls
import ".." as Components

Label {
    property color textColor: Components.Theme.textPrimary

    property string typographyRole: "display"
    readonly property var typographyStyle: Components.Theme.typography.role(typographyRole)
    font.pixelSize: typographyStyle.pixelSize
    font.weight: typographyStyle.weight
    font.letterSpacing: typographyStyle.letterSpacing
    lineHeight: typographyStyle.lineHeight
    lineHeightMode: Text.ProportionalHeight
    font.family: typographyStyle.family
    font.hintingPreference: Font.PreferVerticalHinting
    font.kerning: true
    renderType: Text.NativeRendering
    antialiasing: true
    color: textColor

    Behavior on color {
        ColorAnimation { duration: Components.Theme.animationFast; easing.type: Easing.OutCubic }
    }
}
