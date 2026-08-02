import QtQuick

Item {
    id: root

    readonly property int contentWidth: appRow.implicitWidth + DockController.panelPadding * 2
    readonly property int contentHeight: DockController.iconSize + 20

    width: contentWidth
    height: contentHeight

    Behavior on width {
        NumberAnimation { duration: 135; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        radius: 23
        color: "#80343434"
        border.color: "#33FFFFFF"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#10000000"
        }

        Row {
            id: appRow
            anchors.centerIn: parent
            spacing: DockController.itemSpacing

            Repeater {
                model: DockController.appModel

                delegate: DockAppDelegate {
                    iconSize: DockController.iconSize
                    onActivated: function(row) { DockController.launch(row) }
                }
            }
        }
    }
}
