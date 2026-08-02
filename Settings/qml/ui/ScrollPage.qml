import QtQuick
import QtQuick.Controls

ScrollView {
    id: root

    default property alias contentData: pageColumn.data
    property int pageMargins: Theme.pageMargin
    property int contentSpacing: Theme.spacingLarge

    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    contentWidth: availableWidth

    Column {
        id: pageColumn
        width: Math.max(0, root.availableWidth - root.pageMargins * 2)
        x: root.pageMargins
        topPadding: root.pageMargins
        bottomPadding: root.pageMargins
        spacing: root.contentSpacing
    }
}
