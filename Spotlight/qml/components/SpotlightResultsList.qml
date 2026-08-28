import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string queryText: ""
    property string fontFamily: "SF Pro Display"
    readonly property int resultRowHeight: 50
    readonly property int resultIconSize: 40
    readonly property int count: resultList.count

    signal launchRequested(int index)

    Layout.fillWidth: true
    visible: queryText.length > 0 && resultList.count > 0
    spacing: 0

    function move(delta) {
        if (resultList.count <= 0)
            return
        var next = (resultList.currentIndex + delta + resultList.count) % resultList.count
        SpotlightController.selectedIndex = next
    }

    function launchCurrent() {
        if (resultList.count > 0)
            root.launchRequested(SpotlightController.selectedIndex)
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#15FFFFFF"
        Layout.topMargin: 12
        Layout.bottomMargin: 8
    }

    ListView {
        id: resultList

        Layout.fillWidth: true
        implicitHeight: Math.min(count, 6) * root.resultRowHeight
        model: SpotlightController.resultsModel
        currentIndex: SpotlightController.selectedIndex
        interactive: false
        clip: true

        delegate: Rectangle {
            id: resultDelegate

            required property int index
            required property string name
            required property string iconName
            required property string entryId
            required property string startupWmClass

            width: resultList.width
            height: root.resultRowHeight
            radius: 7
            color: resultList.currentIndex === resultDelegate.index ? "#007AFF" : "transparent"

            RowLayout {
                anchors {
                    fill: parent
                    leftMargin: 12
                    rightMargin: 12
                }
                spacing: 15

                AstreaAppIcon {
                    Layout.preferredWidth: root.resultIconSize
                    Layout.preferredHeight: root.resultIconSize
                    astreaIconName: resultDelegate.iconName
                    appName: resultDelegate.name
                    className: resultDelegate.startupWmClass
                    title: resultDelegate.name
                    iconRadius: 7
                    maximumPresentationLogicalSize: root.resultIconSize
                }

                Text {
                    text: resultDelegate.name
                    font.family: root.fontFamily
                    font.pixelSize: 17
                    color: "white"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: SpotlightController.selectedIndex = resultDelegate.index
                onClicked: root.launchRequested(resultDelegate.index)
            }
        }
    }
}
