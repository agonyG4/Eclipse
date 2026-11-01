import QtQuick
import QtQuick.Layouts
import "." as Components
import "controls" as Controls
Rectangle {
    id: root
    property bool busy: false
    property string error: ""
    property string message: ""
    signal retry()
    visible: busy || error.length > 0 || message.length > 0
    Layout.fillWidth: true
    Layout.bottomMargin: visible ? 18 : 0
    implicitHeight: visible ? Math.max(40, noticeText.implicitHeight + 20) : 0
    radius: Components.Theme.controlRadius
    color: Components.Theme.isLight ? Qt.rgba(0,0,0,0.025) : Qt.rgba(1,1,1,0.035)
    border.width: 1
    border.color: Components.Theme.cardBorder
    Text {
        id: noticeText
        anchors.left: parent.left; anchors.right: retryButton.left
        anchors.leftMargin: 12; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
        text: root.busy ? "Reading or applying settings…" : root.error || root.message
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
    }
    Controls.ButtonCapsule {
        id: retryButton
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
        visible: !root.busy && root.error.length > 0
        label: "Retry"
        onClicked: root.retry()
    }
}
