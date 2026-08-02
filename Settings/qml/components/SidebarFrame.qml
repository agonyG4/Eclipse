import QtQuick

Item {
    id: root

    default property alias contentData: content.data
    property real topMargin: 10
    property real bottomMargin: 10
    property real leftMargin: 10
    property real rightMargin: 8
    property real cornerRadius: Theme.radiusPanel
    property color backgroundColor: Theme.sidebarBackground
    property color gradientStart: Theme.sidebarGradientStart
    property color gradientEnd: Theme.sidebarGradientEnd
    property color washColor: Theme.sidebarWash
    property color borderColor: Theme.sidebarBorder
    property real contentTopPadding: 16
    property real contentBottomPadding: 16

    Rectangle {
        id: card

        anchors.fill: parent
        anchors.topMargin: root.topMargin
        anchors.bottomMargin: root.bottomMargin
        anchors.leftMargin: root.leftMargin
        anchors.rightMargin: root.rightMargin
        radius: root.cornerRadius
        color: "#4B5059"
        clip: true

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: root.gradientStart }
            GradientStop { position: 0.52; color: root.backgroundColor }
            GradientStop { position: 1.0; color: root.gradientEnd }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.washColor
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.radius - 1)
            color: "transparent"
            border.width: 1
            border.color: root.borderColor
        }

        Item {
            id: content

            anchors.fill: parent
            anchors.topMargin: root.contentTopPadding
            anchors.bottomMargin: root.contentBottomPadding
        }
    }
}
