import QtQuick
import QtQuick.Layouts
import Astrea.Shared as Shared

Item {
    id: root
    objectName: "themeExampleAltTabPanel"
    readonly property int themeApiVersion: 1
    property var controller
    property var windowModel
    property real availableWidth: 900
    readonly property Item backdropItem: background
    Shared.ThemeTokens { id: tokens }

    width: Math.min(Math.max(220, windows.implicitWidth + 36), availableWidth - 120)
    height: 116

    FocusScope {
        anchors.fill: parent
        focus: root.controller && root.controller.open
        Keys.onEscapePressed: if (root.controller) root.controller.cancel()
        Keys.onReturnPressed: if (root.controller) root.controller.commit()
        Keys.onRightPressed: if (root.controller) root.controller.step(1)
        Keys.onLeftPressed: if (root.controller) root.controller.step(-1)

        Rectangle {
            id: background
            anchors.fill: parent
            radius: tokens.value("altTabRadius", 24)
            color: tokens.value("shellBackground", "#25212b")
            border.color: tokens.value("shellBorder", "#55505c")

            RowLayout {
                id: windows
                anchors.centerIn: parent
                spacing: 12
                Repeater {
                    model: root.windowModel
                    delegate: Rectangle {
                        required property string displayName
                        required property bool selected
                        implicitWidth: 110
                        implicitHeight: 72
                        radius: 10
                        color: selected ? tokens.value("accent", "#6574e8") : tokens.value("shellSurface", "#38333f")
                        Text { anchors.centerIn: parent; text: displayName; color: "white"; elide: Text.ElideRight; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter }
                    }
                }
            }
        }
    }
}
