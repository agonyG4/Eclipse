import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "."

Item {
    id: root

    readonly property bool open: AltTabController.open

    width: Math.min(Math.max(220, appRow.implicitWidth + 34), (parent ? parent.width : 1920) - 120)
    height: 116

    scale: open ? 1.0 : 0.96
    opacity: open ? 1.0 : 0.0

    Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 90 } }

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

        Rectangle {
            id: panel
            anchors.fill: parent
            radius: 26
            color: "#80343434"
            border.color: "#33FFFFFF"
            border.width: 1
            clip: true

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "#10000000"
                visible: true
            }

            Row {
                id: appRow
                anchors.centerIn: parent
                spacing: 12

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
