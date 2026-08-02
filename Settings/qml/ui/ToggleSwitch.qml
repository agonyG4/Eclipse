import QtQuick

Item {
    id: root

    property bool checked: false
    property bool itemEnabled: true
    property string accessibleName: qsTr("Toggle")
    signal toggled(bool checked)

    implicitWidth: 42
    implicitHeight: 24
    activeFocusOnTab: itemEnabled
    opacity: itemEnabled ? 1 : Theme.opacityDisabled

    function toggle() {
        if (!itemEnabled)
            return
        checked = !checked
        toggled(checked)
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.surfacePressed
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.focusRing

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    Rectangle {
        width: 18
        height: 18
        radius: 9
        y: 3
        x: root.checked ? root.width - width - 3 : 3
        color: root.checked ? Theme.accentForeground : Theme.textPrimary

        Behavior on x {
            NumberAnimation {
                duration: Theme.animationFast
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.itemEnabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggle()
    }

    Keys.onSpacePressed: root.toggle()
    Keys.onEnterPressed: root.toggle()
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.accessibleName
    Accessible.checked: root.checked
}
