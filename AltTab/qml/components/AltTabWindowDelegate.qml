import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "."

Item {
    id: root
    objectName: "altTabWindowDelegate"

    property int windowIndex: 0
    property bool windowSelected: false
    property bool windowActive: false
    property bool windowHovered: false
    property string windowIconUrl: ""
    property string windowIconName: ""
    property string windowIconPath: ""
    property bool windowIconPending: false
    property bool windowShowFallbackText: true
    property string windowDisplayName: ""

    width: 92
    height: 92

    Rectangle {
        id: tileBg
        anchors.fill: parent
        radius: 22
        color: "transparent"

        Rectangle {
            id: selectionHighlight
            anchors.fill: parent
            radius: parent.radius
            color: windowSelected
                ? Qt.rgba(0.0, 0.48, 1.0, 0.25)
                : "transparent"
            border.color: windowSelected
                ? Qt.rgba(0.0, 0.48, 1.0, 0.40)
                : "transparent"
            border.width: 1
            scale: windowSelected ? 1.06 : 1.0

            Behavior on color { ColorAnimation { duration: 90 } }
            Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        }

        AstreaAppIcon {
            anchors.centerIn: parent
            width: windowSelected ? 84 : 72
            height: width
            iconSource: windowIconUrl
            iconName: windowIconName
            iconPath: windowIconPath
            iconPending: windowIconPending
            showFallbackText: windowShowFallbackText
            appName: windowDisplayName
            iconSize: width
            maximumPresentationLogicalSize: 84
            iconRadius: windowSelected ? 18 : 15
            fallbackRadius: iconRadius
            fallbackColor: "#22FFFFFF"
            fallbackFontSize: 27

            Behavior on width { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on height { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on iconRadius { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                root.windowHovered = true
                AltTabController.preview(windowIndex)
            }
            onExited: root.windowHovered = false
            onClicked: AltTabController.commitIndex(windowIndex)
        }
    }
}
