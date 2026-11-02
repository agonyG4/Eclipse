import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared

RowLayout {
    id: root
    Shared.ThemeTokens { id: themeTokens; typographySurface: "spotlight" }

    property var controller: null
    property color textColor: themeTokens.value("shellTextSecondary", "#66FFFFFF")
    property string fontFamily: themeTokens.typography.role("label").family

    Layout.maximumWidth: 68
    Layout.alignment: Qt.AlignVCenter
    spacing: 1
    // Without a reading there is nothing worth showing; no placeholder glyphs.
    visible: controller !== null && controller.weatherEnabled && controller.weatherReady

    Image {
        Layout.preferredWidth: 20
        Layout.preferredHeight: 20
        Layout.alignment: Qt.AlignVCenter
        source: root.controller ? root.controller.weatherIconSource : ""
        sourceSize: Qt.size(40, 40)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        opacity: 0.78
    }

    Text {
        Layout.maximumWidth: 46
        text: root.controller ? root.controller.weatherTemp + "\u00B0" : ""
        font: themeTokens.typography.font("label", root.fontFamily)
        renderType: Text.NativeRendering
        color: root.textColor
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
