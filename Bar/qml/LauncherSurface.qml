import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var barController: null
    property var popupController: null
    property var workspaceModel: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int leftMargin: barGeometry ? barGeometry.launcherLeftMargin : 8
    property string logoSource: "qrc:/qt/qml/Astrea/Shell/Bar/assets/astrea.png"
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: launcherPill.width
    height: barGeometry ? barGeometry.pillHeight : 36

    BarSegment {
        id: launcherPill
        objectName: "launcherPill"
        interactive: true
        horizontalPadding: barGeometry ? barGeometry.sidePadding : 10
        onClicked: popupController && popupController.toggleAstreaMenu(
            barGeometry ? barGeometry.launcherAnchorX(width) : 0)

        Image {
            objectName: "logoImage"
            source: window.logoSource
            sourceSize.width: 20
            sourceSize.height: 20
            width: 20
            height: 20
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        WorkspaceStrip {
            workspaceModel: window.workspaceModel
        }
    }
}
