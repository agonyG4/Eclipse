import QtQuick
import QtQuick.Window

Window {
    id: window

    property int outputWidth: 1
    property int outputHeight: 1
    property var barGeometry: null
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowTransparentForInput
    width: outputWidth
    height: barGeometry ? barGeometry.barHeight : 45
}
