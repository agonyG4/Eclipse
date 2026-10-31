import QtQuick
import QtQuick.Layouts
import ".." as Components

Rectangle {
    id: cardRoot
    Layout.fillWidth: true
    radius: Components.Theme.cardRadius
    color: Components.Theme.cardBg
    border.width: 1
    border.color: Components.Theme.cardBorder
    // Margins inset the content on every side, top and bottom included.
    implicitHeight: Math.max(48, contentCol.implicitHeight + margins * 2)

    default property alias content: contentCol.data
    property real spacing: 0
    property real margins: 0

    ColumnLayout {
        id: contentCol
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: cardRoot.margins
        spacing: cardRoot.spacing
    }
}
