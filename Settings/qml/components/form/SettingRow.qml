import QtQuick
import QtQuick.Layouts
import ".." as Components
import "../controls" as Controls

Item {
    id: sr
    property string label: ""
    property string sublabel: ""
    property int sublabelTextFormat: Text.AutoText
    property int labelTextFormat: Text.AutoText
    property bool   isLast:  false
    property bool   clickable: false
    property bool   controlBlocksRowClick: true

    // Theme colors
    property color textPrimary: Components.Theme.textPrimary
    property color textSecondary: Components.Theme.textSecondary
    property color cardBorder: Components.Theme.cardBorder
    property color rowHoverBg: Components.Theme.isLight
        ? Qt.rgba(0, 0, 0, 0.045) : Qt.rgba(1, 1, 1, 0.055)

    default property alias control: slot.data

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    activeFocusOnTab: sr.clickable
    Accessible.role: sr.clickable ? Accessible.Button : Accessible.StaticText
    Accessible.name: sr.label
    Accessible.description: sr.sublabel
    Keys.onReturnPressed: (event) => {
        if (sr.clickable) {
            sr.clicked()
            event.accepted = true
        }
    }
    Keys.onSpacePressed: (event) => {
        if (sr.clickable) {
            sr.clicked()
            event.accepted = true
        }
    }

    implicitWidth: parent ? parent.width : 200
    implicitHeight: Math.max(sr.sublabel !== "" ? 64 : 52, rowLayout.implicitHeight + Components.Theme.spacingMedium * 2)

    // Interactive subtle hover background
    Rectangle {
        id: bgHighlight
        anchors { fill: parent; leftMargin: Components.Theme.spacingMicro; rightMargin: Components.Theme.spacingMicro; topMargin: Components.Theme.spacingTiny; bottomMargin: Components.Theme.spacingTiny }
        radius: Components.Theme.controlRadius
        color: rowArea.containsMouse && sr.clickable ? sr.rowHoverBg : "transparent"
        Behavior on color { ColorAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic } }

        Controls.PressTint {
            anchors.fill: parent
            radius: parent.radius
            pressed: rowArea.pressed && sr.clickable
        }
    }

    RowLayout {
        id: rowLayout
        anchors { fill: parent; leftMargin: Components.Theme.spacingXLarge; rightMargin: Components.Theme.spacingXLarge }
        spacing: Components.Theme.spacingLarge

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Components.Theme.spacingMicro

            Text {
                text: sr.label;
                textFormat: sr.labelTextFormat
                color: sr.textPrimary;
                font.family: Components.Theme.typography.role("label").family
                font.pixelSize: Components.Theme.typography.role("label").pixelSize;
                font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
                lineHeight: Components.Theme.typography.role("label").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.weight: Components.Theme.typography.role("label").weight
                Layout.fillWidth: true
                elide: Text.ElideRight
                // Subtle scale or translation could theoretically be added here
            }
            Text {
                visible: sr.sublabel !== "";
                text: sr.sublabel
                textFormat: sr.sublabelTextFormat
                color: sr.textSecondary;
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize;
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.weight: Components.Theme.typography.role("caption").weight
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: slot
            implicitWidth:  children.length > 0 ? Math.max(children[0].implicitWidth, children[0].width)  : 0
            implicitHeight: children.length > 0 ? Math.max(children[0].implicitHeight, children[0].height) : 0
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            Layout.maximumWidth: Math.max(0, sr.width - rowLayout.anchors.leftMargin - rowLayout.anchors.rightMargin)
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
        }
    }

    Rectangle {
        visible: !sr.isLast
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: Components.Theme.spacingXLarge }
        height: 1;
        color: sr.cardBorder
    }

    signal rightClicked(real x, real y)
    signal clicked()

    MouseArea {
        id: rowArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        propagateComposedEvents: true
        cursorShape: sr.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        function isOverControl(mouse) {
            if (!sr.controlBlocksRowClick)
                return false
            const p = mapToItem(slot, mouse.x, mouse.y)
            return p.x >= 0 && p.x <= slot.width && p.y >= 0 && p.y <= slot.height
        }
        onPressed: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                mouse.accepted = true
            } else if (rowArea.isOverControl(mouse)) {
                mouse.accepted = false
            } else if (sr.clickable) {
                mouse.accepted = true
            } else {
                mouse.accepted = false
            }
        }
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                sr.rightClicked(mouse.x, mouse.y)
            } else if (rowArea.isOverControl(mouse)) {
                mouse.accepted = false
            } else if (sr.clickable) {
                sr.clicked()
            } else {
                mouse.accepted = false
            }
        }
        onReleased: (mouse) => {
            if (mouse.button !== Qt.RightButton && (!sr.clickable || rowArea.isOverControl(mouse))) {
                mouse.accepted = false
            }
        }
    }
}
