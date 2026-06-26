import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    readonly property color placeholderTextColor: "#66FFFFFF"
    readonly property bool hasQuery: searchInput.text.length > 0
    readonly property string fontFamily: SpotlightController ? SpotlightController.fontFamily : "SF Pro Display"

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.top: parent.top
    anchors.topMargin: parent.height * 0.25

    width: 600
    height: hasQuery ? Math.min(contentColumn.implicitHeight + 28, 450) : 58
    radius: 24
    color: "#80343434"
    border.color: "#33FFFFFF"
    border.width: 1
    clip: true

    scale: SpotlightController.open ? 1.0 : 0.98
    opacity: SpotlightController.open ? 1.0 : 0.0

    Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: 120 } }

    function focusSearch() {
        searchInput.forceActiveFocus()
        searchInput.selectAll()
    }

    ColumnLayout {
        id: contentColumn
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: 14
        }
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            spacing: 12

            Text {
                text: "\u2315"
                font.family: root.fontFamily
                font.pixelSize: 24
                color: "#99FFFFFF"
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignVCenter
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Layout.alignment: Qt.AlignVCenter

                Text {
                    anchors.fill: parent
                    text: (AstreaI18n && AstreaI18n.messages && AstreaI18n.tr("spotlight.placeholder", "Spotlight Search")) || "Spotlight Search"
                    font.family: root.fontFamily
                    font.pixelSize: 22
                    font.weight: Font.Light
                    color: root.placeholderTextColor
                    verticalAlignment: Text.AlignVCenter
                    visible: searchInput.text.length === 0
                    renderType: Text.NativeRendering
                }

                TextInput {
                    id: searchInput
                    anchors.fill: parent
                    font.family: root.fontFamily
                    font.pixelSize: 22
                    font.weight: Font.Light
                    color: "white"
                    selectionColor: "#407AFF"
                    selectedTextColor: "white"
                    clip: true
                    verticalAlignment: TextInput.AlignVCenter

                    onTextChanged: {
                        if (text !== SpotlightController.query)
                            SpotlightController.scheduleSearch(text)
                    }

                    Keys.onEscapePressed: SpotlightController.close()
                    Keys.onReturnPressed: resultsList.launchCurrent()
                    Keys.onDownPressed: resultsList.move(1)
                    Keys.onUpPressed: resultsList.move(-1)

                    Component.onCompleted: {
                        forceActiveFocus()
                        if (SpotlightController.query !== text)
                            text = SpotlightController.query
                    }

                    Connections {
                        target: SpotlightController
                        function onQueryChanged() {
                            if (searchInput.text !== SpotlightController.query)
                                searchInput.text = SpotlightController.query
                        }
                    }

                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus()
                            if (SpotlightController.query !== text)
                                text = SpotlightController.query
                        }
                    }
                }
            }

            SpotlightWeatherChip {
                id: weatherChip
                controller: SpotlightController
                textColor: root.placeholderTextColor
                fontFamily: root.fontFamily
            }
        }

        SpotlightResultsList {
            id: resultsList
            queryText: searchInput.text
            fontFamily: root.fontFamily
            onLaunchRequested: function(index) {
                SpotlightController.launch(index)
            }
        }
    }
}
