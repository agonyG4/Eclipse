import QtQuick

Rectangle {
    id: root

    property url source
    property string fallbackText: "?"

    radius: width / 2
    color: Theme.surfaceHover
    border.width: 1
    border.color: Theme.cardBorder
    clip: true

    Image {
        id: avatarImage
        anchors.fill: parent
        source: root.source
        sourceSize.width: 96
        sourceSize.height: 96
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        visible: status === Image.Ready
    }

    Text {
        anchors.centerIn: parent
        visible: !avatarImage.visible
        text: root.fallbackText
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeAvatar
        font.weight: Theme.fontWeightMedium
    }
}
