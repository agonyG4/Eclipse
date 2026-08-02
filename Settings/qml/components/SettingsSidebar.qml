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
            Layout.preferredHeight: 76
            userName: root.controller.userName
            avatarUrl: root.controller.avatarUrl
            collapsed: root.collapsed
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.preferredHeight: 1
            color: Theme.sidebarBorder
        }

        SearchField {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 14
            Layout.bottomMargin: 10
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
            Layout.bottomMargin: 14
            clip: true
            spacing: 4
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
                height: kind === "spacer" ? 28 : 40

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    height: 1
                    color: Theme.separator
                    visible: delegateRoot.kind === "spacer"
                }

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
