import QtQuick
import QtQuick.Layouts
import ".." as Components
import "../typography" as Typography

// The title block every settings page opens with: a large title, an optional
// one-line description, and an optional trailing control (a switch or action).
Item {
    id: header
    property string title: ""
    property string subtitle: ""
    default property alias trailing: trailingSlot.data

    Layout.fillWidth: true
    Layout.bottomMargin: Components.Theme.spacingXLarge
    implicitHeight: row.implicitHeight

    RowLayout {
        id: row
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Components.Theme.spacingLarge

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Components.Theme.spacingMicro

            Typography.DisplayLabel {
                Layout.fillWidth: true
                text: header.title
                font.pixelSize: Components.Theme.fontSizeHero
                font.weight: Components.Theme.fontWeightDemiBold
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Typography.TextLabel {
                Layout.fillWidth: true
                visible: header.subtitle !== ""
                text: header.subtitle
                textColor: Components.Theme.textSecondary
                font.pixelSize: Components.Theme.fontSizeNormal
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: trailingSlot
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            implicitWidth: children.length > 0 ? children[0].implicitWidth : 0
            implicitHeight: children.length > 0 ? children[0].implicitHeight : 0
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
        }
    }
}
