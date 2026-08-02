import QtQuick

Item {
    id: root

    property string text: ""
    property bool itemEnabled: true
    property bool emphasized: true
    signal clicked()

    implicitWidth: Math.max(92, label.implicitWidth + 28)
    implicitHeight: 36
    activeFocusOnTab: itemEnabled
    opacity: itemEnabled ? 1 : Theme.opacityDisabled

    Rectangle {
        anchors.fill: parent
        radius: Theme.controlRadius
        color: root.emphasized
            ? (pressArea.pressed
                ? Qt.darker(Theme.accent, 1.12)
                : hoverHandler.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
            : (pressArea.pressed
                ? Theme.surfacePressed
                : hoverHandler.hovered ? Theme.surfaceHover : Theme.surface)
        border.width: root.activeFocus || !root.emphasized ? 1 : 0
        border.color: root.activeFocus ? Theme.focusRing : Theme.cardBorder

        Behavior on color {
            ColorAnimation { duration: Theme.animationQuick }
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.emphasized ? Theme.accentForeground : Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeNormal
        font.weight: Theme.fontWeightDemiBold
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.itemEnabled
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        enabled: root.itemEnabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Keys.onSpacePressed: if (root.itemEnabled) root.clicked()
    Keys.onEnterPressed: if (root.itemEnabled) root.clicked()
    Accessible.role: Accessible.Button
    Accessible.name: root.text
}
