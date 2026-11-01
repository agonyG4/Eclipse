import QtQuick

Text {
    id: root
    property string typographyRole: "body"
    property Typography typography: Typography {}
    readonly property var resolvedStyle: typography.role(typographyRole)
    font.family: resolvedStyle.family
    font.pixelSize: resolvedStyle.pixelSize
    font.weight: resolvedStyle.weight
    font.letterSpacing: resolvedStyle.letterSpacing
    font.kerning: true
    font.hintingPreference: Font.PreferVerticalHinting
    lineHeightMode: Text.ProportionalHeight
    lineHeight: resolvedStyle.lineHeight
    renderType: Text.NativeRendering
}
