import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Astrea.Shared as Shared
import "."

Item {
    id: root

    property real availableWidth: Screen.width > 0 ? Screen.width : 1920
    readonly property bool open: AltTabController.open
    readonly property Item backdropItem: panel
    Shared.ShellMaterialTheme { id: shellMaterial }
    readonly property Shared.ThemeTokens themeTokens: shellMaterial.tokens

    width: Math.min(Math.max(220, appRow.implicitWidth + 34), availableWidth - 120)
    height: 116
    implicitWidth: width
    implicitHeight: height

    scale: open ? 1.0 : 0.96
    opacity: open ? 1.0 : 0.0

    Behavior on scale { NumberAnimation { duration: themeTokens.value("animationSubtle", 130); easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: themeTokens.value("animationInstant", 90) } }

    onOpenChanged: {
        if (open) {
            focusScope.forceActiveFocus()
        }
    }

    FocusScope {
        id: focusScope
        anchors.fill: parent
        focus: open
        activeFocusOnTab: true

        Keys.onEscapePressed: AltTabController.cancel()
        Keys.onReturnPressed: AltTabController.commit()
        Keys.onEnterPressed: AltTabController.commit()
        Keys.onRightPressed: AltTabController.step(1)
        Keys.onLeftPressed: AltTabController.step(-1)
        Keys.onReleased: function(event) {
            if (event.key === Qt.Key_Alt || event.key === Qt.Key_AltGr) {
                event.accepted = true
                AltTabController.commit()
            }
        }

        Shared.ShellCard {
            id: panel
            objectName: "altTabPanelBackground"
            anchors.fill: parent
            material: shellMaterial
            cardRadius: themeTokens.value("altTabRadius", 26)
            clip: true

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: shellMaterial.surface
                visible: true
            }

            Row {
                id: appRow
                anchors.centerIn: parent
                spacing: themeTokens.value("spacingMedium", 12)

                Repeater {
                    model: AltTabWindowModel

                    delegate: AltTabWindowDelegate {
                        required property int index
                        required property string address
                        required property string iconUrl
                        required property string iconName
                        required property string iconPath
                        required property bool iconPending
                        required property bool showFallbackText
                        required property string displayName
                        required property bool selected
                        required property bool active

                        windowIndex: index
                        windowSelected: selected
                        windowActive: active
                        windowIconUrl: iconUrl
                        windowIconName: iconName
                        windowIconPath: iconPath
                        windowIconPending: iconPending
                        windowShowFallbackText: showFallbackText
                        windowDisplayName: displayName
                    }
                }
            }
        }
    }
}
