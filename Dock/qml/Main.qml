import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    title: qsTr("Astrea Dock")
    // Output geometry is supplied by the C++ layer-shell surface. The Dock
    // window is intentionally content-sized; these properties describe the
    // output-local coordinate space used by its context-menu anchors.
    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: panel.width
    height: panel.height

    DockPanel {
        id: panel
        objectName: "dockPanel"
        anchors.centerIn: parent
        contextMenuController: ContextMenuController
        dockSurfaceGeometry: DockSurfaceGeometry
        inputRegionBridge: DockInputRegion
        outputKey: window.outputKey
        outputWidth: window.outputWidth
        outputHeight: window.outputHeight
        onReorderRequested: function(desktopFileName, targetPinIndex) {
            DockController.movePinned(desktopFileName, targetPinIndex)
        }
    }
}
