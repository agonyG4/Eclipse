import QtQuick

Image {
    id: icon

    property int iconSize: 16
    width: iconSize
    height: iconSize
    sourceSize: Qt.size(Math.max(1, width), Math.max(1, height))
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: false
}
