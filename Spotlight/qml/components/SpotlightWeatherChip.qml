import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared

RowLayout {
    id: root
    Shared.ThemeTokens { id: themeTokens }

    property var controller: null
    property color textColor: themeTokens.value("shellTextSecondary", "#66FFFFFF")
    property string fontFamily: themeTokens.typography.role("label").family

    Layout.maximumWidth: 68
    Layout.alignment: Qt.AlignVCenter
    spacing: 1
    visible: controller !== null && controller.weatherEnabled

    Image {
        Layout.preferredWidth: 20
        Layout.preferredHeight: 20
        Layout.alignment: Qt.AlignVCenter
        source: root.controller ? root.controller.weatherIconSource : ""
        sourceSize: Qt.size(40, 40)
        visible: root.controller !== null && root.controller.weatherReady
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        opacity: 0.78
    }

    Text {
        text: "\u25CB"
        visible: root.controller === null || !root.controller.weatherReady
        font.pixelSize: themeTokens.value("fontSizeIconLarge", 18)
        color: root.textColor
        Layout.alignment: Qt.AlignVCenter
    }

    Text {
        Layout.maximumWidth: 46
        text: root.controller && root.controller.weatherReady ? root.controller.weatherTemp + "\u00B0" : "--\u00B0"
        font.family: root.fontFamily
        font.pixelSize: themeTokens.typography.role("label").pixelSize
        font.weight: Font.Medium
        color: root.textColor
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
