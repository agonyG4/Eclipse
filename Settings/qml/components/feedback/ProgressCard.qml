import QtQuick
import QtQuick.Layouts
import ".." as Components

Rectangle {
    id: card

    property string title: ""
    property string detail: ""
    property string destination: ""
    property real progress: 0
    property int percent: Math.round(Math.max(0, Math.min(1, progress)) * 100)
    property int completedItems: 0
    property int totalItems: 0
    property string remainingText: ""
    property bool indeterminate: totalItems <= 0 && percent <= 0 && !failed
    property bool failed: false
    property color panelColor: "#1e1e20"
    property color borderColor: "#3a3a3c"
    property color primaryTextColor: "#f2f2f7"
    property color secondaryTextColor: "#8e8e93"
    property color trackColor: "#2c2c2e"
    property color fillColor: "#f2f2f7"
    property color errorColor: "#ff8b8b"

    width: 320
    height: Math.max(82, progressColumn.implicitHeight + 24)
    radius: 8
    color: panelColor
    border.width: 1
    border.color: borderColor

    Column {
        id: progressColumn
        anchors { fill: parent; margins: 12 }
        spacing: 7

        RowLayout {
            width: parent.width
            height: Math.max(16, Math.ceil(Components.Theme.typography.role("label").pixelSize * Components.Theme.typography.role("label").lineHeight))
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: card.title
                color: card.failed ? card.errorColor : card.primaryTextColor
                font { family: Components.Theme.typography.role("label").family; pixelSize: Components.Theme.typography.role("label").pixelSize; weight: Components.Theme.typography.role("label").weight }
                elide: Text.ElideRight
            }

            Text {
                text: card.indeterminate ? "" : card.percent + "%"
                color: card.secondaryTextColor
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                font.family: Components.Theme.typography.role("caption").family
                visible: !card.failed
            }
        }

        Text {
            width: parent.width
            height: implicitHeight
            text: card.destination !== "" ? card.detail + " -> " + card.destination : card.detail
            color: card.secondaryTextColor
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            font.family: Components.Theme.typography.role("caption").family
            elide: Text.ElideMiddle
            visible: text !== ""
        }

        RowLayout {
            width: parent.width
            height: Math.max(16, Math.ceil(Components.Theme.typography.role("label").pixelSize * Components.Theme.typography.role("label").lineHeight))
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                height: 5
                radius: 3
                color: card.trackColor
                clip: true

                Rectangle {
                    id: progressFill

                    anchors { top: parent.top; bottom: parent.bottom }
                    x: card.indeterminate ? -width : 0
                    width: card.indeterminate ? Math.max(42, parent.width * 0.34) : parent.width * Math.max(0, Math.min(1, card.progress))
                    radius: 3
                    color: card.failed ? card.errorColor : card.fillColor

                    SequentialAnimation on x {
                        running: card.visible && card.indeterminate
                        loops: Animation.Infinite
                        NumberAnimation { from: -progressFill.width; to: progressFill.parent.width; duration: 1100; easing.type: Easing.InOutQuad }
                    }

                    Behavior on width {
                        enabled: !card.indeterminate
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }
                }
            }

            Text {
                Layout.preferredWidth: 96
                text: card.failed
                    ? I18n.tr("core.components.feedback.progress_card.status.failed", "Failed")
                    : (card.remainingText !== "" ? card.remainingText
                       : (card.totalItems > 0 ? (card.completedItems + " / " + card.totalItems)
                          : (card.completedItems > 0
                             ? (card.completedItems + " " + I18n.tr("core.components.feedback.progress_card.status.items", "items"))
                             : I18n.tr("core.components.feedback.progress_card.status.preparing", "Preparing"))))
                color: card.secondaryTextColor
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                font.family: Components.Theme.typography.role("caption").family
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
