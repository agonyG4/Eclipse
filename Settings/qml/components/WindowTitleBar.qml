import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property bool collapsed: false
    property bool maximized: false

    signal collapseRequested()
    signal moveRequested()
    signal minimizeRequested()
    signal maximizeRestoreRequested()
    signal closeRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.dark ? "#0CFFFFFF" : "#16FFFFFF"
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: root.moveRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        spacing: 8

        SidebarCollapseButton {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            collapsed: root.collapsed
            onClicked: root.collapseRequested()
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Settings")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.weight: Theme.fontWeightDemiBold
            verticalAlignment: Text.AlignVCenter
        }

        RowLayout {
            Layout.rightMargin: 8
            spacing: 4

            TitleBarButton {
                symbol: "−"
                accessibleName: qsTr("Minimize")
                onClicked: root.minimizeRequested()
            }

            TitleBarButton {
                symbol: root.maximized ? "❐" : "□"
                accessibleName: root.maximized ? qsTr("Restore") : qsTr("Maximize")
                onClicked: root.maximizeRestoreRequested()
            }

            TitleBarButton {
                symbol: "×"
                danger: true
                accessibleName: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }
    }

    component TitleBarButton: Item {
        id: button

        property string symbol: ""
        property string accessibleName: ""
        property bool danger: false
        signal clicked()

        implicitWidth: 34
        implicitHeight: 30
        activeFocusOnTab: true

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: buttonMouse.pressed
                ? (button.danger ? Theme.danger : Theme.surfacePressed)
                : buttonMouse.containsMouse
                    ? (button.danger ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.72) : Theme.surfaceHover)
                    : "transparent"
            border.width: button.activeFocus ? 1 : 0
            border.color: Theme.focusRing

            Behavior on color {
                ColorAnimation { duration: Theme.animationQuick }
            }
        }

        Text {
            anchors.centerIn: parent
            text: button.symbol
            color: buttonMouse.containsMouse && button.danger ? "white" : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.weight: Theme.fontWeightMedium
        }

        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }

        Keys.onSpacePressed: button.clicked()
        Keys.onEnterPressed: button.clicked()
        Accessible.role: Accessible.Button
        Accessible.name: button.accessibleName
    }
}
