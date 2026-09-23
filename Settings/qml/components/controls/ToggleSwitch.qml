import QtQuick
import ".." as Components

Item {
    id: toggle
    implicitWidth: 48
    implicitHeight: 32
    width: implicitWidth
    height: implicitHeight
    property bool checked: false
    property string accessibleName: ""
    signal toggled(bool targetChecked)

    focus: enabled
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName

    Keys.onSpacePressed: function(event) {
        event.accepted = true
        if (toggle.enabled)
            toggle.toggled(!toggle.checked)
    }
    Keys.onReturnPressed: function(event) {
        event.accepted = true
        if (toggle.enabled)
            toggle.toggled(!toggle.checked)
    }
    Keys.onEnterPressed: function(event) {
        event.accepted = true
        if (toggle.enabled)
            toggle.toggled(!toggle.checked)
    }

    opacity: enabled ? 1.0 : 0.55
    Behavior on opacity { NumberAnimation { duration: Components.Theme.animationMicro } }

    Rectangle {
        id: track
        width: 36
        height: 20
        radius: Components.Theme.controlRadius
        anchors.centerIn: parent
        color: toggle.checked ? Components.Theme.accent : Qt.rgba(1, 1, 1, 0.18)
        Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: knobShadow
        width: knob.width
        height: knob.height
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.22)
        anchors.verticalCenter: track.verticalCenter
        anchors.verticalCenterOffset: 1
        x: knob.x
        opacity: toggle.enabled ? 1.0 : 0.0
        Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Components.Theme.animationMicro } }
    }

    Rectangle {
        id: knob
        width: 14
        height: 14
        radius: height / 2
        color: "#ffffff"
        anchors.verticalCenter: track.verticalCenter
        x: track.x + (toggle.checked ? track.width - width - 3 : 3)
        Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        anchors.fill: parent
        enabled: toggle.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            toggle.toggled(!toggle.checked)
        }
    }
}
