import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property bool clearOnEscape: true
    property bool showClearButton: true

    signal textEdited(string text)
    signal accepted(string text)
    signal cleared()

    implicitWidth: 260
    implicitHeight: 38

    function focusField(selectAll) {
        field.forceActiveFocus()
        if (selectAll)
            field.selectAll()
    }

    function clearSearch(restoreFocus) {
        if (field.text.length === 0)
            return
        field.clear()
        if (restoreFocus)
            field.forceActiveFocus()
        root.textEdited("")
        root.cleared()
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.controlRadius
        color: field.activeFocus
            ? Theme.surfaceHover
            : hoverHandler.hovered ? Theme.surfaceHover : Theme.surface
        border.width: field.activeFocus ? 1 : 0
        border.color: Theme.focusRing

        Behavior on color {
            ColorAnimation { duration: Theme.animationFast }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 8

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: "⌕"
            color: field.activeFocus ? Theme.accent : Theme.textTertiary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.weight: Theme.fontWeightDemiBold
        }

        TextField {
            id: field

            Layout.fillWidth: true
            Layout.fillHeight: true
            selectByMouse: true
            color: Theme.textPrimary
            placeholderTextColor: Theme.textTertiary
            selectionColor: Theme.accent
            selectedTextColor: Theme.accentForeground
            background: null
            leftPadding: 0
            rightPadding: 0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeNormal
            font.weight: Theme.fontWeightMedium
            verticalAlignment: TextInput.AlignVCenter

            onTextEdited: root.textEdited(text)
            onAccepted: root.accepted(text)

            Keys.onEscapePressed: event => {
                if (root.clearOnEscape)
                    root.clearSearch(true)
                event.accepted = true
            }
        }

        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            visible: root.showClearButton && field.text.length > 0
            activeFocusOnTab: visible

            Rectangle {
                anchors.fill: parent
                radius: 12
                color: clearMouse.pressed
                    ? Theme.surfacePressed
                    : clearMouse.containsMouse ? Theme.surfaceHover : "transparent"
                border.width: parent.activeFocus ? 1 : 0
                border.color: Theme.focusRing
            }

            Text {
                anchors.centerIn: parent
                text: "×"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 15
            }

            MouseArea {
                id: clearMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.clearSearch(true)
            }

            Keys.onSpacePressed: root.clearSearch(true)
            Keys.onEnterPressed: root.clearSearch(true)
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Clear search")
        }
    }

    HoverHandler {
        id: hoverHandler
    }
}
