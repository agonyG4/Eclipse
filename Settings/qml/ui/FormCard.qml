import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    default property alias contentData: content.data
    property int contentMargins: 16
    property int contentSpacing: 0

    implicitHeight: content.implicitHeight + contentMargins * 2
    radius: Theme.cardRadius
    color: Theme.cardBackground
    border.width: 1
    border.color: Theme.cardBorder

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: root.contentMargins
        spacing: root.contentSpacing
    }
}
