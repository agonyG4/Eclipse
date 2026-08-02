import QtQuick
import QtQuick.Layouts

SidebarFrame {
    id: root

    required property var controller
    property bool collapsed: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ProfileHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            userName: root.controller.userName
            avatarUrl: root.controller.avatarUrl
            collapsed: root.collapsed
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.preferredHeight: 1
            color: Theme.separator
        }

        SearchField {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 12
            Layout.bottomMargin: 8
            visible: !root.collapsed
            placeholderText: qsTr("Search settings")
            text: root.controller.filterText
            onTextEdited: text => root.controller.setFilterText(text)
            onCleared: root.controller.clearFilter()
        }

        ListView {
            id: navigationList

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: root.collapsed ? 10 : 2
            Layout.bottomMargin: 12
            clip: true
            spacing: 2
            model: root.controller.navigationModel
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: delegateRoot

                required property string entryId
                required property string title
                required property string subtitle
                required property string iconName
                required property string kind
                required property bool entryEnabled
                required property bool selected

                width: navigationList.width
                height: kind === "spacer" ? 12 : 42

                SidebarItem {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    visible: delegateRoot.kind === "item"
                    title: delegateRoot.title
                    subtitle: delegateRoot.subtitle
                    iconName: delegateRoot.iconName
                    compact: root.collapsed
                    selected: delegateRoot.selected
                    itemEnabled: delegateRoot.entryEnabled
                    onClicked: root.controller.selectSection(delegateRoot.entryId)
                }
            }
        }
    }
}
