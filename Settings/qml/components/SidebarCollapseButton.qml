import QtQuick

Item {
    id: root

    property bool collapsed: false
    signal clicked()

    implicitWidth: 30
    implicitHeight: 30
    activeFocusOnTab: true

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: pressArea.pressed
            ? Theme.surfacePressed
            : hoverHandler.hovered ? Theme.surfaceHover : "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.focusRing

        Behavior on color {
            ColorAnimation { duration: Theme.animationQuick }
        }
    }

    Item {
        width: 15
        height: 13
        anchors.centerIn: parent

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "transparent"
            border.width: 1
            border.color: hoverHandler.hovered ? Theme.textPrimary : Theme.textSecondary
        }

        Rectangle {
            width: 1
            height: parent.height - 4
            x: root.collapsed ? 9 : 5
            anchors.verticalCenter: parent.verticalCenter
            color: hoverHandler.hovered ? Theme.textPrimary : Theme.textSecondary

            Behavior on x {
                NumberAnimation { duration: Theme.animationQuick }
            }
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Keys.onSpacePressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Accessible.role: Accessible.Button
    Accessible.name: root.collapsed ? qsTr("Expand sidebar") : qsTr("Collapse sidebar")
}
